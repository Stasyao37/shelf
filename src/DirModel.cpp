#include "DirModel.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QQueue>
#include <QStorageInfo>
#include <QtConcurrent>

#include <algorithm>

namespace {
// Как в quickshell/scripts/spotlight_search.py: в этих папках искать бессмысленно
bool skipInSearch(const QString &name, const QString &path)
{
    static const QStringList names = {QStringLiteral("node_modules"), QStringLiteral("__pycache__"),
                                      QStringLiteral(".git")};
    static const QStringList paths = {QStringLiteral("/proc"), QStringLiteral("/sys"), QStringLiteral("/dev"),
                                      QStringLiteral("/run")};
    if (names.contains(name) || paths.contains(path))
        return true;
    return path.endsWith(QLatin1String("/target/debug")) || path.endsWith(QLatin1String("/target/release"));
}

constexpr int kSearchLimit = 3000;
}

DirModel::DirModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);

    // Копирование большой папки шлёт сотни событий — перечитываем не чаще раза в 200 мс
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(200);
    connect(&m_reloadTimer, &QTimer::timeout, this, [this] {
        if (m_query.isEmpty())
            listDir();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_reloadTimer.start(); });
}

DirModel::~DirModel()
{
    stopSearch();
}

int DirModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant DirModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const FileEntry &e = m_rows.at(index.row());
    switch (role) {
    case NameRole: return e.name;
    case PathRole: return e.path;
    case DirRole: return e.dir;
    case IsDirRole: return e.isDir;
    case IsHiddenRole: return e.isHidden;
    case IsLinkRole: return e.isLink;
    case IsLockedRole: return e.isLocked;
    case SizeRole: return e.size;
    case SizeTextRole:
        if (!e.isDir)
            return formatSize(e.size);
        return e.childCount < 0 ? QString() : QStringLiteral("%1 эл.").arg(e.childCount);
    case DateTextRole: return formatDate(e.mtime);
    case MimeTypeRole: return e.mimeType;
    case MimeCommentRole: return e.mimeComment;
    case IconRole: return e.icon;
    }
    return {};
}

QHash<int, QByteArray> DirModel::roleNames() const
{
    return {
        {NameRole, "name"},         {PathRole, "path"},
        {DirRole, "dir"},           {IsDirRole, "isDir"},
        {IsHiddenRole, "isHidden"}, {IsLinkRole, "isLink"},
        {IsLockedRole, "isLocked"},
        {SizeRole, "size"},         {SizeTextRole, "sizeText"},
        {DateTextRole, "dateText"}, {MimeTypeRole, "mimeType"},
        {MimeCommentRole, "mimeComment"}, {IconRole, "icon"},
    };
}

QString DirModel::expandPath(const QString &path)
{
    QString p = path.trimmed();
    if (p.startsWith(QLatin1String("file://")))
        p = QUrl(p).toLocalFile();
    if (p == QLatin1String("~") || p.startsWith(QLatin1String("~/")))
        p = QDir::homePath() + p.mid(1);
    return QDir::cleanPath(QDir(p).absolutePath());
}

void DirModel::setPath(const QString &path)
{
    const QString p = expandPath(path);
    const QFileInfo fi(p);
    if (!fi.isDir()) {
        setError(QStringLiteral("Нет такой папки: %1").arg(p));
        return;
    }
    if (!fi.isReadable()) {
        setError(QStringLiteral("Нет доступа: %1").arg(p));
        return;
    }
    if (p == m_path && m_query.isEmpty())
        return;

    if (!m_path.isEmpty())
        m_watcher.removePath(m_path);
    m_path = p;
    m_watcher.addPath(m_path);

    if (!m_query.isEmpty()) {
        stopSearch();
        m_query.clear();
        emit searchQueryChanged();
    }
    setError({});
    emit pathChanged();
    listDir();
}

void DirModel::setShowHidden(bool on)
{
    if (m_showHidden == on)
        return;
    m_showHidden = on;
    emit showHiddenChanged();
    if (m_query.isEmpty())
        applyFilterAndSort();
    else
        startSearch();
}

void DirModel::setSortKey(const QString &key)
{
    if (m_sortKey == key)
        return;
    m_sortKey = key;
    emit sortChanged();
    applyFilterAndSort();
}

void DirModel::setSortDesc(bool desc)
{
    if (m_sortDesc == desc)
        return;
    m_sortDesc = desc;
    emit sortChanged();
    applyFilterAndSort();
}

void DirModel::setSearchQuery(const QString &q)
{
    const QString query = q.trimmed();
    if (query == m_query)
        return;
    m_query = query;
    emit searchQueryChanged();
    if (m_query.isEmpty()) {
        stopSearch();
        listDir();
    } else {
        startSearch();
    }
}

void DirModel::reload()
{
    if (m_query.isEmpty())
        listDir();
    else
        startSearch();
}

void DirModel::listDir()
{
    if (m_path.isEmpty())
        return;
    const QDir dir(m_path);
    const QFileInfoList infos = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    m_all.clear();
    m_all.reserve(infos.size());
    for (const QFileInfo &fi : infos)
        m_all.append(FileEntry::fromInfo(fi, m_mime, true));
    applyFilterAndSort();
}

bool DirModel::lessThan(const FileEntry &a, const FileEntry &b) const
{
    if (a.isDir != b.isDir)
        return a.isDir; // папки всегда сверху, независимо от направления
    int c = 0;
    if (m_sortKey == QLatin1String("size"))
        c = a.isDir ? (a.childCount - b.childCount) : (a.size < b.size ? -1 : a.size > b.size ? 1 : 0);
    else if (m_sortKey == QLatin1String("date"))
        c = a.mtime < b.mtime ? -1 : a.mtime > b.mtime ? 1 : 0;
    else if (m_sortKey == QLatin1String("type"))
        c = m_collator.compare(a.mimeComment, b.mimeComment);
    if (c == 0)
        c = m_collator.compare(a.name, b.name);
    return m_sortDesc ? c > 0 : c < 0;
}

void DirModel::applyFilterAndSort()
{
    if (!m_query.isEmpty())
        return;
    QList<FileEntry> rows;
    for (const FileEntry &e : std::as_const(m_all))
        if (m_showHidden || !e.isHidden)
            rows.append(e);
    std::stable_sort(rows.begin(), rows.end(),
                     [this](const FileEntry &a, const FileEntry &b) { return lessThan(a, b); });
    updateRows(rows);
    emit countChanged();
    emit reloaded();
}

// Изменения на диске применяем точечно: при сбросе модели QML пересоздаёт
// все строки, и поле переименования (или прокрутка) пропадает
void DirModel::updateRows(const QList<FileEntry> &rows)
{
    QHash<QString, int> newIndex;
    newIndex.reserve(rows.size());
    for (int i = 0; i < rows.size(); ++i)
        newIndex.insert(rows.at(i).path, i);

    // Оставшиеся строки должны идти в том же порядке, иначе (сменилась
    // сортировка, файл переехал по дате) проще сбросить
    bool sameOrder = true;
    int last = -1;
    int kept = 0;
    for (const FileEntry &e : std::as_const(m_rows)) {
        const auto it = newIndex.constFind(e.path);
        if (it == newIndex.cend())
            continue;
        ++kept;
        if (*it <= last) {
            sameOrder = false;
            break;
        }
        last = *it;
    }
    // Много изменений (другая папка, конец поиска) — по одной строке дольше, чем сбросом
    const int changes = int(m_rows.size()) - kept + int(rows.size()) - kept;
    if (!sameOrder || m_rows.isEmpty() || changes > 64) {
        beginResetModel();
        m_rows = rows;
        endResetModel();
        return;
    }

    for (int i = int(m_rows.size()) - 1; i >= 0; --i) {
        if (newIndex.contains(m_rows.at(i).path))
            continue;
        beginRemoveRows({}, i, i);
        m_rows.removeAt(i);
        endRemoveRows();
    }
    for (int j = 0; j < rows.size(); ++j) {
        if (j < m_rows.size() && m_rows.at(j).path == rows.at(j).path) {
            m_rows[j] = rows.at(j);
            continue;
        }
        beginInsertRows({}, j, j);
        m_rows.insert(j, rows.at(j));
        endInsertRows();
    }
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(int(m_rows.size()) - 1));
}

void DirModel::stopSearch()
{
    ++m_searchGen;
    if (m_searchCancel)
        m_searchCancel->store(true);
    if (m_searchFuture.isRunning())
        m_searchFuture.waitForFinished();
    if (m_searching) {
        m_searching = false;
        emit searchingChanged();
    }
}

void DirModel::startSearch()
{
    stopSearch();
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();

    const quint64 gen = m_searchGen;
    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_searchCancel = cancel;
    m_searching = true;
    emit searchingChanged();

    const QString root = m_path;
    const QString query = m_query;
    const bool hidden = m_showHidden;

    // Обход в ширину: сначала ближние совпадения, глубокие — потом
    m_searchFuture = QtConcurrent::run([this, gen, cancel, root, query, hidden] {
        QMimeDatabase db;
        QList<FileEntry> batch;
        QElapsedTimer flushTimer;
        flushTimer.start();
        int found = 0;
        QQueue<QString> queue;
        queue.enqueue(root);

        auto flush = [&] {
            if (batch.isEmpty())
                return;
            QMetaObject::invokeMethod(this, [this, gen, b = std::move(batch)] { appendResults(gen, b); },
                                      Qt::QueuedConnection);
            batch.clear();
            flushTimer.restart();
        };

        while (!queue.isEmpty() && !cancel->load() && found < kSearchLimit) {
            const QString dir = queue.dequeue();
            QDirIterator it(dir, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            while (it.hasNext() && !cancel->load()) {
                const QFileInfo fi = it.nextFileInfo();
                const QString name = fi.fileName();
                if (!hidden && name.startsWith(QLatin1Char('.')))
                    continue;
                if (fi.isDir() && !fi.isSymLink() && !skipInSearch(name, fi.absoluteFilePath()))
                    queue.enqueue(fi.absoluteFilePath());
                if (name.contains(query, Qt::CaseInsensitive)) {
                    batch.append(FileEntry::fromInfo(fi, db, false));
                    if (++found >= kSearchLimit)
                        break;
                }
                if (flushTimer.elapsed() > 120)
                    flush();
            }
        }
        flush();
        QMetaObject::invokeMethod(this, [this, gen] {
            if (gen != m_searchGen || !m_searching)
                return;
            m_searching = false;
            emit searchingChanged();
        }, Qt::QueuedConnection);
    });
}

void DirModel::appendResults(quint64 gen, const QList<FileEntry> &batch)
{
    if (gen != m_searchGen)
        return;
    const int first = int(m_rows.size());
    beginInsertRows({}, first, first + int(batch.size()) - 1);
    m_rows.append(batch);
    endInsertRows();
    emit countChanged();
}

void DirModel::setError(const QString &e)
{
    if (m_error == e)
        return;
    m_error = e;
    emit errorChanged();
}

int DirModel::indexOfPath(const QString &path) const
{
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows.at(i).path == path)
            return i;
    return -1;
}

QString DirModel::pathAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).path : QString();
}

bool DirModel::isDirAt(int row) const
{
    return row >= 0 && row < m_rows.size() && m_rows.at(row).isDir;
}

QString DirModel::iconAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).icon : QString();
}

QString DirModel::sizeOf(const QStringList &paths) const
{
    qint64 total = 0;
    bool onlyFiles = true;
    for (const QString &p : paths) {
        const int i = indexOfPath(p);
        if (i < 0)
            continue;
        if (m_rows.at(i).isDir)
            onlyFiles = false;
        else
            total += m_rows.at(i).size;
    }
    return onlyFiles && total > 0 ? formatSize(total) : QString();
}

QString DirModel::freeSpace() const
{
    const QStorageInfo si(m_path);
    if (!si.isValid())
        return {};
    return QStringLiteral("свободно %1").arg(formatSize(si.bytesAvailable()));
}
