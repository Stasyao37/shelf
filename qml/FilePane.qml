import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Список файлов (сетка или строки) с выделением и управлением с клавиатуры.
// Что делать с файлами, решает Main — сюда приходят только сигналы.
FocusScope {
    id: pane

    required property DirModel model
    property bool gridMode: true
    // Сторона значка в сетке; iconShown — она же с анимацией, по ней раскладка.
    // Миниатюры заказываем по iconSize, чтобы не дёргать кэш на каждом кадре
    property int iconSize: 56
    property real iconShown: iconSize
    Behavior on iconShown { NumberAnimation { duration: Theme.dur(160); easing.type: Easing.OutCubic } }
    // Ctrl+колесо: +1 крупнее, -1 мельче
    signal zoomRequested(int step)
    // При масштабе держим на месте первый видимый значок
    property int zoomAnchor: -1
    onIconSizeChanged: zoomAnchor = view.indexAt(view.width / 2, view.contentY + 4)
    onIconShownChanged: {
        if (zoomAnchor >= 0 && gridMode)
            view.positionViewAtIndex(zoomAnchor, GridView.Beginning)
        if (iconShown === iconSize)
            zoomAnchor = -1
    }
    property string renamingPath: ""
    // Окно выбора одного файла: Ctrl/Shift не добавляют к выделению
    property bool singleSelect: false

    // path → true; объект пересоздаётся при каждом изменении, чтобы сработали привязки
    property var selected: ({})
    property int anchorRow: -1
    readonly property int currentRow: view.currentIndex
    readonly property int selectedCount: Object.keys(selected).length
    // Колонка «Тип» — только когда окну хватает ширины
    readonly property bool wideList: view.width > 640
    readonly property int nameRight: wideList ? 330 : 210

    signal activated(int row)
    signal contextMenuRequested(int row, real x, real y)   // row = -1 — пустое место
    // blur — поле закрыли кликом мимо: фокус уже у того, куда щёлкнули, не забираем его
    signal renameRequested(string path, string newName, bool blur)
    signal typed(string text)
    signal filesDropped(var paths, string dest, bool move)
    signal springOpen(string path)   // держали файлы над папкой — открыть её
    signal openInNewTab(string path)  // средняя кнопка по папке

    // ── Перетаскивание ───────────────────────────────────────────────
    property var dragPaths: []
    property string dropFolder: ""   // папка под курсором, куда упадут файлы
    readonly property bool dropBlocked: model.path.startsWith(Places.trashPath + "/")

    function startDrag(iconItem) {
        if (dragPaths.length > 0) return
        var paths = selectedPaths()
        if (paths.length === 0) return
        dragPaths = paths
        iconItem.grabToImage(function (r) {
            dragProxy.Drag.imageSource = r.url
            dragProxy.Drag.hotSpot = Qt.point(iconItem.width / 2, iconItem.height / 2)
            dragProxy.Drag.mimeData = { "text/uri-list": FileOps.uriList(paths) }
            // Системный drag: блокирует до отпускания, работает и с другими приложениями
            dragProxy.Drag.active = true
            dragProxy.Drag.startDrag()
            dragProxy.Drag.active = false
            pane.dragPaths = []
            pane.dropFolder = ""
        })
    }
    function acceptsDrop(drag, dest) {
        if (dropBlocked || dest.startsWith(Places.trashPath + "/")) return false
        var ps = FileOps.localPaths(drag.urls)
        // Папку в саму себя не бросаем
        return ps.length > 0 && ps.indexOf(dest) < 0
    }
    // Возвращает брошенные пути (пусто — бросать нечего)
    function handleDrop(drop, dest) {
        dropFolder = ""
        var ps = FileOps.localPaths(drop.urls)
        if (ps.length === 0) return ps
        var move = FileOps.dropMoves(ps, dest)
        drop.accept(move ? Qt.MoveAction : Qt.CopyAction)
        filesDropped(ps, dest, move)
        return ps
    }

    Item {
        id: dragProxy
        // None: запускаем сами через startDrag(), без внутреннего (только QML) drag
        Drag.dragType: Drag.None
        Drag.supportedActions: Qt.CopyAction | Qt.MoveAction
    }

    // ── Выделение ────────────────────────────────────────────────────
    function isSelected(p) { return selected[p] === true }
    function selectedPaths() {
        var res = []
        for (var i = 0; i < model.count; ++i) {
            var p = model.pathAt(i)
            if (selected[p]) res.push(p)
        }
        return res
    }
    function selectOnly(row) {
        var s = {}
        var p = model.pathAt(row)
        if (p) s[p] = true
        selected = s
        anchorRow = row
        view.currentIndex = row
    }
    function toggleRow(row) {
        if (singleSelect) { selectOnly(row); return }
        var s = Object.assign({}, selected)
        var p = model.pathAt(row)
        if (s[p]) delete s[p]; else s[p] = true
        selected = s
        anchorRow = row
        view.currentIndex = row
    }
    function selectRange(row) {
        if (singleSelect) { selectOnly(row); return }
        if (anchorRow < 0) anchorRow = row
        var s = {}
        for (var i = Math.min(anchorRow, row); i <= Math.max(anchorRow, row); ++i)
            s[model.pathAt(i)] = true
        selected = s
        view.currentIndex = row
    }
    function selectAll() {
        var s = {}
        for (var i = 0; i < model.count; ++i) s[model.pathAt(i)] = true
        selected = s
    }
    function clearSelection() { selected = ({}); anchorRow = -1 }
    function selectPaths(paths) {
        var s = {}
        var first = -1
        for (var i = 0; i < paths.length; ++i) {
            var row = model.indexOfPath(paths[i])
            if (row < 0) continue
            s[paths[i]] = true
            if (first < 0 || row < first) first = row
        }
        selected = s
        if (first >= 0) {
            anchorRow = first
            view.currentIndex = first
            view.positionViewAtIndex(first, GridView.Contain)
        }
        return first >= 0
    }
    function selectRow(row) {
        if (model.count === 0) { clearSelection(); return }
        row = Math.max(0, Math.min(row, model.count - 1))
        selectOnly(row)
        view.positionViewAtIndex(row, GridView.Contain)
    }
    // После перечитывания папки — выкинуть исчезнувшее
    function prune() {
        var s = {}
        var changed = false
        for (var p in selected) {
            if (model.indexOfPath(p) >= 0) s[p] = true
            else changed = true
        }
        if (changed) selected = s
        if (view.currentIndex >= model.count) view.currentIndex = model.count - 1
    }
    function focusView() { view.forceActiveFocus() }
    // Клик мимо поля переименования — как Enter. Сам по себе фокус не уйдёт: поле внутри
    // GridView, а view.forceActiveFocus() отдаёт его обратно тому, кто в view уже в фокусе
    property TextInput renameField: null
    function commitRename() {
        if (renamingPath && renameField) renameRequested(renamingPath, renameField.text, true)
    }

    // Отмена переноса в папку: листки вылетают из той папки здесь, куда переносили.
    // paths — что исчезнет при отмене (FileOps.undoTargets)
    function flyOut(paths) {
        var base = model.path === "/" ? "/" : model.path + "/"
        var byFolder = {}
        for (var i = 0; i < paths.length; ++i) {
            if (!paths[i].startsWith(base)) continue
            var rest = paths[i].substring(base.length).split("/")
            if (rest.length < 2) continue
            var folder = base + rest[0], item = folder + "/" + rest[1]
            var list = byFolder[folder] || (byFolder[folder] = [])
            if (list.indexOf(item) < 0) list.push(item)
        }
        for (var f in byFolder) {
            var cell = view.itemAtIndex(model.indexOfPath(f))
            if (cell && cell.folder) cell.folder.spit(byFolder[f])
        }
    }

    // ── Анимации ─────────────────────────────────────────────────────
    // Переход в другую папку: содержимое проявляется со сдвигом — вглубь справа (dir 1),
    // наверх слева (-1), без сдвига (0). Пока идёт переход, анимации появления
    // отдельных файлов выключены, чтобы не двигалось всё сразу
    property bool navigating: false
    // Переход перестраивает модель целиком (сброс), и add/remove не срабатывают; отключать
    // переходы на ходу нельзя — GridView бросает идущую анимацию и оставляет строку висеть
    readonly property bool itemMotion: Theme.animations
    function playEnter(dir) {
        navigating = true
        viewShift.x = 18 * dir
        view.opacity = 0.35
        enterAnim.stop()
        // Новая папка только что построена синхронно — едем, когда её кадр уже на экране
        Theme.afterFrame(pane, function () { enterAnim.restart() })
    }
    ParallelAnimation {
        id: enterAnim
        NumberAnimation { target: view; property: "opacity"; to: 1; duration: Theme.dur(200); easing.type: Easing.OutCubic }
        NumberAnimation { target: viewShift; property: "x"; to: 0; duration: Theme.dur(220); easing.type: Easing.OutCubic }
        onFinished: pane.navigating = false
    }
    // Сетка ↔ список — то же перетекание без сдвига
    onGridModeChanged: playEnter(0)

    // ── Заголовок колонок (режим списка) ─────────────────────────────
    Item {
        id: header
        visible: !pane.gridMode
        height: visible ? 28 : 0
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 8; rightMargin: 14 }

        component HeaderCell: Item {
            id: cell
            property string label
            property string key
            property int align: Text.AlignLeft
            readonly property bool sorted: pane.model.sortKey === key
            height: header.height
            Text {
                anchors.fill: parent
                anchors.leftMargin: 6; anchors.rightMargin: 6
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: cell.align
                text: cell.label + (cell.sorted ? (pane.model.sortDesc ? "  \u{f0d7}" : "  \u{f0d8}") : "")
                color: cell.sorted ? Theme.accent : Theme.subtext0
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
                elide: Text.ElideRight
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (cell.sorted) pane.model.sortDesc = !pane.model.sortDesc
                    else { pane.model.sortKey = cell.key; pane.model.sortDesc = false }
                }
            }
        }

        HeaderCell { label: qsTr("Имя"); key: "name"; x: 0; width: parent.width - pane.nameRight }
        HeaderCell { label: pane.model.trashRoot ? qsTr("Откуда") : qsTr("Тип"); key: "type"; x: parent.width - 330; width: 120; visible: pane.wideList }
        HeaderCell {
            label: pane.model.searchQuery && !pane.wideList ? qsTr("Где") : pane.model.trashRoot ? qsTr("Удалён") : qsTr("Изменён")
            key: "date"; x: parent.width - 210; width: 120
        }
        HeaderCell { label: qsTr("Размер"); key: "size"; x: parent.width - 90; width: 90; align: Text.AlignRight }
    }

    // Клик по пустому месту: снять выделение / меню папки
    MouseArea {
        anchors.fill: view
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: { pane.commitRename(); view.forceActiveFocus() }
        onClicked: (m) => {
            if (!(m.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) pane.clearSelection()
            if (m.button === Qt.RightButton) {
                var p = mapToItem(pane, m.x, m.y)
                pane.contextMenuRequested(-1, p.x, p.y)
            }
        }
    }

    // Бросили на пустое место — в открытую папку
    DropArea {
        id: bgDrop
        anchors.fill: view
        onEntered: (drag) => { drag.accepted = pane.acceptsDrop(drag, pane.model.path) }
        onDropped: (drop) => pane.handleDrop(drop, pane.model.path)
    }
    Rectangle {
        anchors.fill: view
        anchors.rightMargin: 6
        visible: bgDrop.containsDrag && pane.dropFolder === ""
        color: "transparent"
        radius: Theme.widgetRadius + 2
        border.width: 2
        border.color: Qt.alpha(Theme.accent, 0.6)
    }

    GridView {
        id: view
        transform: Translate { id: viewShift }

        // Новые файлы проявляются и подрастают, удалённые гаснут, соседи плавно уступают место
        add: Transition {
            enabled: pane.itemMotion
            NumberAnimation { property: "appear"; from: 0; to: 1; duration: Theme.dur(200); easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.86; to: 1; duration: Theme.dur(220); easing.type: Easing.OutCubic }
        }
        remove: Transition {
            enabled: pane.itemMotion
            NumberAnimation { property: "appear"; to: 0; duration: Theme.dur(160); easing.type: Easing.InCubic }
            NumberAnimation { property: "scale"; to: 0.86; duration: Theme.dur(160); easing.type: Easing.InCubic }
        }
        displaced: Transition {
            enabled: pane.itemMotion
            NumberAnimation { properties: "x,y"; duration: Theme.dur(220); easing.type: Easing.OutCubic }
        }
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.leftMargin: 8
        clip: true
        focus: true
        model: pane.model
        // Мышью не «тащим» список, как на телефоне — только колесо/тачпад
        interactive: false
        currentIndex: -1
        keyNavigationEnabled: false
        highlightFollowsCurrentItem: false
        reuseItems: true

        // При значке 56 — ячейка 112×118, остальные размеры — пропорционально значку
        readonly property int gridCell: Math.round(pane.iconShown) + 56
        cellWidth: pane.gridMode
            ? Math.floor((width - 14) / Math.max(1, Math.floor((width - 14) / gridCell)))
            : width - 14
        cellHeight: pane.gridMode ? Math.round(pane.iconShown) + 62 : 30

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 6
                radius: 3
                color: Theme.ink(parent.pressed ? 0.45 : 0.25)
            }
        }

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            property real zoomAcc: 0
            onWheel: (ev) => {
                if (ev.modifiers & Qt.ControlModifier) {
                    // Тачпад шлёт мелкие шаги — копим до щелчка колеса
                    zoomAcc += ev.angleDelta.y !== 0 ? ev.angleDelta.y : ev.pixelDelta.y * 4
                    if (Math.abs(zoomAcc) >= 120) {
                        pane.zoomRequested(zoomAcc > 0 ? 1 : -1)
                        zoomAcc = 0
                    }
                    return
                }
                zoomAcc = 0
                var d = ev.pixelDelta.y !== 0 ? ev.pixelDelta.y : ev.angleDelta.y / 120 * 90
                var maxY = Math.max(view.originY, view.originY + view.contentHeight - view.height)
                view.contentY = Math.max(view.originY, Math.min(view.contentY - d, maxY))
            }
        }

        Keys.onPressed: (e) => {
            var n = pane.model.count
            var cols = pane.gridMode ? Math.max(1, Math.floor((view.width - 14) / view.cellWidth)) : 1
            var page = Math.max(1, Math.floor(view.height / view.cellHeight)) * cols
            var cur = view.currentIndex
            var target = -2
            switch (e.key) {
            case Qt.Key_Left:     if (pane.gridMode && !(e.modifiers & Qt.AltModifier)) target = cur - 1; break
            case Qt.Key_Right:    if (pane.gridMode && !(e.modifiers & Qt.AltModifier)) target = cur + 1; break
            case Qt.Key_Up:       if (!(e.modifiers & Qt.AltModifier)) target = cur < 0 ? 0 : cur - cols; break
            case Qt.Key_Down:     target = cur < 0 ? 0 : cur + cols; break
            case Qt.Key_Home:     target = 0; break
            case Qt.Key_End:      target = n - 1; break
            case Qt.Key_PageUp:   target = cur - page; break
            case Qt.Key_PageDown: target = cur + page; break
            case Qt.Key_Return:
            case Qt.Key_Enter:
                if (cur >= 0) pane.activated(cur)
                e.accepted = true
                return
            default:
                if (e.text.length === 1 && e.text.charCodeAt(0) > 32 && e.text.charCodeAt(0) !== 127
                        && !(e.modifiers & (Qt.ControlModifier | Qt.AltModifier))) {
                    pane.typed(e.text)
                    e.accepted = true
                }
                return
            }
            if (target === -2 || n === 0) return
            target = Math.max(0, Math.min(target, n - 1))
            if (e.modifiers & Qt.ShiftModifier) pane.selectRange(target)
            else pane.selectOnly(target)
            view.positionViewAtIndex(target, GridView.Contain)
            e.accepted = true
        }

        delegate: Item {
            id: cell
            required property int index
            required property string name
            required property string path
            required property string dir
            required property bool isDir
            required property bool isHidden
            required property bool isLink
            required property bool isLocked
            required property double mtime
            required property string sizeText
            required property string dateText
            required property string mimeComment
            required property string mimeType
            required property string icon
            required property string origin

            readonly property bool selected: pane.isSelected(path)
            readonly property bool renaming: pane.renamingPath === path
            readonly property bool searchMode: pane.model.searchQuery.length > 0
            readonly property bool dropHere: pane.dropFolder === path
            readonly property FolderIcon folder: folderLoader.item as FolderIcon

            width: view.cellWidth
            height: view.cellHeight
            // appear — для анимаций появления и удаления; у скрытых файлов своя полупрозрачность
            property real appear: 1
            opacity: (isHidden ? 0.6 : 1.0) * appear
            // Делегаты переиспользуются: возвращаем то, что могла оставить анимация удаления
            GridView.onReused: { appear = 1; scale = 1 }

            Rectangle {
                anchors.fill: parent
                anchors.margins: pane.gridMode ? 3 : 1
                radius: Theme.widgetRadius
                color: cell.selected || cell.dropHere ? Theme.activeFill(Theme.accent)
                     : ma.containsMouse ? Theme.hoverFill : "transparent"
                border.width: cell.dropHere ? 2 : 1
                border.color: cell.dropHere ? Theme.accent : cell.selected
                    ? Theme.edge(Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, Theme.activeBorderAlpha))
                    : (view.currentIndex === cell.index && view.activeFocus ? Theme.restBorder : "transparent")
                Behavior on color { ColorAnimation { duration: 90 } }
            }

            Item {
                id: iconBox
                readonly property int size: pane.gridMode ? Math.round(pane.iconShown) : 22
                width: size; height: size
                x: pane.gridMode ? (parent.width - width) / 2 : 8
                y: pane.gridMode ? 10 : (parent.height - height) / 2

                // Создаётся только нужное: у папки — FolderIcon, у файла — FileIcon и миниатюра,
                // если она есть. Раньше в каждой плитке жило всё сразу (лишнее — скрытым,
                // со своими слоями и эффектами), и открытие папки стоило ~3 мс на плитку
                Loader {
                    id: folderLoader
                    active: cell.isDir
                    sourceComponent: FolderIcon {
                        size: iconBox.size
                        iconName: cell.icon
                        open: ma.containsMouse || cell.selected || cell.dropHere
                    }
                }
                Loader {
                    active: !cell.isDir
                    sourceComponent: Item {
                        width: iconBox.size
                        height: iconBox.size
                        FileIcon {
                            visible: !(thumbLoader.item as Thumbnail)?.ready
                            size: iconBox.size
                            mimeType: cell.mimeType
                            fileName: cell.name
                        }
                        Loader {
                            id: thumbLoader
                            // revision и mtime — чтобы перечитать, когда tumbler закончит или файл изменится.
                            // Они в самом выражении: голые строки-зависимости компилятор QML выбрасывает
                            readonly property string src: Thumbnailer.revision < 0 || cell.mtime < 0
                                ? "" : Thumbnailer.thumbnail(cell.path, cell.mimeType, pane.gridMode ? pane.iconSize : 22)
                            active: src !== ""
                            sourceComponent: Thumbnail {
                                size: iconBox.size
                                video: cell.mimeType.startsWith("video/")
                                source: thumbLoader.src
                            }
                        }
                    }
                }

                // Ссылка — слева внизу, замок — справа внизу: оба могут быть сразу
                Loader {
                    active: cell.isLink
                    anchors { left: parent.left; bottom: parent.bottom; leftMargin: pane.gridMode ? -2 : -3 }
                    sourceComponent: Badge {
                        name: "link"
                        size: pane.gridMode ? Math.round(iconBox.size * 0.36) : 11
                    }
                }
                Loader {
                    active: cell.isLocked
                    anchors { right: parent.right; bottom: parent.bottom; rightMargin: pane.gridMode ? -2 : -3 }
                    sourceComponent: Badge {
                        name: "lock"
                        size: pane.gridMode ? Math.round(iconBox.size * 0.36) : 11
                    }
                }
            }

            // Имя: в сетке — под иконкой в две строки, в списке — колонкой
            Text {
                visible: !cell.renaming
                text: cell.name
                color: cell.selected ? Theme.activeFg(Theme.accent) : Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                elide: pane.gridMode ? Text.ElideMiddle : Text.ElideRight
                wrapMode: pane.gridMode ? Text.Wrap : Text.NoWrap
                maximumLineCount: pane.gridMode ? 2 : 1
                horizontalAlignment: pane.gridMode ? Text.AlignHCenter : Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                x: pane.gridMode ? 6 : 38
                y: pane.gridMode ? iconBox.size + 16 : 0
                width: pane.gridMode ? parent.width - 12 : parent.width - 38 - pane.nameRight - 6
                height: pane.gridMode ? 40 : parent.height
            }

            // В поиске — где лежит файл (вместо типа в списке, подсказкой в сетке)
            Text {
                visible: !pane.gridMode && pane.wideList
                x: parent.width - 330; width: 116
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                readonly property bool showsPath: cell.searchMode || cell.origin.length > 0
                text: cell.searchMode ? cell.dir.replace(/^\/home\/[^/]+/, "~")
                    : cell.origin ? cell.origin.replace(/^\/home\/[^/]+/, "~") : cell.mimeComment
                color: Theme.subtext0
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
                elide: showsPath ? Text.ElideLeft : Text.ElideRight
            }
            Text {
                visible: !pane.gridMode
                x: parent.width - 210; width: 120
                height: parent.height
                leftPadding: 6
                verticalAlignment: Text.AlignVCenter
                // Узкий список в поиске: колонки «Тип» нет — папку показываем здесь
                text: cell.searchMode && !pane.wideList ? cell.dir.replace(/^\/home\/[^/]+/, "~") : cell.dateText
                elide: cell.searchMode && !pane.wideList ? Text.ElideLeft : Text.ElideRight
                color: Theme.subtext0
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
            }
            Text {
                visible: !pane.gridMode
                x: parent.width - 90; width: 84
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignRight
                text: cell.sizeText
                color: Theme.subtext0
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
            }

            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                property point pressPos
                onPressed: (m) => { if (!cell.renaming) pane.commitRename(); view.forceActiveFocus(); pressPos = Qt.point(m.x, m.y) }
                onPositionChanged: (m) => {
                    if (!(m.buttons & Qt.LeftButton) || cell.renaming || pane.dragPaths.length > 0) return
                    if (Math.abs(m.x - pressPos.x) + Math.abs(m.y - pressPos.y) < Application.styleHints.startDragDistance) return
                    if (!cell.selected) pane.selectOnly(cell.index)
                    pane.startDrag(iconBox)
                }
                onClicked: (m) => {
                    if (m.button === Qt.MiddleButton) {
                        if (cell.isDir) pane.openInNewTab(cell.path)
                        return
                    }
                    if (m.button === Qt.RightButton) {
                        if (!cell.selected) pane.selectOnly(cell.index)
                        var p = mapToItem(pane, m.x, m.y)
                        pane.contextMenuRequested(cell.index, p.x, p.y)
                        return
                    }
                    if (m.modifiers & Qt.ControlModifier) pane.toggleRow(cell.index)
                    else if (m.modifiers & Qt.ShiftModifier) pane.selectRange(cell.index)
                    else pane.selectOnly(cell.index)
                }
                onDoubleClicked: (m) => { if (m.button === Qt.LeftButton) pane.activated(cell.index) }
            }

            // На папку можно бросить файлы; подержать над ней — откроется
            DropArea {
                anchors.fill: parent
                enabled: cell.isDir
                onEntered: (drag) => {
                    drag.accepted = pane.acceptsDrop(drag, cell.path)
                    if (drag.accepted) pane.dropFolder = cell.path
                }
                onExited: if (pane.dropFolder === cell.path) pane.dropFolder = ""
                onDropped: (drop) => {
                    const dropped = pane.handleDrop(drop, cell.path)
                    if (cell.folder) cell.folder.swallow(dropped)
                }
                Timer {
                    interval: 900
                    running: cell.dropHere
                    onTriggered: { pane.dropFolder = ""; pane.springOpen(cell.path) }
                }
            }

            // Подсказка нужна только в сетке результатов поиска — в остальных плитках её нет
            Loader {
                active: pane.gridMode && cell.searchMode
                sourceComponent: ToolTip {
                    id: whereTip
                    parent: cell
                    visible: ma.containsMouse
                    delay: 600
                    text: cell.dir.replace(/^\/home\/[^/]+/, "~")
                    // Как у кнопок: без этого Basic рисует чёрный прямоугольник с крупным шрифтом
                    contentItem: Text {
                        text: whereTip.text
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

            // Переименование на месте: выделено имя без расширения
            Loader {
                active: cell.renaming
                x: pane.gridMode ? 4 : 34
                y: pane.gridMode ? iconBox.size + 16 : 2
                width: pane.gridMode ? parent.width - 8 : parent.width - 34 - pane.nameRight - 10
                height: pane.gridMode ? 26 : parent.height - 4
                sourceComponent: Rectangle {
                    radius: 6
                    color: Theme.surface0
                    border.color: Theme.accent
                    TextInput {
                        id: field
                        anchors.fill: parent
                        anchors.leftMargin: 6; anchors.rightMargin: 6
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: pane.gridMode ? Text.AlignHCenter : Text.AlignLeft
                        clip: true
                        text: cell.name
                        color: Theme.text
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.base
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                        Component.onCompleted: {
                            pane.renameField = field
                            forceActiveFocus()
                            var dot = cell.isDir ? -1 : text.lastIndexOf(".")
                            select(0, dot > 0 ? dot : text.length)
                        }
                        Keys.onReturnPressed: pane.renameRequested(cell.path, text, false)
                        Keys.onEnterPressed: pane.renameRequested(cell.path, text, false)
                        Keys.onEscapePressed: { pane.renamingPath = ""; view.forceActiveFocus() }
                        // Клик мимо поля — как Enter; Esc уже сбросил renamingPath, и сюда не дойдёт
                        onActiveFocusChanged: if (!activeFocus && pane.renamingPath === cell.path) pane.renameRequested(cell.path, text, true)
                    }
                }
            }
        }
    }

    // Пустая корзина — с картинкой, остальное — просто текстом
    readonly property bool inTrash: pane.model.path === Places.trashPath
    Image {
        id: emptyIcon
        visible: pane.model.count === 0 && pane.inTrash && pane.model.searchQuery.length === 0
        anchors.horizontalCenter: view.horizontalCenter
        anchors.bottom: emptyText.top
        anchors.bottomMargin: 14
        source: Theme.glassIcon("trash-can", Theme.inkTint)
        sourceSize: Qt.size(96, 96)
    }
    Text {
        id: emptyText
        anchors.centerIn: view
        visible: pane.model.count === 0
        text: pane.model.error.length > 0 ? pane.model.error
            : pane.model.searchQuery.length === 0 ? (pane.inTrash ? qsTr("Корзина пуста") : qsTr("Папка пуста"))
            : pane.model.searching ? qsTr("Ищу…") : qsTr("Ничего не найдено")
        color: Theme.subtext0
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize
    }
}
