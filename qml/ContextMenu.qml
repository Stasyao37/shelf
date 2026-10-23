import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Меню по ПКМ. Пункты: { text, icon, shortcut, enabled, checked, act } или { separator: true }
Popup {
    id: menu

    property var items: []

    // Высота по самим пунктам: Column пересчитывает свою лениво, к моменту show()
    // у него ещё размер прошлого меню
    function heightOf(list) {
        var h = topPadding + bottomPadding
        for (var i = 0; i < list.length; ++i) h += list[i].separator === true ? 9 : 32
        return h
    }
    function show(px, py, list) {
        items = list
        var w = parent ? parent.width : 0
        var h = parent ? parent.height : 0
        var mw = implicitWidth, mh = heightOf(list)
        // Не влезает ниже или правее курсора — открываем выше или левее, как в других программах
        x = px + mw + 4 <= w ? px : Math.max(4, px - mw)
        y = py + mh + 4 <= h ? py : Math.max(4, py - mh)
        open()
    }

    padding: 6
    implicitWidth: 260
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 110 }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 110; easing.type: Easing.OutCubic }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: 90 } }

    background: Rectangle {
        color: Theme.material ? Theme.surface1 : Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.97)
        radius: Theme.widgetRadius + 2
        border.width: 1
        border.color: Theme.material ? "transparent" : Theme.glassBorder
    }

    contentItem: Column {
        Repeater {
            model: menu.items
            Item {
                id: entry
                required property var modelData
                readonly property bool sep: modelData.separator === true
                readonly property bool on: modelData.enabled !== false
                width: menu.availableWidth
                height: sep ? 9 : 32

                Rectangle {
                    visible: entry.sep
                    anchors.verticalCenter: parent.verticalCenter
                    x: 8; width: parent.width - 16; height: 1
                    color: Theme.restBorder.a > 0 ? Theme.restBorder : Theme.surface2
                }
                Rectangle {
                    visible: !entry.sep
                    anchors.fill: parent
                    radius: Theme.btnRadius(height, false, 6)
                    color: ma.containsMouse && entry.on ? Theme.hoverFill : "transparent"
                }
                Text {
                    id: ic
                    visible: !entry.sep
                    x: 10; width: 20
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: entry.modelData.checked ? "\u{f00c}" : (entry.modelData.icon || "")
                    color: entry.modelData.checked ? Theme.accent : Theme.subtext1
                    font.family: Theme.iconFont
                    font.pixelSize: Theme.fontSize - 1
                    opacity: entry.on ? 1 : 0.4
                }
                Text {
                    visible: !entry.sep
                    anchors { left: ic.right; leftMargin: 10; right: sc.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    text: entry.modelData.text || ""
                    elide: Text.ElideRight
                    color: entry.modelData.danger ? Theme.red : Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                    opacity: entry.on ? 1 : 0.4
                }
                Text {
                    id: sc
                    visible: !entry.sep
                    anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                    text: entry.modelData.shortcut || ""
                    color: Theme.overlay1
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 4
                }
                MouseArea {
                    id: ma
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !entry.sep && entry.on
                    onClicked: {
                        menu.close()
                        entry.modelData.act()
                    }
                }
            }
        }
    }
}
