#include "Places.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>

#include <algorithm>

namespace {
QString bookmarksFile()
{
    return QDir::homePath() + QStringLiteral("/.config/gtk-3.0/bookmarks");
}

QVariantMap item(const QString &name, const QString &path, const QString &icon)
{
    return {{QStringLiteral("name"), name}, {QStringLiteral("path"), path}, {QStringLiteral("icon"), icon}};
}
}

Places::Places(QObject *parent)
    : QObject(parent)
{
    m_places << item(QStringLiteral("Домашняя"), QDir::homePath(), QStringLiteral(""));
    auto add = [this](QStandardPaths::StandardLocation loc, const QString &name, const QString &icon) {
        const QString p = QStandardPaths::writableLocation(loc);
        if (!p.isEmpty() && p != QDir::homePath() && QFileInfo(p).isDir())
            m_places << item(name, p, icon);
    };
    add(QStandardPaths::DesktopLocation, QStringLiteral("Рабочий стол"), QStringLiteral(""));
    add(QStandardPaths::DocumentsLocation, QStringLiteral("Документы"), QStringLiteral(""));
    add(QStandardPaths::DownloadLocation, QStringLiteral("Загрузки"), QStringLiteral(""));
    add(QStandardPaths::PicturesLocation, QStringLiteral("Изображения"), QStringLiteral(""));
    add(QStandardPaths::MusicLocation, QStringLiteral("Музыка"), QStringLiteral(""));
    add(QStandardPaths::MoviesLocation, QStringLiteral("Видео"), QStringLiteral(""));
    m_places << item(QStringLiteral("Корзина"), trashPath(), QStringLiteral(""));

    // Редакторы пишут файл через замену — следим и за папкой
    m_watcher.addPath(QFileInfo(bookmarksFile()).absolutePath());
    if (QFileInfo::exists(bookmarksFile()))
        m_watcher.addPath(bookmarksFile());
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &Places::loadBookmarks);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &Places::loadBookmarks);
    loadBookmarks();

    // Флешки монтирует udisks в /run/media/$USER — проще опрашивать, чем ловить D-Bus
    m_devTimer.setInterval(3000);
    connect(&m_devTimer, &QTimer::timeout, this, &Places::refreshDevices);
    m_devTimer.start();
    refreshDevices();
}

QString Places::trashPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash/files");
}

QStringList Places::bookmarkLines() const
{
    QFile f(bookmarksFile());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

void Places::loadBookmarks()
{
    if (QFileInfo::exists(bookmarksFile()) && !m_watcher.files().contains(bookmarksFile()))
        m_watcher.addPath(bookmarksFile());

    QVariantList list;
    for (const QString &line : bookmarkLines()) {
        // «file:///путь Подпись» — подпись необязательна
        const int sp = line.indexOf(QLatin1Char(' '));
        const QUrl url(sp < 0 ? line : line.left(sp));
        if (!url.isLocalFile())
            continue; // sftp:// и т.п. — пока не умеем
        const QString path = QDir::cleanPath(url.toLocalFile());
        // Стандартные папки уже есть в «Местах» — GTK кладёт их и в закладки
        if (std::any_of(m_places.cbegin(), m_places.cend(),
                        [&path](const QVariant &v) { return v.toMap().value(QStringLiteral("path")) == path; }))
            continue;
        const QString name = sp < 0 ? QFileInfo(path).fileName() : line.mid(sp + 1);
        list << item(name, path, QStringLiteral(""));
    }
    if (list != m_bookmarks) {
        m_bookmarks = list;
        emit bookmarksChanged();
    }
}

void Places::saveBookmarks(const QStringList &lines)
{
    QDir().mkpath(QFileInfo(bookmarksFile()).absolutePath());
    QFile f(bookmarksFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        f.write((lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
    loadBookmarks();
}

bool Places::isBookmarked(const QString &path) const
{
    for (const QVariant &v : m_bookmarks)
        if (v.toMap().value(QStringLiteral("path")).toString() == path)
            return true;
    return false;
}

void Places::addBookmark(const QString &path)
{
    if (isBookmarked(path))
        return;
    QStringList lines = bookmarkLines();
    lines << QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
    saveBookmarks(lines);
}

void Places::removeBookmark(const QString &path)
{
    QStringList lines = bookmarkLines();
    lines.removeIf([&path](const QString &line) {
        const int sp = line.indexOf(QLatin1Char(' '));
        return QUrl(sp < 0 ? line : line.left(sp)).toLocalFile() == path;
    });
    saveBookmarks(lines);
}

void Places::refreshDevices()
{
    QVariantList list;
    list << item(QStringLiteral("Файловая система"), QStringLiteral("/"), QStringLiteral(""));
    for (const QStorageInfo &si : QStorageInfo::mountedVolumes()) {
        const QString root = si.rootPath();
        if (!si.isValid() || !si.isReady())
            continue;
        if (!(root.startsWith(QLatin1String("/run/media/")) || root.startsWith(QLatin1String("/media/"))
              || root.startsWith(QLatin1String("/mnt/"))))
            continue;
        const QString name = si.displayName().isEmpty() || si.displayName() == root ? QFileInfo(root).fileName()
                                                                                    : si.displayName();
        QVariantMap m = item(name, root, QStringLiteral(""));
        m[QStringLiteral("removable")] = true;
        list << m;
    }
    if (list != m_devices) {
        m_devices = list;
        emit devicesChanged();
    }
}
