#pragma once

#include <QDBusVariant>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Откуда брать цвета и стекло. Палитра отдаётся в именах шелла (base, surface0,
// lavender…), логика — в Theme.qml, как в Quickshell, чтобы QML переносился без правок.
//
// Источники (настройка [theme] source, по умолчанию auto — первый найденный):
//   qs_theme  — палитра шелла Quickshell (~/.local/state/quickshell/user/generated/qs_theme.json)
//   matugen   — свой шаблон Shelf в matugen (~/.local/state/shelf/colors.json)
//   material  — роли Material в colors.json шелла (формат end-4)
//   wallpaper — своя палитра по текущим обоям (hyprpaper, swww, swaybg…), без matugen
//   system    — встроенная светлая или тёмная, как color-scheme в портале
//   light, dark — встроенная, всегда такая
// auto: qs_theme → matugen → material → wallpaper → system.
// Стекло: своя настройка [theme] glass/glassAlpha, иначе ~/.config/quickshell/settings.json,
// иначе — включено, если в Hyprland включено размытие.
class ThemeSource : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantMap palette READ palette NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString sourceSetting READ sourceSetting WRITE setSourceSetting NOTIFY changed)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    // Своя настройка стекла перекрывает шелл / Hyprland
    Q_PROPERTY(bool glassOverridden READ glassOverridden NOTIFY changed)
    // Сила оттенка палитры на стеклянных иконках, 0 — без оттенка
    Q_PROPERTY(double iconTint READ iconTint WRITE setIconTint NOTIFY changed)

public:
    explicit ThemeSource(QObject *parent = nullptr);

    QVariantMap palette() const { return m_palette; }
    QVariantMap settings() const { return m_settings; }
    QString source() const { return m_source; }
    QString sourceSetting() const;
    void setSourceSetting(const QString &id);
    // [{ id, name, available }] — для окна настроек
    QVariantList sources() const;

    bool glassOverridden() const;
    double iconTint() const;
    void setIconTint(double amount);

    Q_INVOKABLE void setGlass(bool on);
    Q_INVOKABLE void setGlassAlpha(double alpha);
    // Вернуть стекло «как у шелла / как в Hyprland»
    Q_INVOKABLE void resetGlass();

    // Роли Material (matugen: surface, primary, on_surface…) → имена палитры шелла
    static QVariantMap fromMaterialRoles(const QVariantMap &roles);

signals:
    void changed();

private Q_SLOTS:
    // org.freedesktop.portal.Settings.SettingChanged
    void portalSettingChanged(const QString &ns, const QString &key, const QDBusVariant &value);
    void pollWallpaper() { refreshWallpaper(false); }

private:
    void load();
    void rewatch();
    void readSystemScheme();
    QVariantMap paletteFor(const QString &id) const;
    QVariantMap glassSettings();
    bool fileUsable(const QString &id) const;
    // Нужны ли обои: выбраны сами или auto, а файлов шелла нет
    bool wallpaperWanted() const;
    // Какие обои сейчас и их палитра; sync — сразу (при запуске), иначе в фоне
    void refreshWallpaper(bool sync);
    struct Wallpaper {
        QString path;
        QString key; // путь, время изменения, светлая/тёмная — пересчитываем, если сменилось
        QVariantMap palette;
    };
    void applyWallpaper(const Wallpaper &w, bool reload);

    QVariantMap m_palette;
    QVariantMap m_settings;
    QString m_source;
    bool m_systemDark = true;
    int m_hyprBlur = -1; // -1 — ещё не спрашивали hyprctl
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    Wallpaper m_wall;
    bool m_wallChecked = false;
    mutable QString m_wallProbe; // для списка источников, пока обои не искали
    mutable bool m_wallProbed = false;
    bool m_wallBusy = false;
    QTimer m_wallTimer; // обои меняются без файлов, за которыми можно следить, — опрашиваем
};
