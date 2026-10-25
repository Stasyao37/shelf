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
    // и когда он закончит, revision вырастет. size — сторона значка в логических
    // пикселях: по ней выбирается размер миниатюры (normal 128 … xx-large 1024).
    // Пока нужной нет, отдаёт меньшую из кэша, если она есть.
    Q_INVOKABLE QString thumbnail(const QString &path, const QString &mimeType, int size = 56);

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
        QString url;
    };

    void loadSupported();
    int flavorFor(int size) const;
    void flush();
    void bump();

    QString m_cacheDir;
    QStringList m_failDirs; // fail/<программа>/ — у каждой своя
    bool m_supportedLoaded = false;
    QSet<QString> m_supported; // MIME-типы, которые tumbler умеет для file://
    int m_maxFlavor = 1;                // самый крупный размер, который умеет tumbler
    QHash<QString, Known> m_known;      // ключ — номер размера и путь
    QHash<QString, qint64> m_never;     // путь → mtime: миниатюры нет и не будет
    QHash<int, QHash<QString, QString>> m_queue; // размер → путь → MIME, ждут отправки
    QSet<QString> m_pending;            // отправлены tumbler'у (ключ как у m_known)
    QTimer m_flushTimer;
    QTimer m_bumpTimer;
    int m_revision = 0;
};
