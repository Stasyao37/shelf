pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Настройки Shelf: откуда цвета, стекло, оттенок иконок, терминал.
// Всё применяется сразу и хранится в ~/.config/shelf/shelf.conf
GlassDialog {
    id: dlg

    signal firstRunRequested()

    padding: 0
    width: 500
    height: Math.min(content.implicitHeight + 90, parent ? parent.height - 48 : 600)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // ── Мелкие контролы ─────────────────────────────────────────────
    component SectionTitle: Text {
        topPadding: 10
        color: Theme.subtext0
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 3
        font.weight: Font.DemiBold
    }

    // Ползунок; moved — пока тянут, released — отпустили (тогда и сохраняем)
    component Slide: Item {
        id: sl
        property real from: 0
        property real to: 1
        property real value: 0
        signal moved(real v)
        signal released(real v)
        height: 22
        readonly property real frac: (value - from) / (to - from)
        Rectangle {
            id: track
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width; height: 6; radius: 3
            color: Theme.ink(0.10)
            Rectangle {
                width: parent.width * sl.frac
                height: parent.height; radius: 3
                color: Theme.accent
            }
        }
        Rectangle {
            width: 16; height: 16; radius: 8
            anchors.verticalCenter: parent.verticalCenter
            x: sl.frac * (sl.width - width)
            color: Theme.material ? Theme.accent : Qt.lighter(Theme.accent, 1.1)
            border.width: 2
            border.color: Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.9)
        }
        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            cursorShape: Qt.PointingHandCursor
            function at(mx) {
                // Шаг 5 %: ноль и края легко поймать мышью
                var f = Math.max(0, Math.min(1, (mx - 6) / (sl.width)))
                return Math.round((sl.from + f * (sl.to - sl.from)) * 20) / 20
            }
            onPressed: (m) => sl.moved(at(m.x))
            onPositionChanged: (m) => { if (pressed) sl.moved(at(m.x)) }
            onReleased: (m) => sl.released(at(m.x))
        }
    }

    // ── Содержимое ──────────────────────────────────────────────────
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
                spacing: 4

                // Заголовок и версия — одной строкой
                Item {
                    width: parent.width
                    height: titleText.implicitHeight
                    Text {
                        id: titleText
                        text: "Настройки"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize + 2
                        font.weight: Font.DemiBold
                    }
                    Text {
                        anchors { right: parent.right; baseline: titleText.baseline }
                        text: "Shelf " + Qt.application.version
                        color: Theme.overlay1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 4
                    }
                }

                SectionTitle { text: "ЦВЕТА" }
                Repeater {
                    model: ThemeSource.sources
                    ChoiceRow {
                        required property var modelData
                        label: modelData.name
                        available: modelData.available
                        checked: ThemeSource.sourceSetting === modelData.id
                        note: !modelData.available ? "не найден"
                            : modelData.id === "auto" ? "сейчас: " + dlg.sourceName(ThemeSource.source) : ""
                        onPicked: ThemeSource.sourceSetting = modelData.id
                    }
                }

                SectionTitle { text: "СТЕКЛО" }
                Item {
                    width: parent.width
                    height: 34
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
                        checked: !Theme.material
                        onToggled: (on) => ThemeSource.setGlass(on)
                    }
                }
                Item {
                    width: parent.width
                    height: 34
                    visible: !Theme.material
                    Text {
                        id: alphaLabel
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Плотность"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Slide {
                        id: alphaSlide
                        anchors { left: parent.left; leftMargin: 130; right: alphaValue.left; rightMargin: 14; verticalCenter: parent.verticalCenter }
                        from: 0.2; to: 1.0
                        value: ThemeSource.settings.glassAlpha ?? 0.62
                        onMoved: (v) => value = v
                        onReleased: (v) => ThemeSource.setGlassAlpha(v)
                    }
                    Text {
                        id: alphaValue
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        width: 40
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(alphaSlide.value * 100) + " %"
                        color: Theme.subtext0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 3
                    }
                }
                Text {
                    visible: ThemeSource.glassOverridden
                    leftPadding: 12
                    text: "Вернуть как у шелла"
                    color: resetMa.containsMouse ? Theme.accent : Theme.subtext0
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 3
                    MouseArea {
                        id: resetMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: ThemeSource.resetGlass()
                    }
                }

                SectionTitle { text: "ИКОНКИ" }
                Item {
                    width: parent.width
                    height: 34
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Оттенок палитры"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Slide {
                        id: tintSlide
                        anchors { left: parent.left; leftMargin: 160; right: tintValue.left; rightMargin: 14; verticalCenter: parent.verticalCenter }
                        from: 0; to: 1
                        value: ThemeSource.iconTint
                        // Иконки перекрашиваются прямо пока тянешь
                        onMoved: (v) => ThemeSource.iconTint = v
                        onReleased: (v) => ThemeSource.iconTint = v
                    }
                    Text {
                        id: tintValue
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        width: 40
                        horizontalAlignment: Text.AlignRight
                        text: tintSlide.value === 0 ? "нет" : Math.round(tintSlide.value * 100) + " %"
                        color: Theme.subtext0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 3
                    }
                }

                SectionTitle { text: "ИНТЕРФЕЙС" }
                Item {
                    width: parent.width
                    height: 34
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Анимации"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Text {
                        anchors { right: animToggle.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        text: "переходы, окна, появление файлов"
                        color: Theme.overlay1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 4
                    }
                    ToggleSwitch {
                        id: animToggle
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        checked: Theme.animations
                        onToggled: (on) => Theme.setAnimations(on)
                    }
                }

                SectionTitle { text: "ТЕРМИНАЛ" }
                ChoiceRow {
                    label: "Автоматически"
                    note: Terminal.chosen === "" ? (Terminal.current ? "сейчас: " + Terminal.current : "терминал не найден") : ""
                    checked: Terminal.chosen === ""
                    onPicked: Terminal.chosen = ""
                }
                Repeater {
                    model: Terminal.available
                    ChoiceRow {
                        required property string modelData
                        label: modelData
                        checked: Terminal.chosen === modelData
                        onPicked: Terminal.chosen = modelData
                    }
                }
            }
        }

        Item {
            id: footer
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 20; rightMargin: 20; bottomMargin: 16 }
            height: 34
            TextButton {
                text: "Первая настройка…"
                onClicked: { dlg.close(); dlg.firstRunRequested() }
            }
            TextButton {
                anchors.right: parent.right
                text: "Готово"
                primary: true
                onClicked: dlg.close()
            }
        }
    }

    function sourceName(id) {
        var list = ThemeSource.sources
        for (var i = 0; i < list.length; ++i)
            if (list[i].id === id) return list[i].name
        return id
    }
}
