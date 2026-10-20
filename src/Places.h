#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Боковая панель: стандартные папки, закладки GTK (общие с Thunar/Nautilus), диски
class Places : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList places READ places CONSTANT)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(QString trashPath READ trashPath CONSTANT)

public:
    explicit Places(QObject *parent = nullptr);

    QVariantList places() const { return m_places; }
    QVariantList bookmarks() const { return m_bookmarks; }
    QVariantList devices() const { return m_devices; }
    QString trashPath() const;

    Q_INVOKABLE bool isBookmarked(const QString &path) const;
    Q_INVOKABLE void addBookmark(const QString &path);
    Q_INVOKABLE void removeBookmark(const QString &path);
    Q_INVOKABLE void refreshDevices();

signals:
    void bookmarksChanged();
    void devicesChanged();

private:
    void loadBookmarks();
    void saveBookmarks(const QStringList &lines);
    QStringList bookmarkLines() const;

    QVariantList m_places;
    QVariantList m_bookmarks;
    QVariantList m_devices;
    QFileSystemWatcher m_watcher;
    QTimer m_devTimer;
};
