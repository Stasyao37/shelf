import QtQuick
import QtQuick.Controls.Basic
import Shelf

Rectangle {
    id: side

    property string currentPath: ""
    signal navigate(string path)
    signal bookmarkMenu(string path, real x, real y)
    signal trashMenu(real x, real y)
    signal filesDropped(var paths, string dest, bool move)
    signal openInNewTab(string path)
    signal openSettings()

    color: Theme.material ? Theme.surface0 : Qt.rgba(0, 0, 0, Theme.light ? 0.04 : 0.14)

    // shown = false — заголовок плавно сворачивается (пустые «Закладки»)
    component Section: Text {
        id: sec
        property bool shown: true
        height: shown ? implicitHeight : 0
        opacity: shown ? 1 : 0
        visible: height > 0.5
        clip: true
        Behavior on height { NumberAnimation { duration: Theme.dur(200); easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Theme.dur(160) } }
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

        // Новая строка выезжает из-под предыдущей, раздвигая соседей; ушедшая — сворачивается.
        // Только высота, прозрачность и сдвиг — без слоёв и шейдеров
        property bool entering: false
        property bool leaving: false
        property real shown: entering ? 0 : 1
        width: column.width
        height: 32 * shown
        opacity: shown
        clip: shown < 1
        enabled: !leaving
        transform: Translate { y: -10 * (1 - item.shown) }
        NumberAnimation {
            running: item.entering
            target: item; property: "shown"; to: 1; duration: Theme.dur(240); easing.type: Easing.OutCubic
        }
        NumberAnimation {
            running: item.leaving
            target: item; property: "shown"; to: 0; duration: Theme.dur(200); easing.type: Easing.InOutCubic
        }

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
            // Картинка icons/<src>.svg; "" — папка
            readonly property string src: {
                const n = item.modelData.icon
                if (n === "folder")
                    return ""
                if (n === "trash")
                    return Places.trashFull ? "trash-full" : "trash-can"
                if (n === "drive" || n === "usb")
                    return n
                return "emblems/" + n
            }
            readonly property bool emblem: src.startsWith("emblems/")

            FolderIcon {
                id: folderIcon
                visible: glyph.src === ""
                anchors.fill: parent
                size: 22
                open: ma.containsMouse || item.current || item.dropHere
            }
            // Корзина, диски и эмблемы мест — тот же оттенок палитры, что у папок
            Image {
                visible: glyph.src !== "" && !glyph.emblem
                anchors.fill: parent
                source: visible ? Theme.glassIcon(glyph.src, Theme.groupTint("folder")) : ""
                // Вдвое крупнее и со сглаживанием — края мягкие при любом масштабе экрана
                sourceSize: Qt.size(width * 2, height * 2)
                smooth: true
                mipmap: true
            }
            // Эмблема — та же вставка из матового стекла, что на папках в сетке:
            // мягкая маска без ступенек, вертикальная заливка, лёгкая тень
            GlassEmblem {
                visible: glyph.emblem
                anchors.centerIn: parent
                width: 19; height: 19
                name: glyph.emblem ? glyph.src : ""
                tint: Theme.groupTint("folder")
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
                if (folderIcon.visible) folderIcon.swallow(ps)
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
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            onClicked: (m) => {
                if (m.button === Qt.MiddleButton) {
                    side.openInNewTab(item.modelData.path)
                    return
                }
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

    KeyedModel { id: bookmarks; source: Places.bookmarks }
    KeyedModel { id: devices; source: Places.devices }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: settingsBtn.top; bottomMargin: 4 }
        contentHeight: column.height + 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: column
            width: side.width

            Section { text: qsTr("Места") }
            Repeater {
                model: Places.places
                PlaceItem {}
            }

            Section { text: qsTr("Закладки"); shown: Places.bookmarks.length > 0 }
            Repeater {
                model: bookmarks.model
                PlaceItem {
                    required property string key
                    required entering
                    required leaving
                    modelData: { bookmarks.rev; return bookmarks.item(key) }
                    isBookmark: true
                }
            }

            Section { text: qsTr("Устройства") }
            Repeater {
                model: devices.model
                PlaceItem {
                    required property string key
                    required entering
                    required leaving
                    modelData: { devices.rev; return devices.item(key) }
                }
            }
        }
    }

    // Настройки — небольшая шестерёнка внизу слева, как в других программах
    GlassButton {
        id: settingsBtn
        anchors { left: parent.left; bottom: parent.bottom; leftMargin: 8; bottomMargin: 6 }
        implicitWidth: 28; implicitHeight: 26
        text: "\u{f013}"
        iconSize: Theme.fontSize - 2
        tooltip: qsTr("Настройки (Ctrl+,)")
        onClicked: side.openSettings()
    }
}
