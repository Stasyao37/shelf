#include "FileEntry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMimeDatabase>
#include <QSet>
#include <QStandardPaths>

QString folderIcon(const QString &path)
{
    // Статика инициализируется потокобезопасно — функция зовётся и из поиска
    static const QHash<QString, QString> special = [] {
        QHash<QString, QString> h;
        auto add = [&h](QStandardPaths::StandardLocation loc, const char *icon) {
            const QString p = QStandardPaths::writableLocation(loc);
            if (!p.isEmpty() && p != QDir::homePath())
                h.insert(p, QString::fromLatin1(icon));
        };
        add(QStandardPaths::DesktopLocation, "user-desktop");
        add(QStandardPaths::DocumentsLocation, "folder-documents");
        add(QStandardPaths::DownloadLocation, "folder-download");
        add(QStandardPaths::MusicLocation, "folder-music");
        add(QStandardPaths::PicturesLocation, "folder-pictures");
        add(QStandardPaths::MoviesLocation, "folder-videos");
        add(QStandardPaths::TemplatesLocation, "folder-templates");
        add(QStandardPaths::PublicShareLocation, "folder-publicshare");
        h.insert(QDir::homePath(), QStringLiteral("user-home"));
        h.insert(QDir::homePath() + QStringLiteral("/Projects"), QStringLiteral("folder-projects"));
        return h;
    }();
    // Служебные папки сборки и зависимостей — блёклые, чтобы не цепляли взгляд
    static const QSet<QString> dimmed = {
        QStringLiteral("node_modules"), QStringLiteral("build"), QStringLiteral(".cache"),
        QStringLiteral("__pycache__"), QStringLiteral("target"), QStringLiteral("dist"),
        QStringLiteral(".venv"), QStringLiteral("venv"), QStringLiteral(".gradle")};
    static const QSet<QString> config = {QStringLiteral(".config"), QStringLiteral(".local")};

    QString icon = special.value(path);
    if (icon.isEmpty()) {
        const QString name = path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
        if (dimmed.contains(name))
            icon = QStringLiteral("folder-dim");
        else if (config.contains(name))
            icon = QStringLiteral("folder-config");
        else if (QFileInfo::exists(path + QStringLiteral("/.git")))
            icon = QStringLiteral("folder-git");
        else
            icon = QStringLiteral("folder");
    }
    return icon + QStringLiteral(",folder");
}

QMimeType mimeForFile(const QFileInfo &fi, const QMimeDatabase &db, bool sniff)
{
    // По расширению — без чтения содержимого, иначе большие папки тормозят
    QMimeType mt = db.mimeTypeForFile(fi, QMimeDatabase::MatchExtension);
    // Незнакомое расширение (.db, без расширения) — смотрим сигнатуру в начале файла.
    // Обычно таких мало, но в /usr/bin это тысячи файлов — там листинг их не открывает
    if (sniff && mt.isDefault() && fi.size() > 0)
        mt = db.mimeTypeForFile(fi, QMimeDatabase::MatchContent);
    // shared-mime-info знает только *.sqlite3, а базы часто зовут иначе
    static const QStringList sqliteSuffixes = {
        QStringLiteral("sqlite"), QStringLiteral("db"), QStringLiteral("db3"),
        QStringLiteral("sqlitedb")};
    if (mt.isDefault() && sqliteSuffixes.contains(fi.suffix(), Qt::CaseInsensitive))
        mt = db.mimeTypeForName(QStringLiteral("application/vnd.sqlite3"));
    return mt;
}

void FileEntry::setMime(const QMimeType &mt)
{
    mimeType = mt.name();
    mimeComment = mt.comment();
    icon = mt.iconName() + QLatin1Char(',') + mt.genericIconName();
}

FileEntry FileEntry::fromInfo(const QFileInfo &fi, const QMimeDatabase &db, bool countChildren, Typing typing)
{
    FileEntry e;
    e.name = fi.fileName();
    e.path = fi.absoluteFilePath();
    e.dir = fi.absolutePath();
    e.isDir = fi.isDir();
    e.isHidden = e.name.startsWith(QLatin1Char('.'));
    e.isLink = fi.isSymLink();
    const bool readable = fi.isReadable() && (!fi.isDir() || fi.isExecutable());
    e.isLocked = !readable || (!fi.isWritable() && QFileInfo(fi.absolutePath()).isWritable());
    e.mtime = fi.lastModified();
    if (e.isDir) {
        e.icon = folderIcon(e.path);
        e.mimeType = QStringLiteral("inode/directory");
        e.mimeComment = QCoreApplication::translate("FileEntry", "Папка");
        if (countChildren)
            e.childCount = int(QDir(e.path).count()) - 2; // без . и ..
    } else {
        e.size = fi.size();
        if (typing == Typing::Later) {
            // Заглушка — один раз на всю программу: база типов в Qt одна на всех под одним
            // замком, и пока фон читает под ним файлы, каждое обращение отсюда ждало бы его
            static const FileEntry placeholder = [&db] {
                FileEntry p;
                p.setMime(db.mimeTypeForName(QStringLiteral("application/octet-stream")));
                return p;
            }();
            e.mimeType = placeholder.mimeType;
            e.mimeComment = placeholder.mimeComment;
            e.icon = placeholder.icon;
            e.typePending = true;
        } else {
            const QMimeType mt = mimeForFile(fi, db, typing == Typing::Full);
            e.setMime(mt);
            e.typePending = typing == Typing::ByName && mt.isDefault() && e.size > 0;
        }
    }
    return e;
}

QString formatSize(qint64 bytes)
{
    const QString units[] = {QCoreApplication::translate("FileEntry", "Б"), QCoreApplication::translate("FileEntry", "КБ"),
                             QCoreApplication::translate("FileEntry", "МБ"), QCoreApplication::translate("FileEntry", "ГБ"),
                             QCoreApplication::translate("FileEntry", "ТБ")};
    double v = double(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    // Язык интерфейса (Language ставит его локалью по умолчанию): 2,4 МБ / 2.4 MB
    const QLocale loc;
    return (u == 0 ? QString::number(bytes) : loc.toString(v, 'f', v < 10 ? 1 : 0)) + QLatin1Char(' ') + units[u];
}

QString formatDate(const QDateTime &dt)
{
    if (!dt.isValid())
        return {};
    const QLocale loc;
    const QDate today = QDate::currentDate();
    const QString time = dt.toString(QStringLiteral("HH:mm"));
    if (dt.date() == today)
        return QCoreApplication::translate("FileEntry", "Сегодня, %1").arg(time);
    if (dt.date() == today.addDays(-1))
        return QCoreApplication::translate("FileEntry", "Вчера, %1").arg(time);
    if (dt.date().year() == today.year())
        return loc.toString(dt.date(), QStringLiteral("d MMM")) + QStringLiteral(", ") + time;
    // 28.10.2025 / 10/28/25 — как принято в языке интерфейса
    return loc.toString(dt.date(), QLocale::ShortFormat);
}
