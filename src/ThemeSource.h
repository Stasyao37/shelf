#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Сырые данные темы шелла: палитра matugen (qs_theme.json) и настройки
// стекла (~/.config/quickshell/settings.json). Логика — в Theme.qml,
// как в шелле, чтобы QML переносился без правок.
class ThemeSource : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantMap palette READ palette NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)

public:
    explicit ThemeSource(QObject *parent = nullptr);

    QVariantMap palette() const { return m_palette; }
    QVariantMap settings() const { return m_settings; }

signals:
    void changed();

private:
    void load();
    void rewatch();

    QVariantMap m_palette;
    QVariantMap m_settings;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
