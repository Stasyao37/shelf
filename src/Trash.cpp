#include "Trash.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

namespace Trash {

QString root()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
}

QString filesDir()
{
    return root() + QStringLiteral("/files");
}

bool isTopLevel(const QString &path)
{
    return QFileInfo(path).absolutePath() == filesDir();
}

bool contains(const QString &path)
{
    return QDir::cleanPath(path).startsWith(filesDir() + QLatin1Char('/'));
}

QString infoPath(const QString &path)
{
    return root() + QStringLiteral("/info/") + QFileInfo(path).fileName() + QStringLiteral(".trashinfo");
}

Info readInfo(const QString &path)
{
    Info info;
    QFile f(infoPath(path));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return info;
    QTextStream in(&f);
    bool inGroup = false;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inGroup = line == QLatin1String("[Trash Info]");
            continue;
        }
        if (!inGroup)
            continue;
        if (line.startsWith(QLatin1String("Path=")))
            // Путь в процентной кодировке; в домашней корзине — абсолютный
            info.origPath = QUrl::fromPercentEncoding(line.mid(5).toUtf8());
        else if (line.startsWith(QLatin1String("DeletionDate=")))
            info.deleted = QDateTime::fromString(line.mid(13), Qt::ISODate);
    }
    if (!info.origPath.isEmpty() && !info.origPath.startsWith(QLatin1Char('/')))
        info.origPath = QDir::homePath() + QLatin1Char('/') + info.origPath;
    return info;
}

void forget(const QString &path)
{
    QFile::remove(infoPath(path));

    // directorysizes: «размер mtime имя-в-процентах» на строку, только для папок
    QFile f(root() + QStringLiteral("/directorysizes"));
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;
    const QByteArray name = QUrl::toPercentEncoding(QFileInfo(path).fileName());
    QByteArray kept;
    bool changed = false;
    for (const QByteArray &line : f.readAll().split('\n')) {
        if (line.isEmpty())
            continue;
        if (line.split(' ').value(2) == name) {
            changed = true;
            continue;
        }
        kept += line + '\n';
    }
    f.close();
    if (changed && f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(kept);
}

}
