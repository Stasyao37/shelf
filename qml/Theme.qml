pragma Singleton

import QtQuick
import Shelf

// Перенесено из ~/.config/quickshell/Theme.qml: те же имена и логика,
// источник данных — ThemeSource (файлы шелла) вместо FileView/SettingsStore.
QtObject {
    id: root

    readonly property var _dyn: ThemeSource.palette
    readonly property var _set: ThemeSource.settings

    readonly property bool light: _rawBase.hslLightness > 0.5
    function ink(a) { return light ? Qt.rgba(0, 0, 0, a * 0.6) : Qt.rgba(1, 1, 1, a) }

    readonly property color teal:      _dyn.teal      ?? "#5baab0"
    readonly property color blue:      _dyn.blue      ?? "#6aaee0"
    readonly property color lavender:  _dyn.lavender  ?? "#8ab2e8"
    readonly property color accent:    lavender
    readonly property color accentAlt: teal

    readonly property color text:     _dyn.text     ?? "#ccd4e8"
    readonly property color subtext1: _dyn.subtext1 ?? "#b0bcd4"
    readonly property color subtext0: _dyn.subtext0 ?? "#98a8c0"
    readonly property color overlay2: _dyn.overlay2 ?? "#7e90aa"
    readonly property color overlay1: _dyn.overlay1 ?? "#667890"
    readonly property color overlay0: _dyn.overlay0 ?? "#506078"

    readonly property real _tintK: material ? (light ? 0.10 : 0.14) : 0
    function _surf(c) {
        return _tintK > 0 ? Qt.tint(c, Qt.rgba(lavender.r, lavender.g, lavender.b, _tintK)) : c
    }
    readonly property color surface2: _surf(_dyn.surface2 ?? "#3c5068")
    readonly property color surface1: _surf(_dyn.surface1 ?? "#2e3e54")
    readonly property color surface0: _surf(_dyn.surface0 ?? "#223044")

    readonly property color _rawBase: _dyn.base ?? "#192030"
    readonly property color base:   _surf(_rawBase)
    readonly property color mantle: _surf(_dyn.mantle ?? "#131928")
    readonly property color crust:  _surf(_dyn.crust  ?? "#0d1220")

    readonly property color primaryContainer:     _dyn.primaryContainer     ?? Qt.tint(surface1, Qt.rgba(lavender.r, lavender.g, lavender.b, 0.35))
    readonly property color primaryContainerFg:   _dyn.onPrimaryContainer   ?? text
    readonly property color secondaryContainer:   _dyn.secondaryContainer   ?? Qt.tint(surface1, Qt.rgba(blue.r, blue.g, blue.b, 0.30))
    readonly property color secondaryContainerFg: _dyn.onSecondaryContainer ?? text
    readonly property color tertiaryContainer:    _dyn.tertiaryContainer    ?? Qt.tint(surface1, Qt.rgba(teal.r, teal.g, teal.b, 0.35))
    readonly property color tertiaryContainerFg:  _dyn.onTertiaryContainer  ?? text

    readonly property color red:   light ? "#b0505c" : "#c47a82"
    readonly property color green: light ? "#3a8560" : "#7eb89a"

    // ── Стекло ──────────────────────────────────────────────────────
    readonly property bool  material:    !(_set.glass ?? true)
    // Окно приложения плотнее попапов шелла: под ним — другие окна, а не обои
    readonly property real  glassAlpha:  material ? 1.0 : Math.min(1.0, (_set.glassAlpha ?? 0.62) + 0.18)
    readonly property color glassBorder: material ? "transparent"
                                       : Qt.rgba(accent.r, accent.g, accent.b, light ? 0.35 : 0.30)

    readonly property color hoverFill:         material ? Qt.rgba(text.r, text.g, text.b, 0.08) : ink(0.10)
    readonly property color hoverBorder:       material ? "transparent" : ink(0.18)
    readonly property color restBorder:        material ? "transparent" : ink(0.13)
    readonly property color restFill:          material ? surface1 : "transparent"
    readonly property real  activeAlpha:       material ? 0.28 : 0.22
    readonly property real  pressedAlpha:      0.34
    readonly property real  activeBorderAlpha: material ? 0.0 : 0.55

    function container(c) {
        if (Qt.colorEqual(c, lavender)) return primaryContainer
        if (Qt.colorEqual(c, teal))     return tertiaryContainer
        if (Qt.colorEqual(c, blue))     return secondaryContainer
        return Qt.tint(surface1, Qt.rgba(c.r, c.g, c.b, light ? 0.22 : 0.30))
    }
    function containerFg(c) {
        if (Qt.colorEqual(c, lavender)) return primaryContainerFg
        if (Qt.colorEqual(c, teal))     return tertiaryContainerFg
        if (Qt.colorEqual(c, blue))     return secondaryContainerFg
        return light ? Qt.darker(c, 1.35) : Qt.lighter(c, 1.2)
    }
    function activeFill(c)  { return material ? container(c) : Qt.rgba(c.r, c.g, c.b, activeAlpha) }
    function pressedFill(c) {
        if (!material) return Qt.rgba(c.r, c.g, c.b, pressedAlpha)
        var on = containerFg(c)
        return Qt.tint(container(c), Qt.rgba(on.r, on.g, on.b, 0.14))
    }
    function activeFg(c)    { return material ? containerFg(c) : c }
    function hoverFg(c, rest) { return material ? rest : c }
    function edge(c)        { return material ? "transparent" : c }
    function btnRadius(h, pressed, glassR) {
        return material ? (pressed ? h * 0.28 : h / 2) : glassR
    }

    // ── Размеры ─────────────────────────────────────────────────────
    readonly property int widgetRadius: material ? 12 : 8
    readonly property string fontFamily: material ? "Google Sans Flex" : "JetBrainsMono Nerd Font"
    // Вшитые в программу (fonts/), вырезаны из тех же JetBrainsMono Nerd Font и Propo
    readonly property string iconFont:   material ? "Shelf Icons Propo" : "Shelf Icons"
    readonly property int    fontSize:   14
    readonly property int    animDuration: 200
}
