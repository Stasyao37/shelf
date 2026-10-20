import QtQuick
import QtQuick.Controls.Basic
import Shelf

Rectangle {
    id: side

    property string currentPath: ""
    signal navigate(string path)
    signal bookmarkMenu(string path, real x, real y)

    color: Theme.material ? Theme.surface0 : Qt.rgba(0, 0, 0, Theme.light ? 0.04 : 0.14)

    component Section: Text {
        leftPadding: 14
        topPadding: 14
        bottomPadding: 4
        color: Theme.subtext0
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 3
        font.weight: Font.DemiBold
    }

    component PlaceItem: Item {
        id: item
        required property var modelData
        property bool isBookmark: false
        readonly property bool current: side.currentPath === modelData.path

        width: column.width
        height: 32

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 6; anchors.rightMargin: 6
            radius: Theme.btnRadius(height, false, Theme.widgetRadius)
            color: item.current ? Theme.activeFill(Theme.accent)
                 : ma.containsMouse ? Theme.hoverFill : "transparent"
            Behavior on color { ColorAnimation { duration: 90 } }
        }
        Text {
            id: glyph
            x: 18; width: 20
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignHCenter
            text: item.modelData.icon
            color: item.current ? Theme.activeFg(Theme.accent) : Theme.subtext1
            font.family: Theme.iconFont
            font.pixelSize: Theme.fontSize
        }
        Text {
            anchors { left: glyph.right; leftMargin: 10; right: parent.right; rightMargin: 14; verticalCenter: parent.verticalCenter }
            text: item.modelData.name
            elide: Text.ElideRight
            color: item.current ? Theme.activeFg(Theme.accent) : Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }
        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (m) => {
                if (m.button === Qt.RightButton) {
                    if (item.isBookmark) {
                        var p = mapToItem(side, m.x, m.y)
                        side.bookmarkMenu(item.modelData.path, p.x, p.y)
                    }
                    return
                }
                side.navigate(item.modelData.path)
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.height + 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: column
            width: side.width

            Section { text: "Места" }
            Repeater {
                model: Places.places
                PlaceItem {}
            }

            Section { text: "Закладки"; visible: Places.bookmarks.length > 0 }
            Repeater {
                model: Places.bookmarks
                PlaceItem { isBookmark: true }
            }

            Section { text: "Устройства" }
            Repeater {
                model: Places.devices
                PlaceItem {}
            }
        }
    }
}
