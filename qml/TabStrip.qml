pragma ComponentBehavior: Bound

import QtQuick
import Shelf

// Полоска вкладок над панелью пути. Вкладки тянутся мышью, средняя кнопка
// закрывает; если держать над вкладкой файлы — она откроется, можно бросить в её папку
Item {
    id: strip

    property var tabs: []        // FolderTab
    // Текущая вкладка — сам объект, а не номер: при перестановке кнопки пересоздаются
    // по новому списку раньше, чем приходит новый номер, и сравнение номеров на миг
    // подсвечивало соседнюю вкладку (issue #1)
    property var currentTab: null

    signal activate(int index)
    signal closeTab(int index)
    signal moveTab(int from, int to)
    signal newTab()
    signal filesDropped(var paths, string dest, bool move)

    // Перетаскивание вкладки: откуда и на какое место. Остальные отъезжают,
    // освобождая слот; порядок меняется, когда вкладка доехала до места
    property int dragFrom: -1
    property int dragTo: -1
    function slotOf(i) {
        if (dragFrom < 0) return i
        if (i === dragFrom) return dragTo
        if (dragFrom < dragTo && i > dragFrom && i <= dragTo) return i - 1
        if (dragTo < dragFrom && i >= dragTo && i < dragFrom) return i + 1
        return i
    }
    Timer {
        id: commitMove
        interval: 200   // чуть дольше анимации x
        onTriggered: {
            var from = strip.dragFrom, to = strip.dragTo
            strip.dragFrom = -1
            strip.dragTo = -1
            if (from >= 0 && to !== from) strip.moveTab(from, to)
        }
    }

    readonly property real tabWidth: Math.max(96, Math.min(220, (width - addBtn.width - 6) / Math.max(1, tabs.length)))

    // Кнопки строятся по shown, а не прямо по tabs: Repeater пересоздаёт их при каждом
    // изменении списка, и соседи закрытой вкладки прыгали бы на место. Перед сменой
    // запоминаем, где стояла и какой ширины была каждая, — новые кнопки доезжают оттуда
    // (Translate и добавка к ширине; привязку x, на которой держится перетаскивание, не трогаем)
    property var shown: []
    property var _was: []   // [{ tab, x, w }]
    property bool _ready: false
    onTabsChanged: {
        var was = []
        for (var i = 0; i < tabRep.count; ++i) {
            var it = tabRep.itemAt(i)
            if (it) was.push({ tab: it.modelData, x: it.x + it.slideX, w: it.width })
        }
        _was = was
        shown = tabs
        if (_ready) { _moving = true; movingDone.restart() }
    }
    // Список только что сменился — кнопка «+» едет к новому месту, а не прыгает
    property bool _moving: false
    Timer { id: movingDone; interval: 260; onTriggered: strip._moving = false }
    Component.onCompleted: Qt.callLater(function () { strip._ready = true })

    Repeater {
        id: tabRep
        model: strip.shown

        delegate: Item {
            id: tabItem
            required property var modelData
            required property int index
            readonly property bool current: modelData === strip.currentTab
            readonly property bool dropHere: drop.containsDrag
            property bool dragging: false
            property real dragX: 0
            property real slideX: 0
            property real growW: 0

            width: strip.tabWidth + growW
            height: strip.height
            transform: Translate { x: tabItem.slideX }
            Component.onCompleted: {
                for (var i = 0; i < strip._was.length; ++i) {
                    var w = strip._was[i]
                    if (w.tab !== modelData) continue
                    slideX = w.x - x
                    growW = w.w - width
                    if (slideX !== 0 || growW !== 0) settle.start()
                    return
                }
                // Новая вкладка проявляется, чуть въезжая слева, — когда её папка уже нарисована
                if (!strip._ready) return
                opacity = 0
                slideX = -14
                Theme.afterFrame(tabItem, function () { appear.start() })
            }
            ParallelAnimation {
                id: settle
                NumberAnimation { target: tabItem; property: "slideX"; to: 0; duration: Theme.dur(200); easing.type: Easing.OutCubic }
                NumberAnimation { target: tabItem; property: "growW"; to: 0; duration: Theme.dur(200); easing.type: Easing.OutCubic }
            }
            ParallelAnimation {
                id: appear
                NumberAnimation { target: tabItem; property: "opacity"; to: 1; duration: Theme.dur(180); easing.type: Easing.OutCubic }
                NumberAnimation { target: tabItem; property: "slideX"; to: 0; duration: Theme.dur(220); easing.type: Easing.OutCubic }
            }
            x: dragging ? dragX : strip.slotOf(index) * strip.tabWidth
            z: dragging || index === strip.dragFrom ? 2 : 0
            Behavior on x { enabled: !tabItem.dragging; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

            // Едущая вкладка проходит над соседями — плотная подложка, чтобы их подписи не просвечивали
            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                visible: tabItem.index === strip.dragFrom
                color: Theme.material ? Theme.surface1 : Theme.base
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                color: tabItem.current || tabItem.dropHere ? Theme.activeFill(Theme.accent)
                     : ma.containsMouse ? Theme.hoverFill : "transparent"
                border.width: tabItem.dropHere ? 2 : 1
                border.color: tabItem.dropHere ? Theme.accent
                            : tabItem.current ? Theme.edge(Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, Theme.activeBorderAlpha))
                            : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
            }

            Text {
                id: glyph
                x: 12
                anchors.verticalCenter: parent.verticalCenter
                text: tabItem.modelData.dir.searchQuery ? "\u{f002}"
                    : tabItem.modelData.dir.path.startsWith(Places.trashPath) ? "\u{f1f8}" : "\u{f07b}"
                color: tabItem.current ? Theme.activeFg(Theme.accent) : Theme.subtext0
                font.family: Theme.iconFont
                font.pixelSize: Theme.fontSize - 3
            }
            Text {
                anchors { left: glyph.right; leftMargin: 8; right: closeBtn.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
                text: tabItem.modelData.title
                elide: Text.ElideRight
                color: tabItem.current ? Theme.activeFg(Theme.accent) : Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }

            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                // В координатах полоски: сама вкладка едет под курсором
                property real pressX: 0
                onPressed: (m) => {
                    pressX = mapToItem(strip, m.x, 0).x
                    if (m.button === Qt.LeftButton) strip.activate(tabItem.index)
                }
                onPositionChanged: (m) => {
                    if (!(m.buttons & Qt.LeftButton) || strip.tabs.length < 2 || commitMove.running) return
                    var dx = mapToItem(strip, m.x, 0).x - pressX
                    if (!tabItem.dragging && Math.abs(dx) < Application.styleHints.startDragDistance) return
                    tabItem.dragging = true
                    var maxX = (strip.tabs.length - 1) * strip.tabWidth
                    tabItem.dragX = Math.max(0, Math.min(maxX, tabItem.index * strip.tabWidth + dx))
                    strip.dragFrom = tabItem.index
                    strip.dragTo = Math.round(tabItem.dragX / strip.tabWidth)
                }
                onReleased: {
                    if (!tabItem.dragging) return
                    // Отпустили — x снова от slotOf, Behavior довозит вкладку до слота
                    tabItem.dragging = false
                    commitMove.start()
                }
                onClicked: (m) => { if (m.button === Qt.MiddleButton) strip.closeTab(tabItem.index) }
            }

            Rectangle {
                id: closeBtn
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                width: 20; height: 20
                radius: 10
                visible: tabItem.current || ma.containsMouse || closeMa.containsMouse
                color: closeMa.containsMouse ? Theme.hoverFill : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "\u{f00d}"
                    color: closeMa.containsMouse ? Theme.text : Theme.subtext0
                    font.family: Theme.iconFont
                    font.pixelSize: Theme.fontSize - 4
                }
                MouseArea {
                    id: closeMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: strip.closeTab(tabItem.index)
                }
            }

            DropArea {
                id: drop
                anchors.fill: parent
                onEntered: (drag) => {
                    var ps = FileOps.localPaths(drag.urls)
                    var dest = tabItem.modelData.dir.path
                    drag.accepted = ps.length > 0 && ps.indexOf(dest) < 0 && !dest.startsWith(Places.trashPath + "/")
                }
                onDropped: (d) => {
                    var ps = FileOps.localPaths(d.urls)
                    var dest = tabItem.modelData.dir.path
                    var move = dest === Places.trashPath || FileOps.dropMoves(ps, dest)
                    d.accept(move ? Qt.MoveAction : Qt.CopyAction)
                    strip.filesDropped(ps, dest, move)
                }
                Timer {
                    interval: 600
                    running: tabItem.dropHere && !tabItem.current
                    onTriggered: strip.activate(tabItem.index)
                }
            }
        }
    }

    GlassButton {
        id: addBtn
        x: Math.min(strip.tabs.length * strip.tabWidth + 2, strip.width - width)
        // Едет вместе с вкладками; при перетаскивании края окна — без задержки
        Behavior on x { enabled: strip._moving; NumberAnimation { duration: Theme.dur(200); easing.type: Easing.OutCubic } }
        anchors.verticalCenter: parent.verticalCenter
        text: "\u{f067}"
        iconSize: Theme.fontSize - 2
        tooltip: qsTr("Новая вкладка (Ctrl+T)")
        onClicked: strip.newTab()
    }
}
