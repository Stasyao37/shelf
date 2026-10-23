#include "ThemeSource.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace {
const QString kPortalService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kPortalPath = QStringLiteral("/org/freedesktop/portal/desktop");
const QString kPortalSettings = QStringLiteral("org.freedesktop.portal.Settings");

QString stateDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
}

QString qsThemeFile()
{
    return stateDir() + QStringLiteral("/quickshell/user/generated/qs_theme.json");
}

QString materialFile()
{
    return stateDir() + QStringLiteral("/quickshell/user/generated/colors.json");
}

QString shelfColorsFile()
{
    return stateDir() + QStringLiteral("/shelf/colors.json");
}

QString qsSettingsFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/quickshell/settings.json");
}

QString fileOf(const QString &id)
{
    if (id == QLatin1String("qs_theme"))
        return qsThemeFile();
    if (id == QLatin1String("matugen"))
        return shelfColorsFile();
    if (id == QLatin1String("material"))
        return materialFile();
    return {};
}

QVariantMap readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object().toVariantMap();
}

// Встроенные палитры. Тёмная — те же значения, что запасные в Theme.qml
QVariantMap builtin(bool dark)
{
    if (dark)
        return {{"base", "#192030"}, {"mantle", "#131928"}, {"crust", "#0d1220"},
                {"surface0", "#223044"}, {"surface1", "#2e3e54"}, {"surface2", "#3c5068"},
                {"overlay0", "#506078"}, {"overlay1", "#667890"}, {"overlay2", "#7e90aa"},
                {"subtext0", "#98a8c0"}, {"subtext1", "#b0bcd4"}, {"text", "#ccd4e8"},
                {"lavender", "#8ab2e8"}, {"blue", "#6aaee0"}, {"teal", "#5baab0"}};
    // Светлая — холодный серо-голубой, акцент темнее, чтобы читался на светлом стекле
    return {{"base", "#eef1f7"}, {"mantle", "#e6e9f1"}, {"crust", "#dde1ea"},
            {"surface0", "#e2e6ef"}, {"surface1", "#d8dde8"}, {"surface2", "#ccd2df"},
            {"overlay0", "#a3acbd"}, {"overlay1", "#7f8aa0"}, {"overlay2", "#5e6880"},
            {"subtext0", "#4f5970"}, {"subtext1", "#3a4359"}, {"text", "#262d40"},
            {"lavender", "#4a63c4"}, {"blue", "#3d78b8"}, {"teal", "#2f8a8f"}};
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

    readSystemScheme();
    QDBusConnection::sessionBus().connect(kPortalService, kPortalPath, kPortalSettings, QStringLiteral("SettingChanged"),
                                          this, SLOT(portalSettingChanged(QString, QString, QDBusVariant)));
    load();
}

QVariantMap ThemeSource::fromMaterialRoles(const QVariantMap &roles)
{
    // Та же таблица, что в шаблоне matugen для qs_theme.json
    static const std::pair<const char *, const char *> map[] = {
        {"base", "surface"}, {"mantle", "surface_container_low"}, {"crust", "surface_container_lowest"},
        {"surface0", "surface_container"}, {"surface1", "surface_container_high"},
        {"surface2", "surface_container_highest"}, {"overlay0", "outline_variant"}, {"overlay1", "outline"},
        {"overlay2", "on_surface_variant"}, {"subtext0", "on_surface_variant"}, {"subtext1", "on_surface"},
        {"text", "on_surface"}, {"lavender", "primary"}, {"blue", "secondary"}, {"teal", "tertiary"},
        {"mauve", "primary_fixed_dim"}, {"sapphire", "secondary_fixed_dim"}, {"sky", "tertiary_fixed_dim"},
        {"pink", "primary_fixed"}, {"flamingo", "secondary_fixed"}, {"rosewater", "tertiary_fixed"},
        {"onPrimary", "on_primary"}, {"primaryContainer", "primary_container"},
        {"onPrimaryContainer", "on_primary_container"}, {"secondaryContainer", "secondary_container"},
        {"onSecondaryContainer", "on_secondary_container"}, {"onTertiary", "on_tertiary"},
        {"tertiaryContainer", "tertiary_container"}, {"onTertiaryContainer", "on_tertiary_container"},
    };
    QVariantMap out;
    for (const auto &[name, role] : map) {
        QString v = roles.value(QLatin1String(role)).toString().trimmed();
        if (v.isEmpty())
            continue;
        if (!v.startsWith(QLatin1Char('#')))
            v.prepend(QLatin1Char('#')); // некоторые шаблоны пишут цвета без решётки
        out.insert(QLatin1String(name), v);
    }
    return out;
}

bool ThemeSource::fileUsable(const QString &id) const
{
    const QString f = fileOf(id);
    if (f.isEmpty() || !QFileInfo::exists(f))
        return false;
    const QVariantMap m = readJson(f);
    return m.contains(id == QLatin1String("qs_theme") ? QStringLiteral("base") : QStringLiteral("surface"));
}

QString ThemeSource::sourceSetting() const
{
    return QSettings().value(QStringLiteral("theme/source"), QStringLiteral("auto")).toString();
}

void ThemeSource::setSourceSetting(const QString &id)
{
    if (id == sourceSetting())
        return;
    QSettings().setValue(QStringLiteral("theme/source"), id);
    load();
}

QVariantList ThemeSource::sources() const
{
    const std::pair<const char *, QString> list[] = {
        {"auto", QStringLiteral("Автоматически")},
        {"qs_theme", QStringLiteral("Как у Quickshell")},
        {"matugen", QStringLiteral("matugen — шаблон Shelf")},
        {"material", QStringLiteral("Material из шелла (colors.json)")},
        {"system", QStringLiteral("Как в системе")},
        {"light", QStringLiteral("Светлая")},
        {"dark", QStringLiteral("Тёмная")},
    };
    QVariantList res;
    for (const auto &[id, name] : list) {
        const QString sid = QLatin1String(id);
        const bool needsFile = !fileOf(sid).isEmpty();
        res << QVariantMap{{QStringLiteral("id"), sid}, {QStringLiteral("name"), name},
                           {QStringLiteral("available"), !needsFile || fileUsable(sid)}};
    }
    return res;
}

QVariantMap ThemeSource::paletteFor(const QString &id) const
{
    if (id == QLatin1String("qs_theme"))
        return readJson(qsThemeFile());
    if (id == QLatin1String("matugen") || id == QLatin1String("material"))
        return fromMaterialRoles(readJson(fileOf(id)));
    if (id == QLatin1String("light"))
        return builtin(false);
    if (id == QLatin1String("dark"))
        return builtin(true);
    return builtin(m_systemDark);
}

QVariantMap ThemeSource::glassSettings()
{
    // Основа — настройки стекла шелла, как раньше; своих нет — по размытию Hyprland
    QVariantMap s;
    if (QFileInfo::exists(qsSettingsFile())) {
        s = readJson(qsSettingsFile());
        if (s.isEmpty())
            return m_settings; // файл в процессе записи
    } else {
        if (m_hyprBlur < 0) {
            m_hyprBlur = 0;
            if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
                QProcess p;
                p.start(QStringLiteral("hyprctl"), {QStringLiteral("-j"), QStringLiteral("getoption"),
                                                   QStringLiteral("decoration:blur:enabled")});
                if (p.waitForFinished(1000))
                    m_hyprBlur = QJsonDocument::fromJson(p.readAllStandardOutput()).object().value(QStringLiteral("int")).toInt();
            }
        }
        s[QStringLiteral("glass")] = m_hyprBlur == 1;
    }
    const QSettings own;
    if (own.contains(QStringLiteral("theme/glass")))
        s[QStringLiteral("glass")] = own.value(QStringLiteral("theme/glass")).toBool();
    if (own.contains(QStringLiteral("theme/glassAlpha")))
        s[QStringLiteral("glassAlpha")] = own.value(QStringLiteral("theme/glassAlpha")).toDouble();
    return s;
}

void ThemeSource::setGlass(bool on)
{
    QSettings().setValue(QStringLiteral("theme/glass"), on);
    load();
}

void ThemeSource::setGlassAlpha(double alpha)
{
    QSettings().setValue(QStringLiteral("theme/glassAlpha"), alpha);
    load();
}

void ThemeSource::resetGlass()
{
    QSettings own;
    own.remove(QStringLiteral("theme/glass"));
    own.remove(QStringLiteral("theme/glassAlpha"));
    load();
}

void ThemeSource::readSystemScheme()
{
    QDBusInterface portal(kPortalService, kPortalPath, kPortalSettings);
    portal.setTimeout(500);
    // 1 — тёмная, 2 — светлая, 0 — без предпочтения (берём тёмную, как у шелла)
    QDBusReply<QDBusVariant> one = portal.call(QStringLiteral("ReadOne"), QStringLiteral("org.freedesktop.appearance"),
                                               QStringLiteral("color-scheme"));
    uint scheme = 0;
    if (one.isValid()) {
        scheme = one.value().variant().toUInt();
    } else {
        // Старые порталы: Read и значение, завёрнутое в ещё один variant
        QDBusReply<QDBusVariant> old = portal.call(QStringLiteral("Read"), QStringLiteral("org.freedesktop.appearance"),
                                                   QStringLiteral("color-scheme"));
        if (old.isValid())
            scheme = qvariant_cast<QDBusVariant>(old.value().variant()).variant().toUInt();
    }
    m_systemDark = scheme != 2;
}

void ThemeSource::portalSettingChanged(const QString &ns, const QString &key, const QDBusVariant &value)
{
    if (ns != QLatin1String("org.freedesktop.appearance") || key != QLatin1String("color-scheme"))
        return;
    const bool dark = value.variant().toUInt() != 2;
    if (dark == m_systemDark)
        return;
    m_systemDark = dark;
    load();
}

void ThemeSource::rewatch()
{
    // Файл, заменённый через rename, выпадает из наблюдения — добавляем заново.
    // Папки ещё может не быть (matugen создаст её при первой смене обоев) —
    // тогда следим за ближайшей существующей сверху
    for (const QString &f : {qsThemeFile(), materialFile(), shelfColorsFile(), qsSettingsFile()}) {
        if (QFileInfo::exists(f) && !m_watcher.files().contains(f))
            m_watcher.addPath(f);
        QString dir = QFileInfo(f).absolutePath();
        while (!QFileInfo::exists(dir) && dir.size() > 1)
            dir = QFileInfo(dir).absolutePath();
        if (!m_watcher.directories().contains(dir))
            m_watcher.addPath(dir);
    }
}

void ThemeSource::load()
{
    rewatch();
    QString id = sourceSetting();
    const bool fromFile = !fileOf(id).isEmpty();
    // Выбранный файл ещё не появился — как в auto
    if (id == QLatin1String("auto") || (fromFile && !fileUsable(id))) {
        id = QStringLiteral("system");
        for (const char *candidate : {"qs_theme", "matugen", "material"})
            if (fileUsable(QLatin1String(candidate))) {
                id = QLatin1String(candidate);
                break;
            }
    }
    QVariantMap palette = paletteFor(id);
    // Пустой результат — файл в процессе записи; держим старые цвета
    if (palette.isEmpty() && !m_palette.isEmpty())
        palette = m_palette;
    const QVariantMap settings = glassSettings();
    if (palette == m_palette && settings == m_settings && id == m_source)
        return;
    m_palette = palette;
    m_settings = settings;
    m_source = id;
    emit changed();
}
