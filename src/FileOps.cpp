#include "FileOps.h"

#include "FileEntry.h"
#include "Terminal.h"
#include "Trash.h"

#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QMimeDatabase>
#include <QProcess>
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
        ctx.error = QStringLiteral("Не удалось прочитать %1").arg(src);
        return false;
    }
    if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        ctx.error = QStringLiteral("Не удалось создать %1").arg(dst);
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
            ctx.error = QStringLiteral("Ошибка записи %1").arg(dst);
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
        return QFile::link(fi.symLinkTarget(), dst) || (ctx.error = QStringLiteral("Не удалось создать ссылку %1").arg(dst), false);
    if (!fi.isDir())
        return copyFile(src, dst, ctx);
    if (!QDir().mkpath(dst)) {
        ctx.error = QStringLiteral("Не удалось создать папку %1").arg(dst);
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
    const qsizetype d = n % 10, h = n % 100;
    const char *w = d == 1 && h != 11 ? "объект" : d >= 2 && d <= 4 && (h < 12 || h > 14) ? "объекта" : "объектов";
    return QStringLiteral("%1 %2").arg(n).arg(QString::fromUtf8(w));
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
        ctx.error = QStringLiteral("Не удалось убрать в корзину заменяемый %1").arg(target);
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
        steps.append({UndoStep::Created, dst, {}});
        ctx.error = QStringLiteral("Скопировано, но не удалось удалить исходник %1").arg(src);
        return false;
    }
    steps.append(move ? UndoStep{UndoStep::Moved, src, dst} : UndoStep{UndoStep::Created, dst, {}});
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

// Откат одного шага; restored — где оказалось возвращённое
bool undoStep(const UndoStep &st, CopyCtx &ctx, QStringList &restored)
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
        return true;
    };
    switch (st.kind) {
    case UndoStep::Created:
        // Копия — лишний дубль, в корзине ей делать нечего
        if (!present(st.from))
            return true;
        makeWritable(st.from);
        return removeTree(st.from);
    case UndoStep::Moved:
        return putBack(st.to, st.from, false);
    case UndoStep::Trashed:
        return putBack(st.to, st.from, true);
    case UndoStep::MadeDir:
        // В новую папку уже что-то положили — не трогаем
        return !present(st.from) || QDir().rmdir(st.from);
    case UndoStep::RemovedDir:
        return QDir().mkpath(st.from);
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
}

void FileOps::openTerminal(const QString &dir)
{
    if (!Terminal::openIn(dir))
        emit failed(QStringLiteral("Не найден терминал — укажите его в настройках"));
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
        record(QStringLiteral("удаление в корзину %1").arg(subject(done)), steps);
    return failed.isEmpty() ? QString() : QStringLiteral("Не удалось убрать в корзину: %1").arg(failed.join(QStringLiteral(", ")));
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
        res[QStringLiteral("error")] = QStringLiteral("Не удалось восстановить: %1").arg(failed.join(QStringLiteral(", ")));
    return res;
}

void FileOps::deleteForever(const QStringList &paths)
{
    for (const QString &p : paths)
        if (!Trash::contains(p)) {
            emit failed(QStringLiteral("Удалять навсегда можно только из корзины"));
            return;
        }
    removeInBackground(paths, QStringLiteral("Удаление…"));
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
    removeInBackground(paths, QStringLiteral("Очистка корзины…"));
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
                                               : QStringLiteral("Не удалось удалить: %1").arg(failed.join(QStringLiteral(", ")));
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
        return QStringLiteral("Недопустимое имя");
    const QFileInfo fi(path);
    if (name == fi.fileName())
        return {};
    const QString target = fi.absolutePath() + QLatin1Char('/') + name;
    if (QFileInfo::exists(target))
        return QStringLiteral("«%1» уже есть").arg(name);
    if (!QDir().rename(path, target))
        return QStringLiteral("Не удалось переименовать");
    record(QStringLiteral("переименование «%1»").arg(fi.fileName()), {{UndoStep::Moved, path, target}});
    return {};
}

QString FileOps::makeDir(const QString &dir, const QString &baseName)
{
    const QString target = uniquePath(dir, baseName, true);
    if (!QDir().mkdir(target))
        return {};
    record(QStringLiteral("создание папки «%1»").arg(QFileInfo(target).fileName()), {{UndoStep::MadeDir, target, {}}});
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
        return QStringLiteral("папка, %1 · %2").arg(objects(n), when);
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
        emit failed(QStringLiteral("Дождитесь окончания текущей операции"));
        return;
    }
    const QString dest = QDir::cleanPath(destDir);
    for (const QString &s : sources) {
        const QString src = QDir::cleanPath(s);
        if (dest == src || dest.startsWith(src + QLatin1Char('/'))) {
            emit failed(QStringLiteral("Нельзя скопировать папку внутрь самой себя"));
            return;
        }
    }
    if (!startJob(move ? QStringLiteral("Перемещение…") : QStringLiteral("Копирование…")))
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
            const QString text = QStringLiteral("%1 %2 — %3 из %4")
                                     .arg(move ? QStringLiteral("Перемещение") : QStringLiteral("Копирование"), name,
                                          formatSize(ctx.done), formatSize(ctx.total));
            QMetaObject::invokeMethod(this, [this, p, text] { setProgress(p, text); }, Qt::QueuedConnection);
        };

        QStringList done;
        for (const QString &s : sources) {
            if (cancel->load())
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

        const QString label = (move ? QStringLiteral("перемещение %1") : QStringLiteral("копирование %1")).arg(subject(done));
        const QString error = ctx.error;
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(this, [this, created, error, cancelled, label, steps] {
            // Прерванное тоже записываем — сделанную часть можно откатить
            if (!steps.isEmpty())
                record(label, steps);
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
    if (m_undo.isEmpty())
        return;
    if (m_busy) {
        emit failed(QStringLiteral("Дождитесь окончания текущей операции"));
        return;
    }
    const UndoEntry entry = m_undo.takeLast();
    emit undoChanged();
    if (!startJob(QStringLiteral("Отмена: %1…").arg(entry.label)))
        return;

    auto cancel = m_cancel;
    m_job = QtConcurrent::run([this, entry, cancel] {
        CopyCtx ctx{*cancel, 0, 0, [](const QString &) {}, {}};
        QStringList restored, failed;
        for (qsizetype i = entry.steps.size() - 1; i >= 0 && !cancel->load(); --i) {
            const UndoStep &st = entry.steps.at(i);
            if (!undoStep(st, ctx, restored))
                failed << QFileInfo(st.from).fileName();
            const double p = double(entry.steps.size() - i) / double(entry.steps.size());
            QMetaObject::invokeMethod(this, [this, p, entry] { setProgress(p, QStringLiteral("Отмена: %1…").arg(entry.label)); },
                                      Qt::QueuedConnection);
        }
        const QString error = failed.isEmpty() ? QString()
            : QStringLiteral("Не удалось отменить для: %1 — его переместили или удалили").arg(failed.join(QStringLiteral(", ")));
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(this, [this, restored, error, cancelled, label = entry.label] {
            finishJob(restored, error, cancelled);
            if (error.isEmpty() && !cancelled)
                emit undone(label);
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
        emit failed(QStringLiteral("Дождитесь окончания текущей операции"));
        return false;
    }
    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_busy = true;
    emit busyChanged();
    setProgress(0, text);
    return true;
}

void FileOps::record(const QString &label, const QList<UndoStep> &steps)
{
    if (steps.isEmpty())
        return;
    m_undo.append({label, steps});
    // Помним последние операции — дальше вряд ли кто-то отматывает
    while (m_undo.size() > 50)
        m_undo.removeFirst();
    emit undoChanged();
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
        m[QStringLiteral("type")] = QStringLiteral("Папка");
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
        m[QStringLiteral("size")] = (truncated ? QStringLiteral("больше ") : QString()) + formatSize(total);
        m[QStringLiteral("contents")] = QStringLiteral("%1 файлов, %2 папок").arg(files).arg(dirs);
    } else {
        const QMimeType mt = mimeForFile(fi, QMimeDatabase());
        m[QStringLiteral("type")] = QStringLiteral("%1 (%2)").arg(mt.comment(), mt.name());
        m[QStringLiteral("mimeType")] = mt.name();
        m[QStringLiteral("size")] = QStringLiteral("%1 (%2 байт)").arg(formatSize(fi.size()), QLocale(QLocale::Russian).toString(fi.size()));
    }
    return m;
}
