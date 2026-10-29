#include "FileOps.h"

#include "FileEntry.h"
#include "Terminal.h"
#include "Trash.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPointer>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QUrl>
#include <QtConcurrent>

#include <sys/stat.h>

namespace {
const QString kGnomeCopied = QStringLiteral("x-special/gnome-copied-files");
const QString kKdeCut = QStringLiteral("application/x-kde-cutselection");

// «отчёт.pdf» → «отчёт (2).pdf»; у папок расширения нет
QString uniquePath(const QString &dir, const QString &name, bool isDir)
{
    QString target = dir + QLatin1Char('/') + name;
    if (!QFileInfo::exists(target) && !QFileInfo(target).isSymLink())
        return target;
    QString stem = name, ext;
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (!isDir && dot > 0) {
        stem = name.left(dot);
        ext = name.mid(dot);
    }
    for (int i = 2;; ++i) {
        target = QStringLiteral("%1/%2 (%3)%4").arg(dir, stem).arg(i).arg(ext);
        if (!QFileInfo::exists(target) && !QFileInfo(target).isSymLink())
            return target;
    }
}

qint64 treeSize(const QString &path, const std::atomic_bool &cancel)
{
    const QFileInfo fi(path);
    if (fi.isSymLink() || !fi.isDir())
        return fi.isSymLink() ? 0 : fi.size();
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext() && !cancel.load())
        total += it.nextFileInfo().size();
    return total;
}

// ── Архивы: внешней программой ────────────────────────────────────
// Сколько архива path уже прочитал процесс pid — позиция его дескриптора в /proc
qint64 readOffset(qint64 pid, const QString &path)
{
    const QString fdDir = QStringLiteral("/proc/%1/fd").arg(pid);
    for (const QString &fd : QDir(fdDir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System)) {
        if (QFile::symLinkTarget(fdDir + QLatin1Char('/') + fd) != path)
            continue;
        QFile info(QStringLiteral("/proc/%1/fdinfo/%2").arg(pid).arg(fd));
        if (!info.open(QIODevice::ReadOnly))
            return -1;
        for (const QByteArray &line : info.readAll().split('\n'))
            if (line.startsWith("pos:"))
                return line.mid(4).trimmed().toLongLong();
    }
    return -1;
}

// Запустить программу и ждать; tick — раз в 100 мс, пока работает (накопленный вывод).
// false — отменили (процесс убит) или ошибка (текст в error)
bool runTool(const QString &program, const QStringList &args, const QString &workDir, const std::atomic_bool &cancel,
             const std::function<void(qint64 pid, const QByteArray &out)> &tick, QString &error)
{
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.setWorkingDirectory(workDir);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();
    if (!p.waitForStarted()) {
        error = QCoreApplication::translate("FileOps", "Не удалось запустить %1").arg(QFileInfo(program).fileName());
        return false;
    }
    // Пароль и прочие вопросы задать некому — пусть программа сразу завершится с ошибкой
    p.closeWriteChannel();
    QByteArray out;
    while (!p.waitForFinished(100)) {
        if (cancel.load()) {
            p.kill();
            p.waitForFinished();
            return false;
        }
        out += p.readAll();
        tick(p.processId(), out);
    }
    out += p.readAll();
    // У 7z код 1 — предупреждение, архив при этом готов
    const bool sevenZip = !program.endsWith(QLatin1String("bsdtar"));
    if (p.exitStatus() == QProcess::NormalExit && (p.exitCode() == 0 || (sevenZip && p.exitCode() == 1)))
        return true;
    QString last;
    for (const QByteArray &line : out.split('\n')) {
        const QString l = QString::fromUtf8(line).trimmed();
        if (!l.isEmpty())
            last = l;
        if (l.contains(QLatin1String("rror"))) {
            last = l;
            break;
        }
    }
    error = last.isEmpty() ? QCoreApplication::translate("FileOps", "%1 завершился с ошибкой").arg(QFileInfo(program).fileName()) : last;
    return false;
}

QString tarInside(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("\\.(tar\\.[a-z0-9]+|tgz|tbz2?|txz|tzst)$"), QRegularExpression::CaseInsensitiveOption);
    return re.match(name).hasMatch() ? name : QString();
}

struct CopyCtx {
    const std::atomic_bool &cancel;
    qint64 total = 0;
    qint64 done = 0;
    std::function<void(const QString &)> report;
    QString error;
};

bool copyFile(const QString &src, const QString &dst, CopyCtx &ctx)
{
    QFile in(src), out(dst);
    if (!in.open(QIODevice::ReadOnly)) {
        ctx.error = QCoreApplication::translate("FileOps", "Не удалось прочитать %1").arg(src);
        return false;
    }
    if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        ctx.error = QCoreApplication::translate("FileOps", "Не удалось создать %1").arg(dst);
        return false;
    }
    QByteArray buf(4 << 20, Qt::Uninitialized);
    const QString name = QFileInfo(src).fileName();
    while (!in.atEnd()) {
        if (ctx.cancel.load()) {
            out.close();
            out.remove(); // недокопированный файл не оставляем
            return false;
        }
        const qint64 n = in.read(buf.data(), buf.size());
        if (n < 0 || out.write(buf.constData(), n) != n) {
            ctx.error = QCoreApplication::translate("FileOps", "Ошибка записи %1").arg(dst);
            out.close();
            out.remove();
            return false;
        }
        ctx.done += n;
        ctx.report(name);
    }
    out.setPermissions(in.permissions());
    out.setFileTime(QFileInfo(src).lastModified(), QFileDevice::FileModificationTime);
    return true;
}

bool copyTree(const QString &src, const QString &dst, CopyCtx &ctx)
{
    const QFileInfo fi(src);
    if (fi.isSymLink())
        return QFile::link(fi.symLinkTarget(), dst) || (ctx.error = QCoreApplication::translate("FileOps", "Не удалось создать ссылку %1").arg(dst), false);
    if (!fi.isDir())
        return copyFile(src, dst, ctx);
    if (!QDir().mkpath(dst)) {
        ctx.error = QCoreApplication::translate("FileOps", "Не удалось создать папку %1").arg(dst);
        return false;
    }
    const QFileInfoList children =
        QDir(src).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &c : children) {
        if (ctx.cancel.load())
            return false;
        if (!copyTree(c.absoluteFilePath(), dst + QLatin1Char('/') + c.fileName(), ctx))
            return false;
    }
    QFile::setPermissions(dst, fi.permissions());
    return true;
}

// Из папки без права записи (кэш Go, распакованные архивы) файлы не удалить —
// перед удалением навсегда возвращаем владельцу запись на все папки
void makeWritable(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.isDir() || fi.isSymLink())
        return;
    QFile::setPermissions(path, fi.permissions() | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo d = it.nextFileInfo();
        if (!(d.permissions() & QFileDevice::WriteOwner) || !(d.permissions() & QFileDevice::ExeOwner))
            QFile::setPermissions(d.absoluteFilePath(), d.permissions() | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
}

bool removeTree(const QString &path)
{
    const QFileInfo fi(path);
    if (fi.isDir() && !fi.isSymLink())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}

bool present(const QString &path)
{
    return QFileInfo::exists(path) || QFileInfo(path).isSymLink();
}

bool realDir(const QFileInfo &fi)
{
    return fi.isDir() && !fi.isSymLink();
}

QString parentOf(const QString &path)
{
    return QFileInfo(path).absolutePath();
}

// Перемещение в пределах раздела — rename, копировать (и считать размер) не нужно
bool sameDevice(const QString &a, const QString &b)
{
    struct stat sa {}, sb {};
    return ::stat(QFile::encodeName(a).constData(), &sa) == 0 && ::stat(QFile::encodeName(b).constData(), &sb) == 0
        && sa.st_dev == sb.st_dev;
}

// «отчёт.pdf», «3 объекта»
QString objects(qsizetype n)
{
    return QCoreApplication::translate("FileOps", "%n объект(ов)", nullptr, int(n));
}
QString subject(const QStringList &paths)
{
    return paths.size() == 1 ? QStringLiteral("«%1»").arg(QFileInfo(paths.first()).fileName()) : objects(paths.size());
}

// Вынутое из корзины забывается: .trashinfo лежит в info/ рядом с files/ — и в домашней
// корзине, и в $topdir/.Trash-$uid другого диска
void forgetTrashed(const QString &trashPath)
{
    if (Trash::isTopLevel(trashPath)) {
        Trash::forget(trashPath);
        return;
    }
    const QFileInfo fi(trashPath);
    QFile::remove(QDir::cleanPath(fi.absolutePath() + QStringLiteral("/../info/") + fi.fileName() + QStringLiteral(".trashinfo")));
}

// Заменяемое не удаляем, а убираем в корзину — замену можно отменить
bool trashTarget(const QString &target, CopyCtx &ctx, QList<UndoStep> &steps)
{
    QString where;
    if (!QFile::moveToTrash(target, &where)) {
        ctx.error = QCoreApplication::translate("FileOps", "Не удалось убрать в корзину заменяемый %1").arg(target);
        return false;
    }
    steps.append({UndoStep::Trashed, target, where});
    return true;
}
// Замена сорвалась — возвращаем старый на место
void untrashTarget(const QString &target, QList<UndoStep> &steps)
{
    if (steps.isEmpty() || steps.last().kind != UndoStep::Trashed || steps.last().from != target || present(target))
        return;
    if (QDir().rename(steps.last().to, target)) {
        forgetTrashed(steps.last().to);
        steps.removeLast();
    }
}

// src → dst (dst свободен): перемещение — переименованием, если выйдет, иначе копией
// с удалением исходника. Сделанное — в steps
bool placeItem(const QString &src, const QString &dst, bool move, CopyCtx &ctx, QList<UndoStep> &steps)
{
    if (move && QDir().rename(src, dst)) {
        steps.append({UndoStep::Moved, src, dst});
        return true;
    }
    if (!copyTree(src, dst, ctx)) {
        removeTree(dst); // половинчатую копию не оставляем
        return false;
    }
    if (move && !removeTree(src)) {
        steps.append({UndoStep::Created, dst, src});
        ctx.error = QCoreApplication::translate("FileOps", "Скопировано, но не удалось удалить исходник %1").arg(src);
        return false;
    }
    steps.append(move ? UndoStep{UndoStep::Moved, src, dst} : UndoStep{UndoStep::Created, dst, src});
    return true;
}

// Папку в папку: недостающее добавляется, вложенные папки объединяются,
// файлы с теми же именами заменяются (старые — в корзину)
bool mergeTree(const QString &src, const QString &dst, bool move, CopyCtx &ctx, QList<UndoStep> &steps)
{
    const QFileInfoList children =
        QDir(src).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &c : children) {
        if (ctx.cancel.load())
            return false;
        const QString d = dst + QLatin1Char('/') + c.fileName();
        if (realDir(c) && realDir(QFileInfo(d))) {
            if (!mergeTree(c.absoluteFilePath(), d, move, ctx, steps))
                return false;
            continue;
        }
        const bool replacing = present(d);
        if (replacing && !trashTarget(d, ctx, steps))
            return false;
        if (!placeItem(c.absoluteFilePath(), d, move, ctx, steps)) {
            if (replacing)
                untrashTarget(d, steps);
            return false;
        }
    }
    if (move && QDir().rmdir(src))
        steps.append({UndoStep::RemovedDir, src, {}});
    return true;
}

// sources → dest с ответами на совпадения имён (resolutions: имя → replace | skip | keep).
// created — что появилось в dest, done — что из sources перенесено
void placeAll(const QStringList &sources, const QString &dest, bool move, const QVariantMap &resolutions,
              CopyCtx &ctx, QList<UndoStep> &steps, QStringList &created, QStringList &done)
{
    for (const QString &s : sources) {
        if (ctx.cancel.load())
            break;
        const QString src = QDir::cleanPath(s);
        const QFileInfo fi(src);
        if (move && fi.absolutePath() == dest)
            continue; // уже здесь
        QString target = dest + QLatin1Char('/') + fi.fileName();
        const QString how = present(target) && target != src && !src.startsWith(target + QLatin1Char('/'))
            ? resolutions.value(fi.fileName()).toString() : QString();
        if (how == QLatin1String("skip"))
            continue;
        if (how == QLatin1String("replace") && realDir(fi) && realDir(QFileInfo(target))) {
            if (!mergeTree(src, target, move, ctx, steps))
                break;
        } else if (how == QLatin1String("replace")) {
            if (!trashTarget(target, ctx, steps))
                break;
            if (!placeItem(src, target, move, ctx, steps)) {
                untrashTarget(target, steps);
                break;
            }
        } else {
            target = uniquePath(dest, fi.fileName(), realDir(fi));
            if (!placeItem(src, target, move, ctx, steps))
                break;
        }
        created << target;
        done << src;
    }
}

// Откат одного шага; restored — где оказалось возвращённое, inverse — шаги,
// которые откатят сам откат (для повтора), в порядке выполнения
bool undoStep(const UndoStep &st, CopyCtx &ctx, QStringList &restored, QList<UndoStep> &inverse)
{
    const auto putBack = [&](const QString &from, const QString &to, bool trashed) {
        if (!present(from))
            return false;
        QDir().mkpath(parentOf(to));
        const QFileInfo fi(from);
        const QString target = present(to) ? uniquePath(parentOf(to), QFileInfo(to).fileName(), realDir(fi)) : to;
        if (!QDir().rename(from, target)) {
            // Перемещали между дисками — возвращаем так же, копией
            if (trashed || !copyTree(from, target, ctx)) {
                removeTree(target);
                return false;
            }
            removeTree(from);
        }
        if (trashed)
            forgetTrashed(from);
        restored << target;
        inverse.append(trashed ? UndoStep{UndoStep::Restored, target, {}} : UndoStep{UndoStep::Moved, from, target});
        return true;
    };
    switch (st.kind) {
    case UndoStep::Created:
        // Копия — лишний дубль, в корзине ей делать нечего: повтор скопирует заново
        if (!present(st.from))
            return true;
        makeWritable(st.from);
        if (!removeTree(st.from))
            return false;
        if (!st.to.isEmpty())
            inverse.append({UndoStep::Uncopied, st.from, st.to});
        return true;
    case UndoStep::Moved:
        return putBack(st.to, st.from, false);
    case UndoStep::Trashed:
        return putBack(st.to, st.from, true);
    case UndoStep::MadeDir:
        // В новую папку уже что-то положили — не трогаем
        if (!present(st.from))
            return true;
        if (!QDir().rmdir(st.from))
            return false;
        inverse.append({UndoStep::RemovedDir, st.from, {}});
        return true;
    case UndoStep::RemovedDir:
        if (!QDir().mkpath(st.from))
            return false;
        inverse.append({UndoStep::MadeDir, st.from, {}});
        return true;
    case UndoStep::Uncopied: {
        if (!present(st.to))
            return false;
        const QFileInfo fi(st.to);
        const QString target = present(st.from) ? uniquePath(parentOf(st.from), QFileInfo(st.from).fileName(), realDir(fi)) : st.from;
        if (!copyTree(st.to, target, ctx)) {
            removeTree(target);
            return false;
        }
        restored << target;
        inverse.append({UndoStep::Created, target, st.to});
        return true;
    }
    case UndoStep::Restored: {
        QString where;
        if (!QFile::moveToTrash(st.from, &where))
            return false;
        inverse.append({UndoStep::Trashed, st.from, where});
        return true;
    }
    }
    return false;
}
}

FileOps::FileOps(QObject *parent)
    : QObject(parent)
{
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &FileOps::readClipboard);
    readClipboard();
}

FileOps::~FileOps()
{
    if (m_cancel)
        m_cancel->store(true);
    m_job.waitForFinished();
    // Закрыли окно, не ответив на вопрос о совпадениях, — распакованное не нужно
    for (auto it = m_extracting.cbegin(); it != m_extracting.cend(); ++it)
        removeTree(it.key());
}

void FileOps::openTerminal(const QString &dir)
{
    if (!Terminal::openIn(dir))
        emit failed(tr("Не найден терминал — укажите его в настройках"));
}

QString FileOps::trash(const QStringList &paths)
{
    QStringList failed, done;
    QList<UndoStep> steps;
    for (const QString &p : paths) {
        QString where;
        if (QFile::moveToTrash(p, &where)) {
            steps.append({UndoStep::Trashed, p, where});
            done << p;
        } else {
            failed << QFileInfo(p).fileName();
        }
    }
    if (!done.isEmpty())
        record(QStringLiteral("trash"), tr("удаление в корзину %1").arg(subject(done)), steps);
    return failed.isEmpty() ? QString() : tr("Не удалось убрать в корзину: %1").arg(failed.join(QStringLiteral(", ")));
}

QVariantMap FileOps::restore(const QStringList &paths)
{
    QStringList restored, failed;
    for (const QString &p : paths) {
        const QString name = QFileInfo(p).fileName();
        const Trash::Info info = Trash::isTopLevel(p) ? Trash::readInfo(p) : Trash::Info{};
        if (!info.valid()) {
            failed << name;
            continue;
        }
        const QFileInfo orig(info.origPath);
        // Папку, где лежал файл, могли удалить — создаём заново
        if (!QDir().mkpath(orig.absolutePath())) {
            failed << name;
            continue;
        }
        const QFileInfo fi(p);
        const QString target = uniquePath(orig.absolutePath(), orig.fileName(), fi.isDir() && !fi.isSymLink());
        if (!QDir().rename(p, target)) {
            failed << name;
            continue;
        }
        Trash::forget(p);
        restored << target;
    }
    QVariantMap res;
    res[QStringLiteral("paths")] = restored;
    if (!failed.isEmpty())
        res[QStringLiteral("error")] = tr("Не удалось восстановить: %1").arg(failed.join(QStringLiteral(", ")));
    return res;
}

void FileOps::deleteForever(const QStringList &paths)
{
    for (const QString &p : paths)
        if (!Trash::contains(p)) {
            emit failed(tr("Удалять навсегда можно только из корзины"));
            return;
        }
    removeInBackground(paths, tr("Удаление…"));
}

void FileOps::emptyTrash()
{
    QStringList paths;
    for (const QFileInfo &fi : QDir(Trash::filesDir()).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
        paths << fi.absoluteFilePath();
    // .trashinfo без файла (файл удалили руками) тоже убираем
    for (const QFileInfo &fi : QDir(Trash::root() + QStringLiteral("/info")).entryInfoList({QStringLiteral("*.trashinfo")}, QDir::Files | QDir::Hidden))
        if (!QFileInfo::exists(Trash::filesDir() + QLatin1Char('/') + fi.completeBaseName()))
            QFile::remove(fi.absoluteFilePath());
    if (paths.isEmpty())
        return;
    removeInBackground(paths, tr("Очистка корзины…"));
}

void FileOps::removeInBackground(const QStringList &paths, const QString &text)
{
    if (!startJob(text))
        return;
    auto cancel = m_cancel;
    m_job = QtConcurrent::run([this, paths, text, cancel] {
        QStringList failed;
        for (int i = 0; i < paths.size() && !cancel->load(); ++i) {
            const QString &p = paths.at(i);
            makeWritable(p);
            if (!removeTree(p) && QFileInfo::exists(p))
                failed << QFileInfo(p).fileName();
            else if (Trash::isTopLevel(p))
                Trash::forget(p);
            const double progress = double(i + 1) / double(paths.size());
            QMetaObject::invokeMethod(this, [this, progress, text] { setProgress(progress, text); }, Qt::QueuedConnection);
        }
        const QString error = failed.isEmpty() ? QString()
                                               : tr("Не удалось удалить: %1").arg(failed.join(QStringLiteral(", ")));
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(this, [this, error, cancelled] { finishJob({}, error, cancelled); }, Qt::QueuedConnection);
    });
}

QVariantMap FileOps::trashInfo(const QString &path)
{
    const Trash::Info info = Trash::isTopLevel(path) ? Trash::readInfo(path) : Trash::Info{};
    if (!info.valid())
        return {};
    return {{QStringLiteral("origPath"), info.origPath},
            {QStringLiteral("origDir"), QFileInfo(info.origPath).absolutePath()},
            {QStringLiteral("deleted"), info.deleted.toString(QStringLiteral("dd.MM.yyyy HH:mm"))}};
}

QString FileOps::rename(const QString &path, const QString &newName)
{
    const QString name = newName.trimmed();
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name == QLatin1String(".") || name == QLatin1String(".."))
        return tr("Недопустимое имя");
    const QFileInfo fi(path);
    if (name == fi.fileName())
        return {};
    const QString target = fi.absolutePath() + QLatin1Char('/') + name;
    if (QFileInfo::exists(target))
        return tr("«%1» уже есть").arg(name);
    if (!QDir().rename(path, target))
        return tr("Не удалось переименовать");
    record(QStringLiteral("rename"), tr("переименование «%1»").arg(fi.fileName()), {{UndoStep::Moved, path, target}});
    return {};
}

QString FileOps::makeDir(const QString &dir, const QString &baseName)
{
    const QString target = uniquePath(dir, baseName, true);
    if (!QDir().mkdir(target))
        return {};
    record(QStringLiteral("mkdir"), tr("создание папки «%1»").arg(QFileInfo(target).fileName()), {{UndoStep::MadeDir, target, {}}});
    return target;
}

void FileOps::copyToClipboard(const QStringList &paths, bool cut)
{
    if (paths.isEmpty())
        return;
    auto *md = new QMimeData;
    QList<QUrl> urls;
    QStringList uriLines;
    for (const QString &p : paths) {
        urls << QUrl::fromLocalFile(p);
        uriLines << urls.last().toString(QUrl::FullyEncoded);
    }
    md->setUrls(urls);
    md->setText(paths.join(QLatin1Char('\n')));
    // Форматы GNOME и KDE — чтобы вставка работала в Thunar/Nautilus/Dolphin
    md->setData(kGnomeCopied, ((cut ? QStringLiteral("cut\n") : QStringLiteral("copy\n")) + uriLines.join(QLatin1Char('\n'))).toUtf8());
    md->setData(kKdeCut, cut ? "1" : "0");
    QGuiApplication::clipboard()->setMimeData(md);
}

void FileOps::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void FileOps::readClipboard()
{
    const QMimeData *md = QGuiApplication::clipboard()->mimeData();
    QStringList files;
    bool cut = false;
    if (md) {
        for (const QUrl &u : md->urls())
            if (u.isLocalFile())
                files << u.toLocalFile();
        if (md->hasFormat(kGnomeCopied))
            cut = md->data(kGnomeCopied).startsWith("cut");
        else if (md->hasFormat(kKdeCut))
            cut = md->data(kKdeCut) == "1";
    }
    if (files == m_clipFiles && cut == m_clipCut)
        return;
    m_clipFiles = files;
    m_clipCut = cut;
    emit clipboardChanged();
}

QStringList FileOps::clipboardPaths()
{
    readClipboard();
    return m_clipFiles;
}

QVariantList FileOps::conflicts(const QStringList &sources, const QString &destDir) const
{
    const QString dest = QDir::cleanPath(destDir);
    const auto describe = [](const QFileInfo &fi) {
        const QString when = fi.lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
        if (!realDir(fi))
            return formatSize(fi.size()) + QStringLiteral(" · ") + when;
        const qsizetype n = QDir(fi.absoluteFilePath()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System).size();
        return tr("папка, %1 · %2").arg(objects(n), when);
    };
    QVariantList res;
    QStringList seen;
    for (const QString &s : sources) {
        const QString src = QDir::cleanPath(s);
        const QFileInfo fi(src);
        const QString name = fi.fileName();
        const QString target = dest + QLatin1Char('/') + name;
        // Перемещать на то же место некуда, копия рядом с собой — всегда « (2)»
        if (fi.absolutePath() == dest || !present(target) || seen.contains(name))
            continue;
        seen << name;
        const QFileInfo t(target);
        const QDateTime a = fi.lastModified(), b = t.lastModified();
        res << QVariantMap{
            {QStringLiteral("name"), name},
            {QStringLiteral("isDir"), realDir(fi)},
            {QStringLiteral("merge"), realDir(fi) && realDir(t)},
            // Заменить папку, внутри которой лежит переносимое, — значит выбросить его самого
            {QStringLiteral("canReplace"), !src.startsWith(target + QLatin1Char('/'))},
            {QStringLiteral("srcInfo"), describe(fi)},
            {QStringLiteral("dstInfo"), describe(t)},
            {QStringLiteral("newer"), a > b ? QStringLiteral("src") : a < b ? QStringLiteral("dst") : QString()},
        };
    }
    return res;
}

void FileOps::paste(const QString &destDir, const QVariantMap &resolutions)
{
    readClipboard();
    if (m_clipFiles.isEmpty())
        return;
    const bool cut = m_clipCut;
    transfer(m_clipFiles, destDir, cut, resolutions);
    // Вырезанное вставляется один раз
    if (cut)
        QGuiApplication::clipboard()->clear();
}

void FileOps::transfer(const QStringList &sources, const QString &destDir, bool move, const QVariantMap &resolutions)
{
    if (m_busy) {
        emit failed(tr("Дождитесь окончания текущей операции"));
        return;
    }
    const QString dest = QDir::cleanPath(destDir);
    for (const QString &s : sources) {
        const QString src = QDir::cleanPath(s);
        if (dest == src || dest.startsWith(src + QLatin1Char('/'))) {
            emit failed(tr("Нельзя скопировать папку внутрь самой себя"));
            return;
        }
    }
    if (!startJob(move ? tr("Перемещение…") : tr("Копирование…")))
        return;

    auto cancel = m_cancel;
    m_job = QtConcurrent::run([this, sources, dest, move, resolutions, cancel] {
        QStringList created;
        QList<UndoStep> steps;
        CopyCtx ctx{*cancel, 0, 0, {}, {}};
        // Размер считаем только у того, что придётся копировать
        for (const QString &s : sources)
            if (!move || !sameDevice(parentOf(s), dest))
                ctx.total += treeSize(s, *cancel);

        QElapsedTimer tick;
        tick.start();
        ctx.report = [&](const QString &name) {
            if (tick.elapsed() < 100)
                return;
            tick.restart();
            const double p = ctx.total > 0 ? qMin(1.0, double(ctx.done) / double(ctx.total)) : 0;
            const QString text = (move ? tr("Перемещение %1 — %2 из %3") : tr("Копирование %1 — %2 из %3"))
                                     .arg(name, formatSize(ctx.done), formatSize(ctx.total));
            QMetaObject::invokeMethod(this, [this, p, text] { setProgress(p, text); }, Qt::QueuedConnection);
        };

        QStringList done;
        placeAll(sources, dest, move, resolutions, ctx, steps, created, done);

        const QString label = (move ? tr("перемещение %1") : tr("копирование %1")).arg(subject(done));
        const QString error = ctx.error;
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(this, [this, created, error, cancelled, label, steps, move] {
            // Прерванное тоже записываем — сделанную часть можно откатить
            if (!steps.isEmpty())
                record(move ? QStringLiteral("move") : QStringLiteral("copy"), label, steps);
            finishJob(created, error, cancelled);
        }, Qt::QueuedConnection);
    });
}

QStringList FileOps::undoTargets() const
{
    QStringList res;
    if (m_undo.isEmpty())
        return res;
    for (const UndoStep &st : m_undo.last().steps)
        if (st.kind == UndoStep::Created)
            res << st.from;
        else if (st.kind == UndoStep::Moved)
            res << st.to;
    return res;
}

void FileOps::undo()
{
    rollBack(false);
}

void FileOps::redo()
{
    rollBack(true);
}

void FileOps::rollBack(bool redo)
{
    QList<UndoEntry> &from = redo ? m_redo : m_undo;
    if (from.isEmpty())
        return;
    if (m_busy) {
        emit failed(tr("Дождитесь окончания текущей операции"));
        return;
    }
    const UndoEntry entry = from.takeLast();
    emit undoChanged();
    const QString text = redo ? tr("Повтор: %1…").arg(entry.label) : tr("Отмена: %1…").arg(entry.label);
    if (!startJob(text))
        return;

    auto cancel = m_cancel;
    m_job = QtConcurrent::run([this, entry, cancel, text, redo] {
        CopyCtx ctx{*cancel, 0, 0, [](const QString &) {}, {}};
        QStringList restored, failed;
        QList<UndoStep> inverse;
        for (qsizetype i = entry.steps.size() - 1; i >= 0 && !cancel->load(); --i) {
            const UndoStep &st = entry.steps.at(i);
            if (!undoStep(st, ctx, restored, inverse))
                failed << QFileInfo(st.from).fileName();
            const double p = double(entry.steps.size() - i) / double(entry.steps.size());
            QMetaObject::invokeMethod(this, [this, p, text] { setProgress(p, text); }, Qt::QueuedConnection);
        }
        const QString error = failed.isEmpty() ? QString()
            : redo ? tr("Не удалось повторить для: %1 — его переместили или удалили").arg(failed.join(QStringLiteral(", ")))
                   : tr("Не удалось отменить для: %1 — его переместили или удалили").arg(failed.join(QStringLiteral(", ")));
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(this, [this, restored, error, cancelled, entry, inverse, redo] {
            // Откаченное (хотя бы частично) можно откатить обратно. Кроме архивов:
            // удалённое при отмене распакованное повтором уже не вернуть
            const bool noRedo = !redo && (entry.kind == QLatin1String("extract") || entry.kind == QLatin1String("compress"));
            if (!inverse.isEmpty() && !noRedo) {
                (redo ? m_undo : m_redo).append({entry.kind, entry.label, inverse});
                emit undoChanged();
            }
            finishJob(restored, error, cancelled);
            if (error.isEmpty() && !cancelled) {
                if (redo)
                    emit redone(entry.label);
                else
                    emit undone(entry.label);
            }
        }, Qt::QueuedConnection);
    });
}

// text/uri-list: строки через CRLF, как в RFC 2483
QString FileOps::uriList(const QStringList &paths) const
{
    QStringList lines;
    for (const QString &p : paths)
        lines << QUrl::fromLocalFile(p).toString(QUrl::FullyEncoded);
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

QStringList FileOps::localPaths(const QList<QUrl> &urls) const
{
    QStringList res;
    for (const QUrl &u : urls)
        if (u.isLocalFile())
            res << QDir::cleanPath(u.toLocalFile());
    return res;
}

QString FileOps::mimeTypeOf(const QString &path) const
{
    // По имени: файл может быть большим или на медленном диске, а листок мелькнёт на полсекунды
    static const QMimeDatabase db;
    return QFileInfo(path).isDir() ? QStringLiteral("inode/directory")
                                   : db.mimeTypeForFile(path, QMimeDatabase::MatchExtension).name();
}

bool FileOps::dropMoves(const QStringList &sources, const QString &destDir) const
{
    const Qt::KeyboardModifiers mods = QGuiApplication::queryKeyboardModifiers();
    if (mods & Qt::ControlModifier)
        return false;
    if (mods & Qt::ShiftModifier)
        return true;
    struct stat d {};
    if (sources.isEmpty() || ::stat(QFile::encodeName(destDir).constData(), &d) != 0)
        return false;
    for (const QString &s : sources) {
        struct stat st {};
        // Ссылку перемещаем как ссылку — смотрим на папку, где она лежит
        if (::stat(QFile::encodeName(QFileInfo(s).absolutePath()).constData(), &st) != 0 || st.st_dev != d.st_dev)
            return false;
    }
    return true;
}

void FileOps::cancel()
{
    if (m_cancel)
        m_cancel->store(true);
}

void FileOps::setProgress(double p, const QString &text)
{
    m_progress = p;
    m_jobText = text;
    emit progressChanged();
}

bool FileOps::startJob(const QString &text)
{
    if (m_busy) {
        emit failed(tr("Дождитесь окончания текущей операции"));
        return false;
    }
    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_busy = true;
    emit busyChanged();
    setProgress(0, text);
    return true;
}

void FileOps::record(const QString &kind, const QString &label, const QList<UndoStep> &steps)
{
    if (steps.isEmpty())
        return;
    m_undo.append({kind, label, steps});
    m_redo.clear();
    // Помним последние операции — дальше вряд ли кто-то отматывает
    while (m_undo.size() > 50)
        m_undo.removeFirst();
    emit undoChanged();
}

QString FileOps::previewKind(const QString &path, const QString &mimeType) const
{
    if (QFileInfo(path).isDir())
        return QStringLiteral("dir");
    if (QImageReader::supportedMimeTypes().contains(mimeType.toLatin1()))
        return QStringLiteral("image");
    // Код, конфиги, json, скрипты — всё, что наследует text/plain
    static const QMimeDatabase db;
    return db.mimeTypeForName(mimeType).inherits(QStringLiteral("text/plain")) ? QStringLiteral("text")
                                                                               : QStringLiteral("other");
}

QVariantMap FileOps::readTextPreview(const QString &path, qint64 limit)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {{QStringLiteral("text"), QString()}, {QStringLiteral("truncated"), false}, {QStringLiteral("binary"), false}};
    const QByteArray data = f.read(limit);
    const bool truncated = !f.atEnd();
    if (data.contains('\0'))
        return {{QStringLiteral("text"), QString()}, {QStringLiteral("truncated"), truncated}, {QStringLiteral("binary"), true}};
    // Без flush: оборванный на границе многобайтный символ остаётся в декодере, а не становится «�»
    QStringDecoder utf8(QStringDecoder::Utf8);
    const QString text = utf8(data);
    return {{QStringLiteral("text"), text}, {QStringLiteral("truncated"), truncated}, {QStringLiteral("binary"), false}};
}

void FileOps::requestTextPreview(const QString &path)
{
    // Файл может быть на медленном диске или в сети — не держим интерфейс
    QPointer<FileOps> self(this);
    (void)QtConcurrent::run([self, path] {
        const QVariantMap r = readTextPreview(path);
        QMetaObject::invokeMethod(qApp, [self, path, r] {
            if (self)
                emit self->textPreview(path, r.value(QStringLiteral("text")).toString(),
                                       r.value(QStringLiteral("truncated")).toBool(), r.value(QStringLiteral("binary")).toBool());
        }, Qt::QueuedConnection);
    });
}

QString FileOps::archiver()
{
    // bsdtar (libarchive) умеет и zip, и tar.*, и 7z; 7z — запасной
    static const QString path = [] {
        for (const char *name : {"bsdtar", "7z", "7zz", "7za"}) {
            const QString p = QStandardPaths::findExecutable(QString::fromLatin1(name));
            if (!p.isEmpty())
                return p;
        }
        return QString();
    }();
    return path;
}

QString FileOps::archiveBaseName(const QString &fileName)
{
    static const QMimeDatabase db;
    const QString suffix = db.suffixForFileName(fileName);
    return suffix.isEmpty() || suffix.size() + 1 >= fileName.size() ? fileName : fileName.chopped(suffix.size() + 1);
}

bool FileOps::canExtract(const QString &mimeType) const
{
    static const QRegularExpression re(QStringLiteral("zip|x-tar|compressed-tar|x-7z|x-rar|vnd\\.rar|x-xz|x-zstd|x-bzip|gzip|x-lzma|x-cpio|java-archive"));
    return !archiver().isEmpty() && re.match(mimeType).hasMatch();
}

bool FileOps::canCompress(const QString &format) const
{
    const QString tool = QFileInfo(archiver()).fileName();
    if (format == QLatin1String("zip"))
        return !tool.isEmpty();
    // 7z не умеет zstd
    return format == QLatin1String("tar.zst") && tool == QLatin1String("bsdtar");
}

void FileOps::extract(const QString &archive, const QString &destDir, bool intoFolder)
{
    const QString tool = archiver();
    if (tool.isEmpty()) {
        emit failed(tr("Для архивов нужен bsdtar (libarchive) или 7z"));
        return;
    }
    const QFileInfo fi(archive);
    const QString dest = QDir::cleanPath(destDir);
    const QString name = fi.fileName();
    // Временная папка рядом с местом назначения: тот же диск, дальше — переименованием
    const QString tmp = dest + QStringLiteral("/.shelf-extract-%1").arg(QRandomGenerator::global()->generate(), 0, 16);
    if (!QDir().mkdir(tmp)) {
        emit failed(tr("Не удалось создать папку в %1").arg(dest));
        return;
    }
    if (!startJob(tr("Распаковка «%1»…").arg(name))) {
        QDir().rmdir(tmp);
        return;
    }

    auto cancel = m_cancel;
    m_job = QtConcurrent::run([this, tool, archive = fi.canonicalFilePath(), size = fi.size(), name, dest, tmp, intoFolder, cancel] {
        const bool bsd = tool.endsWith(QLatin1String("bsdtar"));
        const auto args = [&](const QString &file) {
            return bsd ? QStringList{QStringLiteral("-x"), QStringLiteral("-f"), file, QStringLiteral("-C"), tmp}
                       : QStringList{QStringLiteral("x"), QStringLiteral("-y"), QStringLiteral("-bd"), QStringLiteral("-o") + tmp,
                                     QStringLiteral("--"), file};
        };
        const auto tick = [&](qint64 pid, const QByteArray &) {
            const qint64 off = readOffset(pid, archive);
            if (off < 0 || size <= 0)
                return;
            const double p = qMin(1.0, double(off) / double(size));
            const QString text = tr("Распаковка «%1» — %2 из %3").arg(name, formatSize(off), formatSize(size));
            QMetaObject::invokeMethod(this, [this, p, text] { setProgress(p, text); }, Qt::QueuedConnection);
        };
        QString error;
        bool ok = runTool(tool, args(archive), tmp, *cancel, tick, error);
        // 7z снимает с .tar.gz только сжатие — внутри остаётся .tar, распаковываем и его
        if (ok && !bsd && !tarInside(name).isEmpty()) {
            const QStringList inner = QDir(tmp).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
            if (inner.size() == 1 && inner.first().endsWith(QLatin1String(".tar"))) {
                const QString tar = tmp + QLatin1Char('/') + inner.first();
                ok = runTool(tool, args(tar), tmp, *cancel, [](qint64, const QByteArray &) {}, error);
                QFile::remove(tar);
            }
        }
        const bool cancelled = cancel->load();
        if (!ok)
            removeTree(tmp);
        QMetaObject::invokeMethod(this, [this, ok, error, cancelled, tmp, dest, name, intoFolder] {
            if (!ok) {
                finishJob({}, error, cancelled);
                return;
            }
            if (intoFolder) {
                const QString target = uniquePath(dest, archiveBaseName(name), true);
                if (!QDir().rename(tmp, target)) {
                    removeTree(tmp);
                    finishJob({}, tr("Не удалось создать папку в %1").arg(dest), false);
                    return;
                }
                record(QStringLiteral("extract"), tr("распаковывание «%1»").arg(name), {{UndoStep::Created, target, {}}});
                finishJob({target}, {}, false);
                return;
            }
            QStringList entries;
            for (const QFileInfo &e : QDir(tmp).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
                entries << e.absoluteFilePath();
            const QVariantList cs = conflicts(entries, dest);
            m_extracting.insert(tmp, name);
            finishJob({}, {}, false);
            emit extracted(tmp, dest, cs);
        }, Qt::QueuedConnection);
    });
}

void FileOps::placeExtracted(const QString &tmpDir, const QString &destDir, const QVariantMap &resolutions)
{
    const QString tmp = QDir::cleanPath(tmpDir);
    const QString dest = QDir::cleanPath(destDir);
    if (!QFileInfo(tmp).fileName().startsWith(QLatin1String(".shelf-extract-")) || !startJob(tr("Распаковка…")))
        return;
    auto cancel = m_cancel;
    const QString label = tr("распаковывание «%1»").arg(m_extracting.take(tmp));
    m_job = QtConcurrent::run([this, tmp, dest, resolutions, cancel, label] {
        QStringList entries, created, done;
        for (const QFileInfo &e : QDir(tmp).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
            entries << e.absoluteFilePath();
        QList<UndoStep> steps;
        CopyCtx ctx{*cancel, 0, 0, [](const QString &) {}, {}};
        placeAll(entries, dest, true, resolutions, ctx, steps, created, done);
        // Перенесённое из временной папки для отката — просто появившееся: его удаляют, а не возвращают
        QList<UndoStep> undo;
        for (const UndoStep &st : steps) {
            if (!st.from.startsWith(tmp + QLatin1Char('/')))
                undo << st;
            else if (st.kind == UndoStep::Moved || st.kind == UndoStep::Created)
                undo << UndoStep{UndoStep::Created, st.kind == UndoStep::Moved ? st.to : st.from, {}};
        }
        removeTree(tmp); // пропущенное
        const QString error = ctx.error;
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(this, [this, created, error, cancelled, undo, label] {
            if (!undo.isEmpty())
                record(QStringLiteral("extract"), label, undo);
            finishJob(created, error, cancelled);
        }, Qt::QueuedConnection);
    });
}

void FileOps::discardExtracted(const QString &tmpDir)
{
    if (!QFileInfo(tmpDir).fileName().startsWith(QLatin1String(".shelf-extract-")))
        return;
    m_extracting.remove(QDir::cleanPath(tmpDir));
    removeTree(tmpDir);
}

void FileOps::compress(const QStringList &paths, const QString &format)
{
    if (paths.isEmpty() || !canCompress(format))
        return;
    const QString tool = archiver();
    const QFileInfo first(paths.first());
    const QString parent = first.absolutePath();
    const QString base = paths.size() == 1 ? (first.isDir() ? first.fileName() : first.completeBaseName()) : tr("Архив");
    QString target = parent + QLatin1Char('/') + base + QLatin1Char('.') + format;
    for (int i = 2; present(target); ++i)
        target = QStringLiteral("%1/%2 (%3).%4").arg(parent, base).arg(i).arg(format);
    // Пишем во временный файл рядом: недописанный архив не выглядит готовым.
    // Расширение то же — по нему bsdtar выбирает формат
    const QString tmp = parent + QStringLiteral("/.shelf-%1.").arg(QRandomGenerator::global()->generate(), 0, 16) + format;
    QStringList names;
    for (const QString &p : paths)
        names << QFileInfo(p).fileName();
    const QString targetName = QFileInfo(target).fileName();
    if (!startJob(tr("Сжатие в «%1»…").arg(targetName)))
        return;

    auto cancel = m_cancel;
    m_job = QtConcurrent::run([this, tool, paths, names, parent, tmp, target, targetName, cancel] {
        const bool bsd = tool.endsWith(QLatin1String("bsdtar"));
        // Для bsdtar прогресс — по числу уложенных объектов (он печатает «a имя» на каждый)
        qsizetype total = 0;
        if (bsd)
            for (const QString &p : paths) {
                ++total;
                QDirIterator it(p, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
                while (it.hasNext() && !cancel->load()) {
                    it.next();
                    ++total;
                }
            }
        const QStringList args = bsd
            ? QStringList{QStringLiteral("-a"), QStringLiteral("-c"), QStringLiteral("-v"), QStringLiteral("-f"), tmp,
                          QStringLiteral("-C"), parent, QStringLiteral("--")} + names
            : QStringList{QStringLiteral("a"), QStringLiteral("-tzip"), QStringLiteral("-bsp1"), QStringLiteral("-bb0"),
                          QStringLiteral("-y"), tmp, QStringLiteral("--")} + names;
        const auto tick = [&](qint64, const QByteArray &out) {
            double p = 0;
            if (bsd) {
                const qsizetype added = out.count("\na ") + (out.startsWith("a ") ? 1 : 0);
                p = total > 0 ? qMin(1.0, double(added) / double(total)) : 0;
            } else {
                static const QRegularExpression pct(QStringLiteral("(\\d+)%"));
                QRegularExpressionMatchIterator it = pct.globalMatch(QString::fromUtf8(out.right(200)));
                while (it.hasNext())
                    p = it.next().captured(1).toDouble() / 100.0;
            }
            const QString text = tr("Сжатие в «%1» — %2%").arg(targetName).arg(qRound(p * 100));
            QMetaObject::invokeMethod(this, [this, p, text] { setProgress(p, text); }, Qt::QueuedConnection);
        };
        QString error;
        bool ok = runTool(tool, args, parent, *cancel, tick, error);
        if (ok && !QFile::rename(tmp, target)) {
            ok = false;
            error = tr("Не удалось сохранить %1").arg(targetName);
        }
        if (!ok)
            QFile::remove(tmp);
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(this, [this, ok, error, cancelled, target, targetName] {
            if (ok)
                record(QStringLiteral("compress"), tr("создание архива «%1»").arg(targetName), {{UndoStep::Created, target, {}}});
            finishJob(ok ? QStringList{target} : QStringList{}, error, cancelled);
        }, Qt::QueuedConnection);
    });
}

void FileOps::finishJob(const QStringList &created, const QString &error, bool cancelled)
{
    m_busy = false;
    setProgress(0, {});
    emit busyChanged();
    if (!error.isEmpty() && !cancelled)
        emit failed(error);
    emit finished(created);
}

QVariantMap FileOps::info(const QString &path)
{
    const QFileInfo fi(path);
    QVariantMap m;
    m[QStringLiteral("name")] = fi.fileName().isEmpty() ? path : fi.fileName();
    m[QStringLiteral("path")] = fi.absoluteFilePath();
    m[QStringLiteral("dir")] = fi.absolutePath();
    m[QStringLiteral("modified")] = fi.lastModified().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
    m[QStringLiteral("owner")] = fi.owner();
    if (fi.isSymLink())
        m[QStringLiteral("link")] = fi.symLinkTarget();

    // rwxr-xr-x
    const QFileDevice::Permissions p = fi.permissions();
    const QFileDevice::Permission bits[] = {QFileDevice::ReadOwner, QFileDevice::WriteOwner, QFileDevice::ExeOwner,
                                             QFileDevice::ReadGroup, QFileDevice::WriteGroup, QFileDevice::ExeGroup,
                                             QFileDevice::ReadOther, QFileDevice::WriteOther, QFileDevice::ExeOther};
    QString perms;
    for (int i = 0; i < 9; ++i)
        perms += (p & bits[i]) ? QLatin1Char("rwx"[i % 3]) : QLatin1Char('-');
    m[QStringLiteral("permissions")] = perms;

    if (fi.isDir()) {
        m[QStringLiteral("type")] = tr("Папка");
        m[QStringLiteral("isDir")] = true;
        // Размер папки считаем с потолком, чтобы окно не зависало на /
        qint64 total = 0;
        int files = 0, dirs = 0;
        bool truncated = false;
        QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System | QDir::NoSymLinks,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo c = it.nextFileInfo();
            if (c.isDir())
                ++dirs;
            else {
                ++files;
                total += c.size();
            }
            if (files + dirs > 100000) {
                truncated = true;
                break;
            }
        }
        m[QStringLiteral("size")] = truncated ? tr("больше %1").arg(formatSize(total)) : formatSize(total);
        m[QStringLiteral("contents")] = tr("%n файл(ов)", nullptr, int(files)) + QStringLiteral(", ") + tr("%n папка(ок)", nullptr, int(dirs));
    } else {
        const QMimeType mt = mimeForFile(fi, QMimeDatabase());
        m[QStringLiteral("type")] = QStringLiteral("%1 (%2)").arg(mt.comment(), mt.name());
        m[QStringLiteral("mimeType")] = mt.name();
        // Форма слова зависит только от двух последних цифр — а %n у Qt не шире int.
        // Само число — %1 с разрядами, %n в строке нет
        const qint64 bytes = fi.size();
        const int form = bytes < 100 ? int(bytes) : int(bytes % 100) + 100;
        m[QStringLiteral("size")] = tr("%1 (%2 байт(а))", nullptr, form).arg(formatSize(bytes), QLocale().toString(bytes));
    }
    return m;
}
