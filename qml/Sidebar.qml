import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
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
        // Те же стеклянные иконки, что в сетке: папки — с эмблемами,
        // корзина и диски — своими картинками из icons/
        Item {
            id: glyph
            x: 16; width: 22; height: 22
            anchors.verticalCenter: parent.verticalCenter
            // У XDG-мест — одна эмблема без папки: на 22 px внутри папки её не разглядеть
            readonly property string src: {
                const n = item.modelData.icon
                if (n === "folder")
                    return ""
                if (n === "trash")
                    return "icons/trash-can.svg"
                if (n === "drive" || n === "usb")
                    return "icons/" + n + ".svg"
                return "icons/emblems/" + n + ".svg"
            }
            readonly property bool emblem: src.indexOf("/emblems/") > 0

            FolderIcon {
                visible: glyph.src === ""
                anchors.fill: parent
                size: 22
                open: ma.containsMouse || item.current
            }
            Image {
                visible: glyph.src !== "" && !glyph.emblem
                anchors.fill: parent
                source: visible ? Qt.resolvedUrl(glyph.src) : ""
                sourceSize: Qt.size(22, 22)
                layer.enabled: Theme.light
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: Qt.darker(Theme.accent, 1.6)
                }
            }

            // Эмблема рисовалась плоским белым силуэтом для папки. Здесь она
            // служит маской для того же стекла, что у корзины и дисков: белый
            // по диагонали из левого верхнего угла. Линии тоньше сплошных фигур
            // и без кромки, поэтому альфа чуть выше, чем 0.6→0.3 у набора
            Image {
                id: emblemMask
                visible: false
                anchors.centerIn: parent
                width: 19; height: 19
                source: glyph.emblem ? Qt.resolvedUrl(glyph.src) : ""
                sourceSize: Qt.size(width, height)
                layer.enabled: true
            }
            Item {
                visible: glyph.emblem
                anchors.fill: emblemMask
                clip: true
                layer.enabled: visible
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: emblemMask
                }
                Rectangle {
                    id: glassFill
                    // Повёрнутый квадрат с вертикальным градиентом даёт диагональный
                    width: parent.width * 1.5; height: width
                    anchors.centerIn: parent
                    rotation: -45
                    readonly property color base: Theme.light ? Qt.darker(Theme.accent, 1.6) : "white"
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.alpha(glassFill.base, 0.75) }
                        GradientStop { position: 1.0; color: Qt.alpha(glassFill.base, 0.4) }
                    }
                }
            }
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
