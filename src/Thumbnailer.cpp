#include "Thumbnailer.h"

#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QtEndian>
#include <QStandardPaths>
#include <QUrl>

namespace {
const QString kService = QStringLiteral("org.freedesktop.thumbnails.Thumbnailer1");
const QString kPath = QStringLiteral("/org/freedesktop/thumbnails/Thumbnailer1");
const QString kIface = QStringLiteral("org.freedesktop.thumbnails.Thumbnailer1");
// Размеры миниатюр по спецификации: сторона 128, 256, 512 и 1024 px
const char *const kFlavors[] = {"normal", "large", "x-large", "xx-large"};
constexpr int kFlavorPx[] = {128, 256, 512, 1024};

QString pendingKey(int flavor, const QString &path)
{
    return QString::number(flavor) + QLatin1Char(':') + path;
}

qint64 mtimeOf(const QString &path)
{
    return QFileInfo(path).lastModified().toSecsSinceEpoch();
}

// Поле tEXt из PNG. QImageReader::text() до чтения картинки режет ключ по первому
// двоеточию («Thumb::MTime» → «Thumb»), а читать всю картинку ради поля дорого.
// Текстовые чанки миниатюр лежат до IDAT, так что хватает начала файла.
QByteArray pngText(const QString &file, const QByteArray &key)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray head = f.read(8192);
    if (!head.startsWith("\x89PNG\r\n\x1a\n"))
        return {};
    qsizetype pos = 8;
    while (pos + 8 <= head.size()) {
        const quint32 len = qFromBigEndian<quint32>(head.constData() + pos);
        const QByteArray type = head.mid(pos + 4, 4);
        if (type == "IDAT" || type == "IEND" || pos + 12 + qsizetype(len) > head.size())
            break;
        if (type == "tEXt") {
            const QByteArray data = head.mid(pos + 8, len);
            const qsizetype zero = data.indexOf('\0');
            if (zero > 0 && data.left(zero) == key)
                return data.mid(zero + 1);
        }
        pos += 12 + len;
    }
    return {};
}

// По спецификации MTime — целые секунды, но новые tumbler и GLib пишут
// с долями («1792680195.046882»), поэтому сравниваем целую часть
bool sameMtime(const QString &thumb, qint64 mtime)
{
    bool ok = false;
    const double t = pngText(thumb, "Thumb::MTime").toDouble(&ok);
    return ok && qint64(t) == mtime;
}
}

Thumbnailer::Thumbnailer(QObject *parent)
    : QObject(parent)
    , m_cacheDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                 + QStringLiteral("/thumbnails"))
{
    // Копим заказы, пока сетка создаёт делегаты, и шлём одной пачкой
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(40);
    connect(&m_flushTimer, &QTimer::timeout, this, &Thumbnailer::flush);
    // Ready приходит по одной миниатюре — не дёргаем все биндинги на каждую
    m_bumpTimer.setSingleShot(true);
    m_bumpTimer.setInterval(60);
    connect(&m_bumpTimer, &QTimer::timeout, this, [this] {
        ++m_revision;
        emit revisionChanged();
    });

    const QDir fail(m_cacheDir + QStringLiteral("/fail"));
    for (const QString &app : fail.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        m_failDirs << fail.filePath(app) + QLatin1Char('/');

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(kService, kPath, kIface, QStringLiteral("Ready"), this, SLOT(onReady(uint, QStringList)));
    bus.connect(kService, kPath, kIface, QStringLiteral("Error"), this,
                SLOT(onError(uint, QStringList, int, QString)));
    loadSupported();

    // Старый tumbler знает только normal и large
    const QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kIface, QStringLiteral("GetFlavors"));
    auto *w = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QStringList> reply = *w;
        if (reply.isError())
            return;
        for (int i = 0; i < 4; ++i)
            if (reply.value().contains(QLatin1String(kFlavors[i])))
                m_maxFlavor = std::max(m_maxFlavor, i);
    });
}

int Thumbnailer::flavorFor(int size) const
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    const qreal px = size * (screen ? screen->devicePixelRatio() : 1.0);
    int f = 0;
    while (f < m_maxFlavor && kFlavorPx[f] < px)
        ++f;
    return f;
}

QString Thumbnailer::cacheName(const QString &path)
{
    const QByteArray uri = QUrl::fromLocalFile(path).toEncoded();
    return QString::fromLatin1(QCryptographicHash::hash(uri, QCryptographicHash::Md5).toHex())
           + QStringLiteral(".png");
}

void Thumbnailer::loadSupported()
{
    // tumbler поднимается по запросу (D-Bus activation), поэтому вызов асинхронный
    const QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kIface, QStringLiteral("GetSupported"));
    auto *w = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QStringList, QStringList> reply = *w;
        m_supportedLoaded = true;
        if (reply.isError()) {
            bump(); // tumbler нет — показываем только то, что уже лежит в кэше
            return;
        }
        const QStringList schemes = reply.argumentAt<0>();
        const QStringList mimes = reply.argumentAt<1>();
        for (qsizetype i = 0; i < schemes.size() && i < mimes.size(); ++i) {
            if (schemes[i] == QLatin1String("file"))
                m_supported.insert(mimes[i]);
        }
        bump();
    });
}

QString Thumbnailer::thumbnail(const QString &path, const QString &mimeType, int size)
{
    // Миниатюры самих миниатюр спецификация запрещает
    if (path.isEmpty() || path.startsWith(m_cacheDir))
        return {};

    const qint64 mtime = mtimeOf(path);
    const auto nv = m_never.constFind(path);
    if (nv != m_never.cend() && *nv == mtime)
        return {};
    const int want = flavorFor(size);
    const QString key = pendingKey(want, path);
    const auto it = m_known.constFind(key);
    if (it != m_known.cend() && it->mtime == mtime)
        return it->url;

    const QString name = cacheName(path);
    auto cached = [&](int f) -> QString {
        const QString file = m_cacheDir + QLatin1Char('/') + QLatin1String(kFlavors[f]) + QLatin1Char('/') + name;
        // Устаревшая — файл менялся после того, как её сделали
        if (!QFileInfo::exists(file) || !sameMtime(file, mtime))
            return {};
        // Путь миниатюры у новой версии файла тот же — без ?mtime QML покажет
        // старую картинку из своего кэша
        QUrl u = QUrl::fromLocalFile(file);
        u.setQuery(QString::number(mtime));
        return u.toString();
    };
    // Подходит любой размер не меньше нужного; больший Qt сам ужмёт
    for (int f = want; f < 4; ++f) {
        const QString url = cached(f);
        if (!url.isEmpty()) {
            m_known.insert(key, {mtime, url});
            return url;
        }
    }

    // Кто-то уже пытался и не смог
    for (const QString &dir : std::as_const(m_failDirs)) {
        const QString file = dir + name;
        if (QFileInfo::exists(file) && sameMtime(file, mtime)) {
            m_never.insert(path, mtime);
            return {};
        }
    }

    if (m_supported.contains(mimeType)) {
        if (!m_pending.contains(key)) {
            m_queue[want].insert(path, mimeType);
            m_flushTimer.start();
        }
    } else if (m_supportedLoaded) {
        // Не умеет никто — запоминаем, чтобы не проверять кэш на каждой перерисовке
        m_never.insert(path, mtime);
        return {};
    }
    // Пока tumbler делает крупную — показываем меньшую, если есть
    for (int f = want - 1; f >= 0; --f) {
        const QString url = cached(f);
        if (!url.isEmpty())
            return url;
    }
    return {};
}

void Thumbnailer::flush()
{
    // Заказ на каждый размер отдельный: размер — параметр всего заказа
    for (auto q = m_queue.cbegin(); q != m_queue.cend(); ++q) {
        const int flavor = q.key();
        QStringList uris, mimes;
        for (auto it = q->cbegin(); it != q->cend(); ++it) {
            uris << QString::fromLatin1(QUrl::fromLocalFile(it.key()).toEncoded());
            mimes << it.value();
            m_pending.insert(pendingKey(flavor, it.key()));
        }

        // foreground — tumbler делает их раньше фоновых заказов других программ
        QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kIface, QStringLiteral("Queue"));
        msg << uris << mimes << QString::fromLatin1(kFlavors[flavor]) << QStringLiteral("foreground") << uint(0);
        auto *w = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
        connect(w, &QDBusPendingCallWatcher::finished, this, [this, uris, flavor](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            if (QDBusPendingReply<uint>(*w).isError()) {
                // Сервис пропал — не держим пути в ожидании вечно
                for (const QString &uri : uris)
                    m_pending.remove(pendingKey(flavor, QUrl(uri).toLocalFile()));
            }
        });
    }
    m_queue.clear();
}

void Thumbnailer::onReady(uint, const QStringList &uris)
{
    for (const QString &uri : uris) {
        // Какого размера готова, сигнал не говорит — снимаем ожидание со всех,
        // недостающий размер закажется снова при следующем запросе
        const QString path = QUrl(uri).toLocalFile();
        for (int f = 0; f < 4; ++f)
            m_pending.remove(pendingKey(f, path));
    }
    bump();
}

void Thumbnailer::onError(uint, const QStringList &uris, int, const QString &)
{
    for (const QString &uri : uris) {
        const QString path = QUrl(uri).toLocalFile();
        bool ours = false;
        for (int f = 0; f < 4; ++f)
            ours |= m_pending.remove(pendingKey(f, path));
        if (!ours)
            continue; // чужой заказ
        m_never.insert(path, mtimeOf(path));
    }
}

void Thumbnailer::bump()
{
    m_bumpTimer.start();
}
