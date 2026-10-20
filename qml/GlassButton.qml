import QtQuick
import QtQuick.Controls.Basic

// Копия GlassButton из шелла; подсказка — ToolTip из Controls
Item {
    id: root

    property string text:         ""
    property int    iconSize:     Theme.fontSize + 1
    property color  accentColor:  Theme.lavender
    property bool   active:       false
    property string tooltip:      ""
    property alias  containsMouse: ma.containsMouse
    property alias  pressed:       ma.pressed

    signal clicked()

    implicitWidth:  32
    implicitHeight: 30
    opacity: enabled ? 1.0 : 0.4

    Rectangle {
        anchors.fill: parent
        radius: Theme.btnRadius(height, ma.pressed, Theme.widgetRadius)

        color: ma.pressed
            ? Theme.pressedFill(root.accentColor)
            : root.active
                ? Theme.activeFill(root.accentColor)
                : ma.containsMouse
                    ? Theme.hoverFill
                    : "transparent"

        border.color: (ma.containsMouse || root.active)
            ? Theme.edge(Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, Theme.activeBorderAlpha))
            : "transparent"
        border.width: 1

        Behavior on color        { ColorAnimation { duration: Theme.animDuration } }
        Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }
        Behavior on radius { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    }

    Text {
        anchors.centerIn: parent
        text:             root.text
        font.family:      Theme.iconFont
        font.pixelSize:   root.iconSize
        color: root.active ? Theme.activeFg(root.accentColor)
             : ma.containsMouse ? Theme.hoverFg(root.accentColor, Theme.text) : Theme.text
        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
    }

    scale: ma.pressed && !Theme.material ? 0.86 : 1.0
    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape:  Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip {
        id: tip
        visible: root.tooltip.length > 0 && ma.containsMouse
        delay: 450
        text: root.tooltip
        contentItem: Text {
            text: tip.text
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 3
        }
        background: Rectangle {
            color: Theme.surface0
            radius: 6
            border.color: Theme.glassBorder
        }
    }
}
