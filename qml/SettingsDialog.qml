pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Настройки Shelf: откуда цвета, стекло, оттенок иконок, терминал.
// Всё применяется сразу и хранится в ~/.config/shelf/shelf.conf
Popup {
    id: dlg

    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 0
    width: 500
    height: Math.min(content.implicitHeight + 40, parent ? parent.height - 48 : 600)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.25) }

    background: Rectangle {
        color: Theme.material ? Theme.surface1 : Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.97)
        radius: Theme.widgetRadius + 4
        border.width: 1
        border.color: Theme.material ? "transparent" : Theme.glassBorder
    }

    // ── Мелкие контролы ─────────────────────────────────────────────
    component SectionTitle: Text {
        topPadding: 10
        color: Theme.subtext0
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 3
        font.weight: Font.DemiBold
    }

    // Строка выбора одного из вариантов
    component Choice: Item {
        id: choice
        property string label
        property string note
        property bool checked: false
        property bool available: true
        signal picked()
        width: parent ? parent.width : 0
        height: 34
        opacity: available ? 1 : 0.45

        Rectangle {
            anchors.fill: parent
            radius: Theme.btnRadius(height, false, Theme.widgetRadius)
            color: choice.checked ? Theme.activeFill(Theme.accent) : choiceMa.containsMouse ? Theme.hoverFill : "transparent"
            border.width: choice.checked ? 1 : 0
            border.color: Theme.edge(Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, Theme.activeBorderAlpha))
        }
        // Кружок-отметка
        Rectangle {
            id: dot
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 16; height: 16; radius: 8
            color: "transparent"
            border.width: 1.5
            border.color: choice.checked ? Theme.accent : Theme.ink(0.3)
            Rectangle {
                anchors.centerIn: parent
                width: 8; height: 8; radius: 4
                visible: choice.checked
                color: Theme.accent
            }
        }
        Text {
            id: choiceLabel
            anchors { left: dot.right; leftMargin: 12; verticalCenter: parent.verticalCenter }
            text: choice.label
            color: choice.checked ? Theme.activeFg(Theme.accent) : Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }
        Text {
            anchors { left: choiceLabel.right; leftMargin: 10; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            text: choice.note
            elide: Text.ElideRight
            color: Theme.overlay1
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 4
        }
        MouseArea {
            id: choiceMa
            anchors.fill: parent
            hoverEnabled: true
            enabled: choice.available
            cursorShape: Qt.PointingHandCursor
            onClicked: choice.picked()
        }
    }

    // Переключатель-таблетка
    component Toggle: Item {
        id: tog
        property bool checked: false
        signal toggled(bool on)
        width: 38; height: 22
        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: tog.checked ? Theme.activeFill(Theme.accent) : Theme.ink(0.08)
            border.width: 1
            border.color: tog.checked ? Theme.accent : Theme.ink(0.25)
            Behavior on color { ColorAnimation { duration: 140 } }
        }
        Rectangle {
            width: 14; height: 14; radius: 7
            anchors.verticalCenter: parent.verticalCenter
            x: tog.checked ? parent.width - width - 4 : 4
            color: tog.checked ? Theme.accent : Theme.subtext0
            Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: tog.toggled(!tog.checked)
        }
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
    contentItem: Flickable {
        clip: true
        contentHeight: content.implicitHeight + 40
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: content
            x: 20; y: 20
            width: dlg.width - 40
            spacing: 4

            Text {
                text: "Настройки"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 2
                font.weight: Font.DemiBold
            }

            SectionTitle { text: "ЦВЕТА" }
            Repeater {
                model: ThemeSource.sources
                Choice {
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
                Toggle {
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

            SectionTitle { text: "ТЕРМИНАЛ" }
            Choice {
                label: "Автоматически"
                note: Terminal.chosen === "" ? (Terminal.current ? "сейчас: " + Terminal.current : "терминал не найден") : ""
                checked: Terminal.chosen === ""
                onPicked: Terminal.chosen = ""
            }
            Repeater {
                model: Terminal.available
                Choice {
                    required property string modelData
                    label: modelData
                    checked: Terminal.chosen === modelData
                    onPicked: Terminal.chosen = modelData
                }
            }

            Item { width: 1; height: 8 }
            Row {
                anchors.right: parent.right
                TextButton { text: "Готово"; primary: true; onClicked: dlg.close() }
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
