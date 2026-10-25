#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Приложения из .desktop-файлов и связи «тип файла → приложение» из mimeapps.list —
// для «Открыть с помощью». Запуск через gio launch, как у Thunar и Nautilus;
// консольные (Terminal=true) — сами, через Terminal: gio знает не все терминалы.
class Apps : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Shelf открывает папки по умолчанию (inode/directory → io.github.nook_apps.Shelf.desktop)
    Q_PROPERTY(bool defaultFileManager READ defaultFileManager NOTIFY defaultsChanged)

public:
    explicit Apps(QObject *parent = nullptr);

    // Подходящие для типа: сначала приложение по умолчанию, затем назначенные
    // пользователем, затем заявившие этот тип, затем — родительские типы (text/plain для кода).
    // Элемент: { id, name, comment, icon, isDefault }
    Q_INVOKABLE QVariantList appsFor(const QString &mimeType);
    // Все видимые в меню приложения по алфавиту
    Q_INVOKABLE QVariantList allApps();
    Q_INVOKABLE QString defaultName(const QString &mimeType);
    Q_INVOKABLE bool launch(const QString &appId, const QStringList &paths);
    // Записывает в ~/.config/mimeapps.list, как xdg-mime default
    Q_INVOKABLE bool setDefault(const QString &appId, const QString &mimeType);

    bool defaultFileManager();
    // Сделать Shelf проводником по умолчанию; true — получилось
    Q_INVOKABLE bool makeDefaultFileManager();

signals:
    void defaultsChanged();

private:
    struct App {
        QString id;
        QString file;
        QString name;
        QString comment;
        QString icon;
        QString exec;
        QStringList mimeTypes;
        bool noDisplay = false;
        bool terminal = false;
    };
    struct Assoc {
        QHash<QString, QStringList> defaults;
        QHash<QString, QStringList> added;
        QHash<QString, QStringList> removed;
    };

    void scan();
    Assoc readAssociations() const;
    QString defaultFor(const QString &mime, const Assoc &assoc) const;
    static QVariantMap toMap(const App &a, bool isDefault);
    static QList<QStringList> commandLines(const App &a, const QStringList &paths);

    QList<App> m_apps;
    QHash<QString, int> m_byId;
    qint64 m_scannedAt = 0;
};
