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
#include <QSettings>
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

// ── Окна выбора файлов через портал ─────────────────────────────────
namespace {
const QString kPortalKey = QStringLiteral("org.freedesktop.impl.portal.FileChooser");

QString desktopName()
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':')).value(0).toLower();
}

QString portalsDir()
{
    // NixOS и другие кладут описания бэкендов в свой каталог и сообщают о нём так
    const QString env = qEnvironmentVariable("XDG_DESKTOP_PORTAL_DIR");
    return env.isEmpty() ? QStringLiteral("/usr/share/xdg-desktop-portal/portals") : env;
}

QString userPortalsConf()
{
    const QString de = desktopName();
    return configDir() + QStringLiteral("/xdg-desktop-portal/") + (de.isEmpty() ? QString() : de + QLatin1Char('-'))
           + QStringLiteral("portals.conf");
}

QString hyprSnippet()
{
    return configDir() + QStringLiteral("/hypr/shelf.conf");
}

QString hyprConf()
{
    return configDir() + QStringLiteral("/hypr/hyprland.conf");
}

const QString kSourceLine = QStringLiteral("source = ~/.config/hypr/shelf.conf");

QString readText(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()) : QString();
}

bool writeText(const QString &path, const QString &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    f.write(text.toUtf8());
    return true;
}

void backupOnce(const QString &path)
{
    if (QFile::exists(path) && !QFile::exists(path + QStringLiteral(".bak-shelf")))
        QFile::copy(path, path + QStringLiteral(".bak-shelf"));
}

// Значение ключа в [preferred] или пусто
QString preferredValue(const QString &text, const QString &key)
{
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (t.startsWith(key) && t.mid(key.size()).trimmed().startsWith(QLatin1Char('=')))
            return t.section(QLatin1Char('='), 1).trimmed();
    }
    return {};
}

// Ставит ключ в [preferred] (value пусто — убирает). Строка меняется там же, где стояла,
// и в том же оформлении («ключ = значение» или «ключ=значение»): выключение возвращает файл как был
QString withPreferred(const QString &text, const QString &key, const QString &value)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    int found = -1;
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QString t = lines[i].trimmed();
        if (!t.startsWith(key) || !t.mid(key.size()).trimmed().startsWith(QLatin1Char('=')))
            continue;
        if (found >= 0)
            lines.removeAt(found); // дубли — оставляем первый
        found = i;
    }
    if (found >= 0) {
        if (value.isEmpty()) {
            lines.removeAt(found);
        } else {
            const QString sep = lines[found].contains(QLatin1String(" = ")) ? QStringLiteral(" = ") : QStringLiteral("=");
            lines[found] = key + sep + value;
        }
    } else if (!value.isEmpty()) {
        int at = lines.indexOf(QStringLiteral("[preferred]"));
        if (at < 0) {
            lines.prepend(QStringLiteral("[preferred]"));
            at = 0;
        }
        // Оформление — как у соседних строк файла
        const bool spaced = text.contains(QLatin1String(" = "));
        lines.insert(at + 1, key + (spaced ? QStringLiteral(" = ") : QStringLiteral("=")) + value);
    }
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void run(const QString &exe, const QStringList &args)
{
    QProcess p;
    p.start(exe, args);
    p.waitForFinished(5000);
}
}

bool Environment::portalBackend() const
{
    return QFile::exists(portalsDir() + QStringLiteral("/shelf.portal"));
}

bool Environment::portalPicker() const
{
    return preferredValue(readText(userPortalsConf()), kPortalKey) == QLatin1String("shelf");
}

QString Environment::portalInstallCommand() const
{
    const QString src = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                        + QStringLiteral("/xdg-desktop-portal/portals/shelf.portal");
    return QStringLiteral("sudo install -Dm644 %1 /usr/share/xdg-desktop-portal/portals/shelf.portal").arg(src);
}

QString Environment::setPortalPicker(bool on)
{
    if (on && !portalBackend())
        return QStringLiteral("Бэкенд портала не установлен — %1").arg(portalInstallCommand());

    // 1. Портал: FileChooser=shelf. Своего файла нет — берём системный как основу,
    //    иначе пользовательский файл заменил бы его целиком
    const QString conf = userPortalsConf();
    QString text = readText(conf);
    if (text.isEmpty())
        text = readText(QStringLiteral("/usr/share/xdg-desktop-portal/") + QFileInfo(conf).fileName());
    if (text.isEmpty())
        text = QStringLiteral("[preferred]\ndefault=gtk\n");
    QSettings st;
    if (on) {
        const QString prev = preferredValue(text, kPortalKey);
        if (prev != QLatin1String("shelf"))
            st.setValue(QStringLiteral("portal/previousFileChooser"), prev);
        backupOnce(conf);
        text = withPreferred(text, kPortalKey, QStringLiteral("shelf"));
    } else {
        text = withPreferred(text, kPortalKey, st.value(QStringLiteral("portal/previousFileChooser")).toString());
        st.remove(QStringLiteral("portal/previousFileChooser"));
    }
    if (!writeText(conf, text))
        return QStringLiteral("Не удалось записать %1").arg(conf);

    // 2. Hyprland: GTK-программам — портал, окну выбора — плавающее по центру
    if (hyprland() && QFile::exists(hyprConf())) {
        QString hc = readText(hyprConf());
        const bool hasSource = hc.contains(kSourceLine);
        if (on) {
            const QString snippet = QStringLiteral(
                "# Shelf: окна выбора файлов (Настройки Shelf → Проводник). Файл перезаписывается Shelf.\n"
                "# GTK 3 и Firefox — выбор файлов через портал; GTK 4 — тоже\n"
                "env = GTK_USE_PORTAL,1\n"
                "env = GDK_DEBUG,portals\n"
                "windowrulev2 = float, class:^(org.stasyao.shelf.picker)$\n"
                "windowrulev2 = size 940 600, class:^(org.stasyao.shelf.picker)$\n"
                "windowrulev2 = center, class:^(org.stasyao.shelf.picker)$\n");
            if (!writeText(hyprSnippet(), snippet))
                return QStringLiteral("Не удалось записать %1").arg(hyprSnippet());
            if (!hasSource) {
                backupOnce(hyprConf());
                if (!hc.endsWith(QLatin1Char('\n')))
                    hc += QLatin1Char('\n');
                hc += QStringLiteral("\n# Shelf: окна выбора файлов\n") + kSourceLine + QLatin1Char('\n');
                writeText(hyprConf(), hc);
            }
        } else {
            // Сначала строку source, потом файл — иначе Hyprland ругнётся на пропавший файл
            if (hasSource) {
                hc.remove(QStringLiteral("\n# Shelf: окна выбора файлов\n") + kSourceLine + QLatin1Char('\n'));
                hc.remove(kSourceLine + QLatin1Char('\n'));
                writeText(hyprConf(), hc);
            }
            QFile::remove(hyprSnippet());
        }
        run(QStringLiteral("hyprctl"), {QStringLiteral("reload")});
    }

    // 3. Портал перечитывает настройки только при запуске
    run(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("restart"), QStringLiteral("xdg-desktop-portal.service")});
    emit changed();
    return {};
}

