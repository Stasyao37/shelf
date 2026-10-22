#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Миниатюры по спецификации freedesktop: общий кэш ~/.cache/thumbnails
// (его же заполняют Thunar, Nautilus, браузеры), недостающие делает tumbler
// по D-Bus. Сам Shelf картинки не уменьшает — только ищет и заказывает.
class Thumbnailer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Растёт, когда появились новые миниатюры: биндинги в QML перечитывают thumbnail()
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit Thumbnailer(QObject *parent = nullptr);

    int revision() const { return m_revision; }

    // URL готовой миниатюры или пустая строка. Если её нет — заказывает у tumbler,
    // и когда он закончит, revision вырастет.
    Q_INVOKABLE QString thumbnail(const QString &path, const QString &mimeType);

    // Имя файла миниатюры: MD5 от file://-URI, как в GLib
    static QString cacheName(const QString &path);

signals:
    void revisionChanged();

private slots:
    void onReady(uint handle, const QStringList &uris);
    void onError(uint handle, const QStringList &uris, int code, const QString &message);

private:
    struct Known {
        qint64 mtime = 0;
        QString url; // пусто — миниатюры нет и не будет (tumbler не смог)
    };

    void loadSupported();
    void flush();
    void bump();

    QString m_cacheDir;
    QStringList m_failDirs; // fail/<программа>/ — у каждой своя
    bool m_supportedLoaded = false;
    QSet<QString> m_supported; // MIME-типы, которые tumbler умеет для file://
    QHash<QString, Known> m_known;
    QHash<QString, QString> m_queue;   // путь → MIME, ждут отправки
    QSet<QString> m_pending;            // отправлены tumbler'у
    QTimer m_flushTimer;
    QTimer m_bumpTimer;
    int m_revision = 0;
};
