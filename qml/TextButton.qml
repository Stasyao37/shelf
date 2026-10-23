import QtQuick
import Shelf

// Кнопка с подписью для диалогов; primary — акцентная, danger — красная
Item {
    id: root

    property string text: ""
    property bool primary: false
    property bool danger: false
    readonly property color tone: danger ? Theme.red : Theme.accent

    signal clicked()

    implicitWidth: Math.max(96, label.implicitWidth + 32)
    implicitHeight: 34
    opacity: enabled ? 1.0 : 0.4
    activeFocusOnTab: true
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Keys.onSpacePressed: clicked()

    Rectangle {
        anchors.fill: parent
        radius: Theme.btnRadius(height, ma.pressed, Theme.widgetRadius)
        color: ma.pressed ? Theme.pressedFill(root.tone)
             : (root.primary || root.danger) ? Theme.activeFill(root.tone)
             : ma.containsMouse ? Theme.hoverFill : Theme.ink(0.04)
        border.width: 1
        border.color: root.activeFocus ? root.tone
                    : (root.primary || root.danger) ? Theme.edge(Qt.rgba(root.tone.r, root.tone.g, root.tone.b, Theme.activeBorderAlpha))
                    : Theme.restBorder
        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: (root.primary || root.danger) ? Theme.activeFg(root.tone) : Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 1
        font.weight: root.primary || root.danger ? Font.DemiBold : Font.Normal
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
