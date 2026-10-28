#pragma once

#include "FileEntry.h"

#include <QAbstractListModel>
#include <QCollator>
#include <QFileSystemWatcher>
#include <QFuture>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

// Содержимое одной папки или результаты поиска в ней (searchQuery не пуст).
class DirModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QString sortKey READ sortKey WRITE setSortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortDesc READ sortDesc WRITE setSortDesc NOTIFY sortChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString freeSpace READ freeSpace NOTIFY freeSpaceChanged)
    // Открыт корень корзины: вместо типа — «Откуда», вместо даты изменения — дата удаления
    Q_PROPERTY(bool trashRoot READ trashRoot NOTIFY pathChanged)
    // Окно выбора файлов: показывать только подходящие файлы (папки видны всегда)
    Q_PROPERTY(QStringList nameFilters READ nameFilters WRITE setNameFilters NOTIFY filtersChanged)
    Q_PROPERTY(QStringList mimeFilters READ mimeFilters WRITE setMimeFilters NOTIFY filtersChanged)
    Q_PROPERTY(bool dirsOnly READ dirsOnly WRITE setDirsOnly NOTIFY filtersChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        DirRole,
        IsDirRole,
        IsHiddenRole,
        IsLinkRole,
        IsLockedRole,
        MtimeRole,
        SizeRole,
        SizeTextRole,
        DateTextRole,
        MimeTypeRole,
        MimeCommentRole,
        IconRole,
        OriginRole,
    };

    explicit DirModel(QObject *parent = nullptr);
    ~DirModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString path() const { return m_path; }
    void setPath(const QString &path);
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool on);
    QString sortKey() const { return m_sortKey; }
    void setSortKey(const QString &key);
    bool sortDesc() const { return m_sortDesc; }
    void setSortDesc(bool desc);
    QString searchQuery() const { return m_query; }
    void setSearchQuery(const QString &q);
    bool searching() const { return m_searching; }
    int count() const { return int(m_rows.size()); }
    QString error() const { return m_error; }
    QString freeSpace() const;
    bool trashRoot() const;
    QStringList nameFilters() const { return m_nameFilters; }
    void setNameFilters(const QStringList &globs);
    QStringList mimeFilters() const { return m_mimeFilters; }
    void setMimeFilters(const QStringList &mimes);
    bool dirsOnly() const { return m_dirsOnly; }
    void setDirsOnly(bool on);

    Q_INVOKABLE void reload();
    Q_INVOKABLE int indexOfPath(const QString &path) const;
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirAt(int row) const;
    Q_INVOKABLE QString iconAt(int row) const;
    Q_INVOKABLE QString mimeTypeAt(int row) const;
    Q_INVOKABLE QString mimeCommentAt(int row) const;
    Q_INVOKABLE QString sizeOf(const QStringList &paths) const;
    Q_INVOKABLE static QString expandPath(const QString &path);

signals:
    void pathChanged();
    // Другая папка или другой язык
    void freeSpaceChanged();
    void showHiddenChanged();
    void sortChanged();
    void searchQueryChanged();
    void searchingChanged();
    void countChanged();
    void errorChanged();
    // Список перечитан (смена папки, изменения на диске) — QML восстанавливает выделение
    void reloaded();
    void filtersChanged();

private:
    void listDir();
    void applyFilterAndSort();
    void refilter();
    void updateRows(const QList<FileEntry> &rows);
    void startSearch();
    void stopSearch();
    void appendResults(quint64 gen, const QList<FileEntry> &batch);
    void startSniff(const QList<FileEntry> &pending);
    void stopSniff();
    void applySniffed(quint64 gen, const QList<FileEntry> &batch);
    bool useSniffed(FileEntry &e) const;
    void setError(const QString &e);
    bool lessThan(const FileEntry &a, const FileEntry &b) const;
    bool passesFilters(const FileEntry &e) const;

    QString m_path;
    QString m_query;
    QString m_error;
    QString m_sortKey = QStringLiteral("name");
    bool m_sortDesc = false;
    bool m_showHidden = false;
    bool m_searching = false;
    QStringList m_nameFilters;
    QList<QRegularExpression> m_nameRegex;
    QStringList m_mimeFilters;
    bool m_dirsOnly = false;
    // Сменилась папка — следующее перечитывание перестраивает список целиком
    bool m_resetNext = true;

    QList<FileEntry> m_all;   // всё содержимое папки, со скрытыми
    QList<FileEntry> m_rows;  // то, что видно

    QMimeDatabase m_mime;
    QCollator m_collator;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;

    quint64 m_searchGen = 0;
    std::shared_ptr<std::atomic_bool> m_searchCancel;
    QFuture<void> m_searchFuture;

    // Что не успели или не стали определять при листинге (FileEntry::typePending),
    // определяется в фоне; узнанное помним до смены папки, чтобы не определять заново
    QHash<QString, FileEntry> m_sniffed;
    quint64 m_sniffGen = 0;
    std::shared_ptr<std::atomic_bool> m_sniffCancel;
    QFuture<void> m_sniffFuture;
};
