#include "Session.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
// Жив ли процесс и это всё ещё Shelf (PID мог достаться другой программе)
bool shelfAlive(qint64 pid)
{
    QFile f(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray exe = f.readAll().split('\0').value(0);
    return QFileInfo(QString::fromLocal8Bit(exe)).fileName() == QLatin1String("shelf");
}

constexpr qint64 kMaxAgeDays = 7;
}

Session::Session(QObject *parent)
    : QObject(parent)
{
}

QString Session::dir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/shelf/sessions");
}

QString Session::bootId()
{
    QFile f(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    return f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll().trimmed()) : QString();
}

QString Session::ownFile() const
{
    return dir() + QStringLiteral("/%1.json").arg(QCoreApplication::applicationPid());
}

void Session::save(const QStringList &paths, int current)
{
    if (!QDir().mkpath(dir()))
        return;
    const QJsonObject o{{QStringLiteral("boot"), bootId()},
                        {QStringLiteral("pid"), QCoreApplication::applicationPid()},
                        {QStringLiteral("saved"), QDateTime::currentSecsSinceEpoch()},
                        {QStringLiteral("current"), current},
                        {QStringLiteral("paths"), QJsonArray::fromStringList(paths)}};
    // QSaveFile: выключение посреди записи не оставит обрезанный файл
    QSaveFile f(ownFile());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

void Session::clear()
{
    QFile::remove(ownFile());
}

QVariantMap Session::takeOrphan()
{
    const QString boot = bootId();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QString bestFile;
    QJsonObject best;
    for (const QFileInfo &fi : QDir(dir()).entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        const qint64 pid = o.value(QStringLiteral("pid")).toInteger();
        if (pid == QCoreApplication::applicationPid())
            continue;
        // Окно этой загрузки ещё открыто — не трогаем
        if (o.value(QStringLiteral("boot")).toString() == boot && shelfAlive(pid))
            continue;
        const qint64 saved = o.value(QStringLiteral("saved")).toInteger();
        if (o.isEmpty() || now - saved > kMaxAgeDays * 86400) {
            QFile::remove(fi.absoluteFilePath()); // битый или слишком старый
            continue;
        }
        if (bestFile.isEmpty() || saved > best.value(QStringLiteral("saved")).toInteger()) {
            bestFile = fi.absoluteFilePath();
            best = o;
        }
    }
    if (bestFile.isEmpty())
        return {};
    QFile::remove(bestFile);

    // Папки могли удалить или отмонтировать, пока Shelf был закрыт
    const QJsonArray all = best.value(QStringLiteral("paths")).toArray();
    const int current = best.value(QStringLiteral("current")).toInt();
    QStringList paths;
    int newCurrent = 0;
    for (int i = 0; i < all.size(); ++i) {
        const QString p = all.at(i).toString();
        if (!QFileInfo(p).isDir())
            continue;
        if (i == current)
            newCurrent = int(paths.size());
        paths << p;
    }
    if (paths.isEmpty())
        return {};
    return {{QStringLiteral("paths"), paths}, {QStringLiteral("current"), newCurrent}};
}
