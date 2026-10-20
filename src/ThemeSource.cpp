#include "ThemeSource.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString paletteFile()
{
    return QDir::homePath() + QStringLiteral("/.local/state/quickshell/user/generated/qs_theme.json");
}

QString settingsFile()
{
    return QDir::homePath() + QStringLiteral("/.config/quickshell/settings.json");
}

QVariantMap readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
}
}

ThemeSource::ThemeSource(QObject *parent)
    : QObject(parent)
{
    // matugen пишет файл не одним куском — читаем, когда запись утихла
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this, &ThemeSource::load);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    load();
}

void ThemeSource::rewatch()
{
    // Файл, заменённый через rename, выпадает из наблюдения — добавляем заново
    for (const QString &f : {paletteFile(), settingsFile()}) {
        if (QFileInfo::exists(f) && !m_watcher.files().contains(f))
            m_watcher.addPath(f);
        const QString dir = QFileInfo(f).absolutePath();
        if (QFileInfo::exists(dir) && !m_watcher.directories().contains(dir))
            m_watcher.addPath(dir);
    }
}

void ThemeSource::load()
{
    rewatch();
    const QVariantMap palette = readJson(paletteFile());
    const QVariantMap settings = readJson(settingsFile());
    // Пустой результат — файл в процессе записи; держим старые цвета
    if ((palette.isEmpty() || palette == m_palette) && (settings.isEmpty() || settings == m_settings))
        return;
    if (!palette.isEmpty())
        m_palette = palette;
    if (!settings.isEmpty())
        m_settings = settings;
    emit changed();
}
