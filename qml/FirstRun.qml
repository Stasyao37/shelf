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
    function defaultPick() {
        if (dlg.available("qs_theme")) return "qs_theme"
        if (dlg.available("material")) return "material"
        if (Environment.matugenConnected && dlg.available("matugen")) return "matugen"
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
    }

    function apply() {
        var msg = ""
        var id = pick
        if (id === "connect") {
            var err = Environment.connectMatugen()
            if (err) { msg = err; id = "system" }
            else { id = "matugen"; msg = "Shelf подключён к matugen — цвета появятся при следующей смене обоев" }
        }
        ThemeSource.sourceSetting = id
        // Стекло трогаем, только если выбор отличается от того, что дал шелл / Hyprland
        if (glass === Theme.material) ThemeSource.setGlass(glass)
        if (makeDefault && !Apps.defaultFileManager && !Apps.makeDefaultFileManager())
            msg = msg ? msg + "; не удалось сделать Shelf проводником по умолчанию"
                      : "Не удалось сделать Shelf проводником по умолчанию — нужен xdg-mime (xdg-utils)"
        else if (makeDefault && !msg)
            msg = "Shelf теперь проводник по умолчанию"
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

    // Строка проверки: галочка или предупреждение, что это и что делать
    component Check: Item {
        id: chk
        property bool ok: true
        property bool required: false
        property string label
        property string hint
        width: parent ? parent.width : 0
        height: hintText.visible ? 44 : 28
        Text {
            id: mark
            x: 12; y: 6
            text: chk.ok ? "\u{f00c}" : chk.required ? "\u{f00d}" : "\u{f071}"
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
                        source: Qt.resolvedUrl("data/icons/org.stasyao.shelf.svg")
                        sourceSize: Qt.size(104, 104)
                        smooth: true
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: content.width - 66
                        spacing: 3
                        Text {
                            text: "Добро пожаловать в Shelf"
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize + 3
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width
                            text: "Пара вопросов, чтобы Shelf подошёл к системе. Всё это можно поменять потом в настройках (Ctrl+,)."
                            wrapMode: Text.Wrap
                            color: Theme.subtext0
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 3
                        }
                    }
                }

                SectionTitle { text: "ЧТО НАШЛОСЬ" }
                Check {
                    ok: Environment.hyprland
                    label: ok ? "Hyprland" : "Не Hyprland"
                    hint: ok ? "" : "Shelf пока проверялся только на Hyprland — что-то может выглядеть иначе"
                }
                Check {
                    ok: Environment.quickshell
                    label: ok ? "Quickshell запущен" : "Quickshell не найден"
                    hint: ok ? "" : "Без него — цвета от matugen или встроенная тема"
                }
                Check {
                    ok: Environment.matugen
                    label: ok ? "matugen — палитра из обоев" : "matugen не установлен"
                    hint: ok ? "" : "Палитра из обоев: " + (Environment.installCommand("matugen") || "matugen")
                }
                Check {
                    ok: Environment.blur
                    label: ok ? "Размытие в Hyprland — будет стекло" : "Размытие выключено"
                    hint: ok ? "" : "Без размытия стекло не смотрится — по умолчанию сплошной фон"
                }
                Check {
                    ok: Environment.tumbler
                    label: ok ? "Миниатюры картинок и PDF (tumbler)" : "Нет tumbler — без миниатюр"
                    hint: ok ? "" : Environment.installCommand("tumbler")
                }
                Check {
                    ok: Environment.ffmpegthumbnailer
                    label: ok ? "Миниатюры видео (ffmpegthumbnailer)" : "Нет ffmpegthumbnailer — видео без миниатюр"
                    hint: ok ? "" : Environment.installCommand("ffmpegthumbnailer")
                }
                Check {
                    ok: Terminal.current !== ""
                    label: ok ? "Терминал: " + Terminal.current : "Терминал не найден"
                    hint: ok ? "" : "«Открыть в терминале» не будет работать: " + (Environment.installCommand("kitty") || "установите любой терминал")
                }
                Check {
                    ok: Environment.gio && Environment.xdgUtils
                    required: true
                    label: ok ? "gio и xdg-utils — открытие файлов" : "Нет gio или xdg-utils — файлы не будут открываться"
                    hint: ok ? "" : [!Environment.gio ? Environment.installCommand("glib2") : "",
                                     !Environment.xdgUtils ? Environment.installCommand("xdg-utils") : ""].filter(function (x) { return x }).join("  ·  ")
                }

                SectionTitle { text: "КАК ВЫГЛЯДЕТЬ" }
                ChoiceRow {
                    visible: dlg.available("qs_theme")
                    label: "Как у Quickshell"
                    note: "цвета и стекло шелла"
                    checked: dlg.pick === "qs_theme"
                    onPicked: dlg.pick = "qs_theme"
                }
                ChoiceRow {
                    visible: dlg.available("material")
                    label: "Material из шелла"
                    note: "colors.json"
                    checked: dlg.pick === "material"
                    onPicked: dlg.pick = "material"
                }
                ChoiceRow {
                    visible: Environment.matugen
                    label: Environment.matugenConnected ? "matugen — шаблон Shelf" : "Подключить к matugen"
                    note: Environment.matugenConnected ? "уже подключён" : "палитра из обоев"
                    checked: dlg.pick === (Environment.matugenConnected ? "matugen" : "connect")
                    onPicked: dlg.pick = Environment.matugenConnected ? "matugen" : "connect"
                }
                Text {
                    visible: dlg.pick === "connect"
                    width: parent.width
                    leftPadding: 38
                    rightPadding: 12
                    bottomPadding: 4
                    text: "Shelf положит свой шаблон в ~/.config/shelf/matugen и допишет его в конец ~/.config/matugen/config.toml (копия — config.toml.bak-shelf). Сам matugen не запускается: цвета появятся при следующей смене обоев."
                    wrapMode: Text.Wrap
                    color: Theme.overlay1
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 4
                }
                ChoiceRow {
                    label: "Как в системе"
                    note: "светлая или тёмная — по системной схеме"
                    checked: dlg.pick === "system"
                    onPicked: dlg.pick = "system"
                }
                ChoiceRow {
                    label: "Светлая"
                    checked: dlg.pick === "light"
                    onPicked: dlg.pick = "light"
                }
                ChoiceRow {
                    label: "Тёмная"
                    checked: dlg.pick === "dark"
                    onPicked: dlg.pick = "dark"
                }

                Item {
                    width: parent.width
                    height: 40
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Стеклянный вид"
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

                // Проводник по умолчанию: папки из других программ и xdg-open откроются в Shelf
                Item {
                    width: parent.width
                    height: 40
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: Apps.defaultFileManager ? "Shelf — проводник по умолчанию" : "Сделать Shelf проводником по умолчанию"
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
                text: "Пропустить"
                onClicked: { ThemeSource.sourceSetting = "auto"; dlg.close(); dlg.finished("") }
            }
            TextButton {
                text: "Готово"
                primary: true
                onClicked: dlg.apply()
            }
        }
    }
}
