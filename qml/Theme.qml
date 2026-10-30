pragma Singleton

import QtCore
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

    // ── Подкраска стеклянных иконок ─────────────────────────────────
    // Стекло белое, поверх — мягкий оттенок палитры по группе файлов: папки — основной
    // цвет, документы — второй, медиа — третий; служебное (код, программы, архивы)
    // остаётся чистым стеклом. В светлой теме — та же группа, но тёмной краской.
    // Палитра тёмной темы пастельная: на белом стекле такой оттенок почти не виден,
    // поэтому оттенок берём из палитры, а насыщенность — свою. Документы чуть
    // спокойнее папок: secondary у Material того же тона, что primary
    readonly property real iconTintAmount: ThemeSource.iconTint
    // Светлый насыщенный оттенок: эффект умножает его на яркость стекла,
    // тёмный оттенок сделал бы стекло мутным, а не цветным
    function _vivid(c, sat) { return Qt.hsla(c.hslHue, Math.max(c.hslSaturation, sat), 0.8, 1) }
    function iconTint(group) {
        if (group === "folder") return _vivid(lavender, 1.0)
        if (group === "doc")    return _vivid(blue, 0.55)
        if (group === "media")  return _vivid(teal, 0.9)
        return "transparent"
    }
    function iconTinted(group) { return light || (iconTintAmount > 0 && iconTint(group).a > 0) }
    function iconColorization(group) { return light ? 1.0 : iconTintAmount }
    // Окраска чуть приглушает стекло — возвращаем яркость (только тёмная тема)
    function iconBrightness(group) { return light || iconTint(group).a === 0 ? 0 : 0.14 * iconTintAmount / 0.55 }
    function iconColor(group) {
        var c = iconTint(group)
        if (!light) return c
        // На светлом стекле краска тёмная — той же глубины, что прежний Qt.darker(accent, 1.6)
        // Оттенок выключен — всё одной краской акцента, как до подкраски
        return c.a > 0 && iconTintAmount > 0 ? Qt.hsla(c.hslHue, c.hslSaturation * 0.7, 0.34, 1) : Qt.darker(accent, 1.6)
    }
    // Картинка icons/<name>.svg, подкрашенная заранее в C++ (src/GlassIcons.h);
    // tint — параметры подкраски: groupTint(группа), inkTint или ""
    function glassIcon(name, tint) { return "image://glass/" + name + (tint ? "?" + tint : "") }
    function groupTint(group) {
        return iconTinted(group) ? _tint(iconColor(group), iconColorization(group), iconBrightness(group)) : ""
    }
    // Белые значки на светлом стекле — тёмной краской акцента; в тёмной теме как есть
    readonly property string inkTint: light ? _tint(Qt.darker(accent, 1.6), 1.0, 0) : ""
    function _tint(c, k, b) { return "tint=" + c.toString().slice(1) + "&k=" + k.toFixed(3) + "&b=" + b.toFixed(3) }

    // ── Размеры ─────────────────────────────────────────────────────
    readonly property int widgetRadius: material ? 12 : 8
    readonly property string fontFamily: material ? "Google Sans Flex" : "JetBrainsMono Nerd Font"
    // Вшитые в программу (fonts/), вырезаны из тех же JetBrainsMono Nerd Font и Propo
    readonly property string iconFont:   material ? "Shelf Icons Propo" : "Shelf Icons"
    readonly property int    fontSize:   14
    readonly property int    animDuration: dur(200)

    // ── Анимации ────────────────────────────────────────────────────
    // Переключатель в настройках; выключены — все переходы мгновенные.
    // Корень — QtObject, детей у него нет: Settings хранится в свойстве
    readonly property Settings _motion: Settings {
        category: "view"
        property bool animations: true
    }
    readonly property bool animations: _motion.animations
    // Значки грузить в фоне — включается после первого кадра окна (см. GlassImage)
    property bool imagesInBackground: false
    function setAnimations(on) { _motion.animations = on }
    function dur(ms) { return _motion.animations ? ms : 0 }

    // Запустить анимацию, когда окно покажет следующий кадр. Новая вкладка или папка
    // строятся синхронно, и их первый кадр тяжёлый; анимация идёт по часам, и запущенная
    // сразу проскакивала бы эти кадры — на экране оставались два-три шага.
    // Окно не рисуется (свёрнуто, под другим) — запасной таймер, чтобы не застрять
    function afterFrame(item, fn) {
        var w = item.Window.window
        if (!w || !animations) { fn(); return }
        // Два кадра: первый, пришедший после синхронной работы, мог быть нарисован ещё до неё.
        // frameSwapped приходит парами с разницей в пару миллисекунд — такая пара считается одним кадром
        var done = false, frames = 0, last = 0
        function stop() {
            if (done) return false
            done = true
            w.frameSwapped.disconnect(onFrame)
            return true
        }
        function once() { if (stop()) fn() }
        function onFrame() {
            var now = Date.now()
            if (now - last < 8) return
            last = now
            if (++frames < 2) { w.update(); return }
            once()
        }
        w.frameSwapped.connect(onFrame)
        // Живёт внутри item: удалили item раньше кадра — подписка снимается вместе с ним
        _fallback.createObject(item, { fire: once, cancel: stop })
    }
    readonly property Component _fallback: Component {
        Timer {
            property var fire
            property var cancel
            interval: 250
            running: true
            onTriggered: { fire(); destroy() }
            Component.onDestruction: cancel()
        }
    }
}
