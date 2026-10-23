import QtQuick
import Shelf

// Переключатель-таблетка
Item {
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
