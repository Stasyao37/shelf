import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import Shelf

Rectangle {
    id: side

    property string currentPath: ""
    signal navigate(string path)
    signal bookmarkMenu(string path, real x, real y)
    signal trashMenu(real x, real y)
    signal filesDropped(var paths, string dest, bool move)

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
        readonly property bool isTrash: modelData.path === Places.trashPath
        property bool dropHere: false

        width: column.width
        height: 32

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 6; anchors.rightMargin: 6
            radius: Theme.btnRadius(height, false, Theme.widgetRadius)
            color: item.current || item.dropHere ? Theme.activeFill(Theme.accent)
                 : ma.containsMouse ? Theme.hoverFill : "transparent"
            border.width: item.dropHere ? 2 : 0
            border.color: Theme.accent
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
                    return Places.trashFull ? "icons/trash-full.svg" : "icons/trash-can.svg"
                if (n === "drive" || n === "usb")
                    return "icons/" + n + ".svg"
                return "icons/emblems/" + n + ".svg"
            }
            readonly property bool emblem: src.indexOf("/emblems/") > 0

            FolderIcon {
                visible: glyph.src === ""
                anchors.fill: parent
                size: 22
                open: ma.containsMouse || item.current || item.dropHere
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
        // Бросить файлы в место; на корзину — убрать в корзину. Подержать — открыть
        DropArea {
            anchors.fill: parent
            onEntered: (drag) => {
                var ps = FileOps.localPaths(drag.urls)
                drag.accepted = ps.length > 0 && ps.indexOf(item.modelData.path) < 0
                    && !(item.isTrash && ps.every(function (p) { return p.startsWith(Places.trashPath + "/") }))
                item.dropHere = drag.accepted
            }
            onExited: item.dropHere = false
            onDropped: (drop) => {
                item.dropHere = false
                var ps = FileOps.localPaths(drop.urls)
                var move = item.isTrash || FileOps.dropMoves(ps, item.modelData.path)
                drop.accept(move ? Qt.MoveAction : Qt.CopyAction)
                side.filesDropped(ps, item.modelData.path, move)
            }
            Timer {
                interval: 900
                running: item.dropHere && !item.isTrash && !item.current
                onTriggered: side.navigate(item.modelData.path)
            }
        }
        MouseArea {
            id: ma
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (m) => {
                if (m.button === Qt.RightButton) {
                    var p = mapToItem(side, m.x, m.y)
                    if (item.isBookmark)
                        side.bookmarkMenu(item.modelData.path, p.x, p.y)
                    else if (item.modelData.path === Places.trashPath)
                        side.trashMenu(p.x, p.y)
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
