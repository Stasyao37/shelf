#include "Environment.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

#include <unistd.h>

namespace {
bool has(const QString &exe)
{
    return !QStandardPaths::findExecutable(exe).isEmpty();
}

QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
}

QString shelfTemplate()
{
    return configDir() + QStringLiteral("/shelf/matugen/shelf-colors.json");
}

QString shelfColors()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/shelf/colors.json");
}

const QString kBlock = QStringLiteral("[templates.shelf]");
}

Environment::Environment(QObject *parent)
    : QObject(parent)
{
    QFile f(QStringLiteral("/etc/os-release"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            auto value = [&line](int from) { return line.mid(from).remove(QLatin1Char('"')).toLower(); };
            if (line.startsWith(QLatin1String("ID=")))
                m_distro = value(3);
            else if (line.startsWith(QLatin1String("ID_LIKE=")))
                m_distroLike = value(8);
        }
    }
}

bool Environment::hyprland() const
{
    return !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE");
}

bool Environment::quickshell() const
{
    // Ищем процесс qs / quickshell среди своих
    for (const QFileInfo &p : QDir(QStringLiteral("/proc")).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool num = false;
        p.fileName().toInt(&num);
        if (!num || p.ownerId() != getuid())
            continue;
        QFile cmd(p.absoluteFilePath() + QStringLiteral("/cmdline"));
        if (!cmd.open(QIODevice::ReadOnly))
            continue;
        const QString exe = QFileInfo(QString::fromLocal8Bit(cmd.readAll().split('\0').value(0))).fileName();
        if (exe == QLatin1String("qs") || exe == QLatin1String("quickshell"))
            return true;
    }
    return false;
}

bool Environment::matugen() const
{
    return has(QStringLiteral("matugen"));
}

QString Environment::matugenConfig()
{
    return configDir() + QStringLiteral("/matugen/config.toml");
}

bool Environment::matugenConnected() const
{
    QFile f(matugenConfig());
    return f.open(QIODevice::ReadOnly) && f.readAll().contains(kBlock.toUtf8());
}

bool Environment::blur() const
{
    if (m_blur < 0) {
        m_blur = 0;
        if (hyprland()) {
            QProcess p;
            p.start(QStringLiteral("hyprctl"), {QStringLiteral("-j"), QStringLiteral("getoption"), QStringLiteral("decoration:blur:enabled")});
            if (p.waitForFinished(1000))
                m_blur = QJsonDocument::fromJson(p.readAllStandardOutput()).object().value(QStringLiteral("int")).toInt() == 1;
        }
    }
    return m_blur == 1;
}

bool Environment::tumbler() const
{
    // tumbler поднимается по запросу — смотрим, знает ли шина, кого запускать
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus)
        return false;
    const QString name = QStringLiteral("org.freedesktop.thumbnails.Thumbnailer1");
    if (bus->isServiceRegistered(name))
        return true;
    QDBusReply<QStringList> names = bus->call(QStringLiteral("ListActivatableNames"));
    return names.isValid() && names.value().contains(name);
}

bool Environment::ffmpegthumbnailer() const
{
    return has(QStringLiteral("ffmpegthumbnailer"));
}

bool Environment::gio() const
{
    return has(QStringLiteral("gio"));
}

bool Environment::xdgUtils() const
{
    return has(QStringLiteral("xdg-open")) && has(QStringLiteral("xdg-mime"));
}

QString Environment::installCommand(const QString &package) const
{
    auto is = [this](const char *id) {
        return m_distro == QLatin1String(id) || m_distroLike.split(QLatin1Char(' ')).contains(QLatin1String(id));
    };
    if (m_distro == QLatin1String("nixos"))
        return package == QLatin1String("tumbler") ? QStringLiteral("services.tumbler.enable = true;")
                                                   : QStringLiteral("environment.systemPackages = [ pkgs.%1 ];").arg(package);
    // matugen есть не во всех репозиториях; cargo работает везде
    if (package == QLatin1String("matugen") && !is("arch"))
        return QStringLiteral("cargo install matugen");
    if (is("fedora") || is("rhel"))
        return QStringLiteral("sudo dnf install %1").arg(package);
    if (is("arch"))
        return QStringLiteral("sudo pacman -S %1").arg(package);
    if (is("debian") || is("ubuntu"))
        return QStringLiteral("sudo apt install %1").arg(package);
    if (is("suse") || is("opensuse"))
        return QStringLiteral("sudo zypper install %1").arg(package);
    return {};
}

QString Environment::connectMatugen()
{
    // 1. Свой шаблон — из ресурсов программы туда, где его увидит matugen
    QDir().mkpath(QFileInfo(shelfTemplate()).absolutePath());
    QFile::remove(shelfTemplate());
    if (!QFile::copy(QStringLiteral(":/qt/qml/Shelf/data/matugen/shelf-colors.json"), shelfTemplate()))
        return QStringLiteral("Не удалось записать шаблон в %1").arg(shelfTemplate());
    QFile::setPermissions(shelfTemplate(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);

    // 2. Блок в config.toml — только если его ещё нет; до первой правки — резервная копия
    const QString cfg = matugenConfig();
    QByteArray text;
    {
        QFile f(cfg);
        if (f.exists()) {
            if (!f.open(QIODevice::ReadOnly))
                return QStringLiteral("Не удалось прочитать %1").arg(cfg);
            text = f.readAll();
        }
    }
    if (text.contains(kBlock.toUtf8())) {
        emit changed();
        return {};
    }
    if (QFile::exists(cfg) && !QFile::exists(cfg + QStringLiteral(".bak-shelf")) && !QFile::copy(cfg, cfg + QStringLiteral(".bak-shelf")))
        return QStringLiteral("Не удалось сделать резервную копию %1").arg(cfg);
    QDir().mkpath(QFileInfo(cfg).absolutePath());
    QFile f(cfg);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return QStringLiteral("Не удалось дописать %1").arg(cfg);
    QByteArray block;
    // Без таблицы [config] matugen не читает файл вовсе
    if (text.trimmed().isEmpty())
        block += "[config]\n";
    else if (!text.endsWith('\n'))
        block += '\n';
    block += "\n# Shelf: палитра для файлового менеджера (добавлено первой настройкой Shelf)\n";
    block += kBlock.toUtf8() + '\n';
    block += "input_path = '" + shelfTemplate().toUtf8() + "'\n";
    block += "output_path = '" + shelfColors().toUtf8() + "'\n";
    f.write(block);
    f.close();
    emit changed();
    return {};
}
