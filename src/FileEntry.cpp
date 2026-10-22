#include "FileEntry.h"

#include <QDir>
#include <QHash>
#include <QLocale>
#include <QMimeDatabase>
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
        return h;
    }();
    return special.value(path, QStringLiteral("folder")) + QStringLiteral(",folder");
}

QMimeType mimeForFile(const QFileInfo &fi, const QMimeDatabase &db)
{
    // По расширению — без чтения содержимого, иначе большие папки тормозят
    QMimeType mt = db.mimeTypeForFile(fi, QMimeDatabase::MatchExtension);
    // Незнакомое расширение (.db, без расширения) — смотрим сигнатуру
    // в начале файла; таких файлов мало, папку это не тормозит
    if (mt.isDefault() && fi.size() > 0)
        mt = db.mimeTypeForFile(fi, QMimeDatabase::MatchContent);
    // shared-mime-info знает только *.sqlite3, а базы часто зовут иначе
    static const QStringList sqliteSuffixes = {
        QStringLiteral("sqlite"), QStringLiteral("db"), QStringLiteral("db3"),
        QStringLiteral("sqlitedb")};
    if (mt.isDefault() && sqliteSuffixes.contains(fi.suffix(), Qt::CaseInsensitive))
        mt = db.mimeTypeForName(QStringLiteral("application/vnd.sqlite3"));
    return mt;
}

FileEntry FileEntry::fromInfo(const QFileInfo &fi, const QMimeDatabase &db, bool countChildren)
{
    FileEntry e;
    e.name = fi.fileName();
    e.path = fi.absoluteFilePath();
    e.dir = fi.absolutePath();
    e.isDir = fi.isDir();
    e.isHidden = e.name.startsWith(QLatin1Char('.'));
    e.isLink = fi.isSymLink();
    e.mtime = fi.lastModified();
    if (e.isDir) {
        e.icon = folderIcon(e.path);
        e.mimeType = QStringLiteral("inode/directory");
        e.mimeComment = QStringLiteral("Папка");
        if (countChildren)
            e.childCount = int(QDir(e.path).count()) - 2; // без . и ..
    } else {
        e.size = fi.size();
        const QMimeType mt = mimeForFile(fi, db);
        e.mimeType = mt.name();
        e.mimeComment = mt.comment();
        e.icon = mt.iconName() + QLatin1Char(',') + mt.genericIconName();
    }
    return e;
}

QString formatSize(qint64 bytes)
{
    static const char *units[] = {"Б", "КБ", "МБ", "ГБ", "ТБ"};
    double v = double(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    const QLocale loc(QLocale::Russian);
    return (u == 0 ? QString::number(bytes) : loc.toString(v, 'f', v < 10 ? 1 : 0))
           + QLatin1Char(' ') + QString::fromUtf8(units[u]);
}

QString formatDate(const QDateTime &dt)
{
    if (!dt.isValid())
        return {};
    const QLocale loc(QLocale::Russian);
    const QDate today = QDate::currentDate();
    const QString time = dt.toString(QStringLiteral("HH:mm"));
    if (dt.date() == today)
        return QStringLiteral("Сегодня, ") + time;
    if (dt.date() == today.addDays(-1))
        return QStringLiteral("Вчера, ") + time;
    if (dt.date().year() == today.year())
        return loc.toString(dt.date(), QStringLiteral("d MMM")) + QStringLiteral(", ") + time;
    return dt.toString(QStringLiteral("dd.MM.yyyy"));
}
