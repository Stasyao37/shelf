#include "FileOps.h"

#include "FileEntry.h"

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

bool removeTree(const QString &path)
{
    const QFileInfo fi(path);
    if (fi.isDir() && !fi.isSymLink())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
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

void FileOps::open(const QString &path)
{
    QProcess::startDetached(QStringLiteral("xdg-open"), {path});
}

void FileOps::openTerminal(const QString &dir)
{
    QProcess::startDetached(QStringLiteral("kitty"), {QStringLiteral("--directory"), dir});
}

QString FileOps::trash(const QStringList &paths)
{
    QStringList failed;
    for (const QString &p : paths)
        if (!QFile::moveToTrash(p))
            failed << QFileInfo(p).fileName();
    return failed.isEmpty() ? QString() : QStringLiteral("Не удалось убрать в корзину: %1").arg(failed.join(QStringLiteral(", ")));
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
    return {};
}

QString FileOps::makeDir(const QString &dir, const QString &baseName)
{
    const QString target = uniquePath(dir, baseName, true);
    return QDir().mkdir(target) ? target : QString();
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

void FileOps::paste(const QString &destDir)
{
    readClipboard();
    if (m_clipFiles.isEmpty())
        return;
    const bool cut = m_clipCut;
    transfer(m_clipFiles, destDir, cut);
    // Вырезанное вставляется один раз
    if (cut)
        QGuiApplication::clipboard()->clear();
}

void FileOps::transfer(const QStringList &sources, const QString &destDir, bool move)
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

    m_cancel = std::make_shared<std::atomic_bool>(false);
    m_busy = true;
    emit busyChanged();
    setProgress(0, move ? QStringLiteral("Перемещение…") : QStringLiteral("Копирование…"));

    auto cancel = m_cancel;
    m_job = QtConcurrent::run([this, sources, dest, move, cancel] {
        QStringList created;
        QStringList pending; // что не удалось просто переименовать — копируем
        for (const QString &s : sources) {
            const QString src = QDir::cleanPath(s);
            const QFileInfo fi(src);
            if (move && fi.absolutePath() == dest)
                continue; // уже здесь
            const QString target = uniquePath(dest, fi.fileName(), fi.isDir());
            // В пределах одного раздела перемещение — это rename, мгновенно
            if (move && QDir().rename(src, target))
                created << target;
            else
                pending << src;
        }

        CopyCtx ctx{*cancel, 0, 0, {}, {}};
        for (const QString &src : std::as_const(pending))
            ctx.total += treeSize(src, *cancel);

        QElapsedTimer tick;
        tick.start();
        ctx.report = [&](const QString &name) {
            if (tick.elapsed() < 100)
                return;
            tick.restart();
            const double p = ctx.total > 0 ? double(ctx.done) / double(ctx.total) : 0;
            const QString text = QStringLiteral("%1 %2 — %3 из %4")
                                     .arg(move ? QStringLiteral("Перемещение") : QStringLiteral("Копирование"), name,
                                          formatSize(ctx.done), formatSize(ctx.total));
            QMetaObject::invokeMethod(this, [this, p, text] { setProgress(p, text); }, Qt::QueuedConnection);
        };

        for (const QString &src : std::as_const(pending)) {
            if (cancel->load())
                break;
            const QFileInfo fi(src);
            const QString target = uniquePath(dest, fi.fileName(), fi.isDir());
            if (!copyTree(src, target, ctx)) {
                removeTree(target); // половинчатую копию не оставляем
                break;
            }
            created << target;
            if (move && !removeTree(src)) {
                ctx.error = QStringLiteral("Скопировано, но не удалось удалить исходник %1").arg(src);
                break;
            }
        }

        const QString error = ctx.error;
        const bool cancelled = cancel->load();
        QMetaObject::invokeMethod(this, [this, created, error, cancelled] { finishJob(created, error, cancelled); },
                                  Qt::QueuedConnection);
    });
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
        const QMimeType mt = QMimeDatabase().mimeTypeForFile(fi);
        m[QStringLiteral("type")] = QStringLiteral("%1 (%2)").arg(mt.comment(), mt.name());
        m[QStringLiteral("size")] = QStringLiteral("%1 (%2 байт)").arg(formatSize(fi.size()), QLocale(QLocale::Russian).toString(fi.size()));
    }
    return m;
}
