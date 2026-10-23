import QtQuick
import Shelf

// Строка выбора одного из вариантов (настройки, первая настройка)
Item {
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
