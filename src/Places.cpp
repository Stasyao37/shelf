#include "Places.h"

#include "Language.h"

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
    // Как у GTK: с учётом XDG_CONFIG_HOME
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/gtk-3.0/bookmarks");
}

QVariantMap item(const QString &name, const QString &path, const QString &icon)
{
    return {{QStringLiteral("name"), name}, {QStringLiteral("path"), path}, {QStringLiteral("icon"), icon}};
}
}

Places::Places(QObject *parent)
    : QObject(parent)
{
    buildPlaces();
    // Названия мест — на языке интерфейса
    connect(Language::notifier(), &LanguageNotifier::changed, this, [this] {
        buildPlaces();
        refreshDevices();
        emit placesChanged();
    });

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

    // Полная или пустая корзина — для иконки. Следим и за самой Trash: files/ может
    // появиться позже, при первом удалении
    connect(&m_trashWatcher, &QFileSystemWatcher::directoryChanged, this, &Places::updateTrash);
    updateTrash();
}

void Places::buildPlaces()
{
    m_places.clear();
    m_places << item(tr("Домашняя"), QDir::homePath(), QStringLiteral("user-home"));
    auto add = [this](QStandardPaths::StandardLocation loc, const QString &name, const QString &icon) {
        const QString p = QStandardPaths::writableLocation(loc);
        if (!p.isEmpty() && p != QDir::homePath() && QFileInfo(p).isDir())
            m_places << item(name, p, icon);
    };
    add(QStandardPaths::DesktopLocation, tr("Рабочий стол"), QStringLiteral("user-desktop"));
    add(QStandardPaths::DocumentsLocation, tr("Документы"), QStringLiteral("folder-documents"));
    add(QStandardPaths::DownloadLocation, tr("Загрузки"), QStringLiteral("folder-download"));
    add(QStandardPaths::PicturesLocation, tr("Изображения"), QStringLiteral("folder-pictures"));
    add(QStandardPaths::MusicLocation, tr("Музыка"), QStringLiteral("folder-music"));
    add(QStandardPaths::MoviesLocation, tr("Видео"), QStringLiteral("folder-videos"));
    m_places << item(tr("Корзина"), trashPath(), QStringLiteral("trash"));
}

void Places::updateTrash()
{
    const QString files = trashPath();
    const QString root = QFileInfo(files).absolutePath();
    for (const QString &p : {root, files}) {
        if (QFileInfo(p).isDir() && !m_trashWatcher.directories().contains(p))
            m_trashWatcher.addPath(p);
    }
    const bool full = QFileInfo(files).isDir() && !QDir(files).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    if (full != m_trashFull) {
        m_trashFull = full;
        emit trashFullChanged();
    }
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
        list << item(name, path, QStringLiteral("folder"));
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
    list << item(tr("Файловая система"), QStringLiteral("/"), QStringLiteral("drive"));
    for (const QStorageInfo &si : QStorageInfo::mountedVolumes()) {
        const QString root = si.rootPath();
        if (!si.isValid() || !si.isReady())
            continue;
        if (!(root.startsWith(QLatin1String("/run/media/")) || root.startsWith(QLatin1String("/media/"))
              || root.startsWith(QLatin1String("/mnt/"))))
            continue;
        const QString name = si.displayName().isEmpty() || si.displayName() == root ? QFileInfo(root).fileName()
                                                                                    : si.displayName();
        // /run/media и /media — то, что монтирует udisks (флешки), /mnt — обычно разделы дисков
        const bool usb = !root.startsWith(QLatin1String("/mnt/"));
        QVariantMap m = item(name, root, usb ? QStringLiteral("usb") : QStringLiteral("drive"));
        m[QStringLiteral("removable")] = true;
        list << m;
    }
    if (list != m_devices) {
        m_devices = list;
        emit devicesChanged();
    }
}
