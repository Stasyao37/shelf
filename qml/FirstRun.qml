pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Первая настройка: что нашлось в системе и как Shelf будет выглядеть.
// Показывается один раз ([setup] done в shelf.conf); заново — из настроек.
// Пакеты не ставит и matugen не запускает — только подсказывает и, по выбору,
// дописывает свой шаблон в config.toml (с резервной копией).
GlassDialog {
    id: dlg

    signal finished(string message)

    // Вариант по умолчанию: как у шелла, если он есть, иначе как в системе
    property string pick: ""
    property bool glass: true
    // Сделать Shelf проводником по умолчанию — меняет системную настройку, поэтому выключено
    property bool makeDefault: false
    // Окна выбора файлов Shelf для других программ — тоже меняет системные настройки
    property bool usePicker: false
    function defaultPick() {
        if (dlg.available("qs_theme")) return "qs_theme"
        if (dlg.available("material")) return "material"
        if (Environment.matugenConnected && dlg.available("matugen")) return "matugen"
        if (dlg.available("wallpaper")) return "wallpaper"
        return "system"
    }
    function available(id) {
        var list = ThemeSource.sources
        for (var i = 0; i < list.length; ++i)
            if (list[i].id === id) return list[i].available
        return false
    }
    onAboutToShow: {
        // Открыли заново из настроек — отмечаем то, что выбрано сейчас
        var current = ThemeSource.sourceSetting
        pick = current !== "auto" ? current : defaultPick()
        glass = !Theme.material
        makeDefault = false
        usePicker = false
    }

    function apply() {
        var msg = ""
        var id = pick
        if (id === "connect") {
            var err = Environment.connectMatugen()
            if (err) { msg = err; id = "system" }
            else { id = "matugen"; msg = qsTr("Shelf подключён к matugen — цвета появятся при следующей смене обоев") }
        }
        ThemeSource.sourceSetting = id
        // Стекло трогаем, только если выбор отличается от того, что дал шелл / Hyprland
        if (glass === Theme.material) ThemeSource.setGlass(glass)
        if (makeDefault && !Apps.defaultFileManager && !Apps.makeDefaultFileManager())
            msg = msg ? msg + qsTr("; не удалось сделать Shelf проводником по умолчанию")
                      : qsTr("Не удалось сделать Shelf проводником по умолчанию — нужен xdg-mime (xdg-utils)")
        else if (makeDefault && !msg)
            msg = qsTr("Shelf теперь проводник по умолчанию")
        if (usePicker && !Environment.portalPicker) {
            var perr = Environment.setPortalPicker(true)
            if (perr) msg = msg ? msg + "; " + perr : perr
        }
        close()
        finished(msg)
    }

    padding: 0
    width: 540
    height: Math.min(content.implicitHeight + 90, parent ? parent.height - 48 : 640)
    // Закрыть мимо нельзя — только «Пропустить» или «Готово», чтобы выбор не потерялся
    closePolicy: Popup.CloseOnEscape

    shade: 0.35

    component SectionTitle: Text {
        topPadding: 12
        bottomPadding: 2
        color: Theme.subtext0
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 3
        font.weight: Font.DemiBold
    }

    // Строка проверки: галочка или предупреждение, что это и что делать.
    // optional — без этого всё работает: вместо предупреждения — просто пометка
    component Check: Item {
        id: chk
        property bool ok: true
        property bool required: false
        property bool optional: false
        property string label
        property string hint
        width: parent ? parent.width : 0
        height: hintText.visible ? 44 : 28
        Text {
            id: mark
            x: 12; y: 6
            text: chk.ok ? "\u{f00c}" : chk.required ? "\u{f00d}" : chk.optional ? "\u{f05a}" : "\u{f071}"
            color: chk.ok ? Theme.accent : chk.required ? Theme.red : Theme.overlay1
            font.family: Theme.iconFont
            font.pixelSize: Theme.fontSize - 3
        }
        Text {
            id: labelText
            anchors { left: parent.left; leftMargin: 38; right: parent.right; rightMargin: 12 }
            y: 4
            text: chk.label
            elide: Text.ElideRight
            color: chk.ok ? Theme.text : Theme.subtext1
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }
        Text {
            id: hintText
            visible: text.length > 0
            anchors { left: labelText.left; right: parent.right; rightMargin: 12; top: labelText.bottom; topMargin: 1 }
            text: chk.hint
            elide: Text.ElideRight
            color: Theme.overlay1
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 4
        }
    }

    contentItem: Item {
        // Кнопки закреплены внизу — видны, даже если список пришлось прокручивать
        Flickable {
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: footer.top; bottomMargin: 10 }
            clip: true
            contentHeight: content.implicitHeight + 24
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: content
                x: 20; y: 20
                width: dlg.width - 40
                spacing: 2

                Row {
                    spacing: 14
                    Image {
                        width: 52; height: 52
                        source: Qt.resolvedUrl("data/icons/io.github.nook_apps.Shelf.svg")
                        sourceSize: Qt.size(104, 104)
                        smooth: true
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: content.width - 66
                        spacing: 3
                        Text {
                            text: qsTr("Добро пожаловать в Shelf")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize + 3
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width
                            text: qsTr("Пара вопросов, чтобы Shelf подошёл к системе. Всё это можно поменять потом в настройках (Ctrl+,).")
                            wrapMode: Text.Wrap
                            color: Theme.subtext0
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 3
                        }
                    }
                }

                SectionTitle { text: qsTr("ЧТО НАШЛОСЬ") }
                Check {
                    ok: Environment.hyprland
                    label: ok ? "Hyprland" : qsTr("Не Hyprland")
                    hint: ok ? "" : qsTr("Shelf пока проверялся только на Hyprland — что-то может выглядеть иначе")
                }
                Check {
                    ok: Environment.quickshell
                    label: ok ? qsTr("Quickshell запущен") : qsTr("Quickshell не найден")
                    hint: ok ? "" : qsTr("Без него — цвета от matugen или встроенная тема")
                }
                // Оттенок обоев Shelf берёт и сам («Из обоев»); matugen — только для полной
                // палитры Material по шаблону, в лад с шеллом
                Check {
                    ok: Environment.matugen
                    optional: true
                    label: ok ? qsTr("matugen — палитра из обоев") : qsTr("matugen не установлен")
                    hint: ok ? "" : qsTr("Не обязателен: оттенок обоев Shelf берёт и сам. С ним — полная палитра Material: %1")
                                        .arg(Environment.installCommand("matugen") || "matugen")
                }
                Check {
                    ok: Environment.blur
                    label: ok ? qsTr("Размытие в Hyprland — будет стекло") : qsTr("Размытие выключено")
                    hint: ok ? "" : qsTr("Без размытия стекло не смотрится — по умолчанию сплошной фон")
                }
                Check {
                    ok: Environment.tumbler
                    label: ok ? qsTr("Миниатюры картинок и PDF (tumbler)") : qsTr("Нет tumbler — без миниатюр")
                    hint: ok ? "" : Environment.installCommand("tumbler")
                }
                Check {
                    ok: Environment.ffmpegthumbnailer
                    label: ok ? qsTr("Миниатюры видео (ffmpegthumbnailer)") : qsTr("Нет ffmpegthumbnailer — видео без миниатюр")
                    hint: ok ? "" : Environment.installCommand("ffmpegthumbnailer")
                }
                Check {
                    ok: Terminal.current !== ""
                    label: ok ? qsTr("Терминал: %1").arg(Terminal.current) : qsTr("Терминал не найден")
                    hint: ok ? "" : qsTr("«Открыть в терминале» не будет работать: %1").arg(Environment.installCommand("kitty") || qsTr("установите любой терминал"))
                }
                Check {
                    ok: Environment.gio && Environment.xdgUtils
                    required: true
                    label: ok ? qsTr("gio и xdg-utils — открытие файлов") : qsTr("Нет gio или xdg-utils — файлы не будут открываться")
                    hint: ok ? "" : [!Environment.gio ? Environment.installCommand("glib2") : "",
                                     !Environment.xdgUtils ? Environment.installCommand("xdg-utils") : ""].filter(function (x) { return x }).join("  ·  ")
                }

                SectionTitle { text: qsTr("КАК ВЫГЛЯДЕТЬ") }
                ChoiceRow {
                    visible: dlg.available("qs_theme")
                    label: qsTr("Как у Quickshell")
                    note: qsTr("цвета и стекло шелла")
                    checked: dlg.pick === "qs_theme"
                    onPicked: dlg.pick = "qs_theme"
                }
                ChoiceRow {
                    visible: dlg.available("material")
                    label: qsTr("Material из шелла")
                    note: "colors.json"
                    checked: dlg.pick === "material"
                    onPicked: dlg.pick = "material"
                }
                ChoiceRow {
                    visible: Environment.matugen
                    label: Environment.matugenConnected ? qsTr("matugen — шаблон Shelf") : qsTr("Подключить к matugen")
                    note: Environment.matugenConnected ? qsTr("уже подключён") : qsTr("палитра из обоев")
                    checked: dlg.pick === (Environment.matugenConnected ? "matugen" : "connect")
                    onPicked: dlg.pick = Environment.matugenConnected ? "matugen" : "connect"
                }
                Text {
                    visible: dlg.pick === "connect"
                    width: parent.width
                    leftPadding: 38
                    rightPadding: 12
                    bottomPadding: 4
                    text: qsTr("Shelf положит свой шаблон в ~/.config/shelf/matugen и допишет его в конец ~/.config/matugen/config.toml (копия — config.toml.bak-shelf). Сам matugen не запускается: цвета появятся при следующей смене обоев.")
                    wrapMode: Text.Wrap
                    color: Theme.overlay1
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 4
                }
                ChoiceRow {
                    visible: dlg.available("wallpaper")
                    label: qsTr("Из обоев")
                    note: qsTr("оттенок текущих обоев, без matugen")
                    checked: dlg.pick === "wallpaper"
                    onPicked: dlg.pick = "wallpaper"
                }
                ChoiceRow {
                    label: qsTr("Как в системе")
                    note: qsTr("светлая или тёмная — по системной схеме")
                    checked: dlg.pick === "system"
                    onPicked: dlg.pick = "system"
                }
                ChoiceRow {
                    label: qsTr("Светлая")
                    checked: dlg.pick === "light"
                    onPicked: dlg.pick = "light"
                }
                ChoiceRow {
                    label: qsTr("Тёмная")
                    checked: dlg.pick === "dark"
                    onPicked: dlg.pick = "dark"
                }

                Item {
                    width: parent.width
                    height: 40
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Стеклянный вид")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    ToggleSwitch {
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        checked: dlg.glass
                        onToggled: (on) => dlg.glass = on
                    }
                }

                // Окна выбора файлов Shelf в других программах (если бэкенд портала установлен)
                Item {
                    visible: Environment.portalBackend
                    width: parent.width
                    height: 40
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: Environment.portalPicker ? qsTr("Выбор файлов в окнах Shelf включён") : qsTr("Выбор файлов в окнах Shelf")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Text {
                        visible: Environment.portalPicker
                        anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                        text: "\u{f00c}"
                        color: Theme.accent
                        font.family: Theme.iconFont
                        font.pixelSize: Theme.fontSize - 2
                    }
                    ToggleSwitch {
                        visible: !Environment.portalPicker
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        checked: dlg.usePicker
                        onToggled: (on) => dlg.usePicker = on
                    }
                }

                // Проводник по умолчанию: папки из других программ и xdg-open откроются в Shelf
                Item {
                    width: parent.width
                    height: 40
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: Apps.defaultFileManager ? qsTr("Shelf — проводник по умолчанию") : qsTr("Сделать Shelf проводником по умолчанию")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Text {
                        visible: Apps.defaultFileManager
                        anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                        text: "\u{f00c}"
                        color: Theme.accent
                        font.family: Theme.iconFont
                        font.pixelSize: Theme.fontSize - 2
                    }
                    ToggleSwitch {
                        visible: !Apps.defaultFileManager
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        checked: dlg.makeDefault
                        onToggled: (on) => dlg.makeDefault = on
                    }
                }
            }
        }

        Row {
            id: footer
            anchors { right: parent.right; bottom: parent.bottom; rightMargin: 20; bottomMargin: 16 }
            spacing: 8
            TextButton {
                text: qsTr("Пропустить")
                onClicked: { ThemeSource.sourceSetting = "auto"; dlg.close(); dlg.finished("") }
            }
            TextButton {
                text: qsTr("Готово")
                primary: true
                onClicked: dlg.apply()
            }
        }
    }
}
