import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Shelf

Window {
    id: win

    property string startPath: ""
    property string startSelect: ""

    // Окно выбора — компактнее, как обычный диалог
    width: Picker.active ? 940 : 1100
    height: Picker.active ? 600 : 700
    minimumWidth: 640
    minimumHeight: 400
    visible: true
    // Прозрачный фон — Hyprland размоет то, что под окном, как у бара
    color: "transparent"
    title: picking ? (Picker.title || win.pickDefaultTitle)
                   : (dir.searchQuery ? qsTr("Поиск «%1»").arg(dir.searchQuery) : folderName(dir.path)) + " — Shelf"

    // undefined — по ширине окна; F9 закрепляет выбор до конца сеанса
    property var sidebarShown: undefined

    // Вкладки (FolderTab) в порядке полоски; dir и pane — текущей вкладки
    property var tabs: []
    // Текущая вкладка хранится как объект, номер выводится из неё: при перестановке
    // вкладок порядок и номер меняются одним присваиванием, без промежуточного
    // состояния, в котором подсветка на миг уходила к соседней вкладке (issue #1)
    property FolderTab tab: null
    readonly property int current: Math.max(0, tabs.indexOf(tab))
    readonly property DirModel dir: tab ? tab.dir : stubDir
    readonly property FilePane pane: tab ? tab.pane : stubPane

    // Вид общий для всех вкладок и запоминается между запусками
    property bool gridMode: true
    // Размер значков в сетке — один из iconSteps (Ctrl+колесо, Ctrl+= / Ctrl+- / Ctrl+0)
    readonly property var iconSteps: [40, 56, 80, 112, 160]
    property int iconSize: 56
    function zoom(step) {
        var i = iconSteps.indexOf(iconSize)
        if (i < 0) i = 1
        if (!gridMode) {
            // В списке крупнее — значит, значками
            if (step > 0) gridMode = true
            return
        }
        iconSize = iconSteps[step === 0 ? 1 : Math.max(0, Math.min(iconSteps.length - 1, i + step))]
    }
    property bool showHidden: false
    property string sortKey: "name"
    property bool sortDesc: false
    onGridModeChanged: tabs.forEach(function (t) { t.pane.gridMode = gridMode })
    onIconSizeChanged: tabs.forEach(function (t) { t.pane.iconSize = iconSize })
    onShowHiddenChanged: tabs.forEach(function (t) { t.dir.showHidden = showHidden })
    onSortKeyChanged: tabs.forEach(function (t) { t.dir.sortKey = sortKey })
    onSortDescChanged: tabs.forEach(function (t) { t.dir.sortDesc = sortDesc })

    // Первая настройка показывается один раз
    Settings {
        id: setupState
        category: "setup"
        property bool done: false
    }

    Settings {
        category: "view"
        property alias gridMode: win.gridMode
        property alias iconSize: win.iconSize
        property alias showHidden: win.showHidden
        property alias sortKey: win.sortKey
        property alias sortDesc: win.sortDesc
    }

    // ── Сессия ───────────────────────────────────────────────────────
    // Вкладки пишутся на диск на случай выключения или падения; при обычном
    // закрытии файл удаляется. Осиротевшую сессию предлагаем вернуть внизу слева
    property var restoreOffer: null
    function saveSession() { sessionSave.restart() }
    Timer {
        id: sessionSave
        interval: 400
        onTriggered: Session.save(win.tabs.map(function (t) { return t.dir.path }), win.current)
    }
    onTabsChanged: if (!picking) saveSession()
    onCurrentChanged: if (!picking) saveSession()
    onClosing: {
        // Закрыли окно выбора крестиком — это «Отмена»
        if (picking) { Picker.cancel(); return }
        sessionSave.stop()
        Session.clear()
    }

    // ── Окно выбора файлов (shelf --pick от портала) ─────────────────
    readonly property bool picking: Picker.active
    readonly property string pickMode: Picker.mode
    readonly property bool pickSave: pickMode === "save"
    readonly property bool pickFolder: pickMode === "folder" || pickMode === "saveFiles"
    readonly property string pickDefaultTitle: pickSave ? qsTr("Сохранить файл") : pickMode === "saveFiles" ? qsTr("Сохранить файлы в папку")
                                              : pickMode === "folder" ? qsTr("Выбрать папку") : Picker.multiple ? qsTr("Открыть файлы") : qsTr("Открыть файл")
    readonly property string pickAcceptText: Picker.acceptLabel ? Picker.acceptLabel.replace("_", "")
                                            : pickSave || pickMode === "saveFiles" ? qsTr("Сохранить") : pickMode === "folder" ? qsTr("Выбрать") : qsTr("Открыть")
    property int pickFilter: Picker.currentFilter >= 0 ? Picker.currentFilter : (Picker.filters.length > 0 ? 0 : -1)
    // id → значение для choices портала
    property var pickChoices: ({})
    function applyPickFilter() {
        var f = Picker.filters[pickFilter]
        dir.dirsOnly = pickFolder
        dir.nameFilters = f ? f.globs : []
        dir.mimeFilters = f ? f.mimes : []
    }
    onPickFilterChanged: if (picking && tab) applyPickFilter()
    // Сохранение: щёлкнули по существующему файлу — его имя в поле (заменить его)
    Connections {
        target: win.picking && win.pickSave ? win.pane : null
        function onSelectedChanged() {
            var sel = win.pane.selectedPaths()
            if (sel.length === 1 && !dir.isDirAt(dir.indexOfPath(sel[0])))
                pickName.text = sel[0].substring(sel[0].lastIndexOf("/") + 1)
        }
    }
    function pickFinish(paths) {
        var ch = []
        for (var id in pickChoices) ch.push([id, pickChoices[id]])
        Picker.accept(paths, pickFilter, ch)
    }
    function pickAccept() {
        var sel = pane.selectedPaths()
        if (pickMode === "open") {
            var files = sel.filter(function (p) { return !dir.isDirAt(dir.indexOfPath(p)) })
            if (files.length === 0) {
                // Выбрана папка — заходим в неё
                if (sel.length === 1) go(sel[0])
                return
            }
            pickFinish(Picker.multiple ? files : [files[0]])
        } else if (pickMode === "folder") {
            pickFinish([sel.length === 1 && dir.isDirAt(dir.indexOfPath(sel[0])) ? sel[0] : dir.path])
        } else if (pickSave) {
            var name = pickName.text.trim()
            if (!name || name.indexOf("/") >= 0) { toast(qsTr("Введите имя файла")); pickName.forceActiveFocus(); return }
            var path = (dir.path === "/" ? "" : dir.path) + "/" + name
            if (Picker.exists(path) && dir.isDirAt(dir.indexOfPath(path))) { go(path); return }
            if (Picker.exists(path))
                confirm.ask(qsTr("Заменить «%1»?").arg(name), qsTr("Файл с таким именем уже есть в этой папке."), qsTr("Заменить"),
                            function () { pickFinish([path]) })
            else pickFinish([path])
        } else {
            // saveFiles — несколько файлов в выбранную папку
            var target = sel.length === 1 && dir.isDirAt(dir.indexOfPath(sel[0])) ? sel[0] : dir.path
            var out = Picker.files.map(function (n) { return (target === "/" ? "" : target) + "/" + n })
            var clash = out.filter(function (p) { return Picker.exists(p) })
            if (clash.length > 0)
                confirm.ask(qsTr("Заменить %n файл(ов)?", "", clash.length), qsTr("В папке уже есть файлы с такими именами."), qsTr("Заменить"),
                            function () { pickFinish(out) })
            else pickFinish(out)
        }
    }
    function restoreTabs() {
        var o = restoreOffer
        restoreOffer = null
        if (!o) return
        // Нетронутую стартовую вкладку заменяем, иначе добавляем к открытым
        var old = tabs.length === 1 && tab.history.length <= 1 ? tabs[0] : null
        var list = old ? [] : tabs.slice()
        var base = list.length
        o.paths.forEach(function (p) {
            var t = tabComponent.createObject(tabStack)
            list.push(t)
            go(p, true, t)
        })
        tab = list[base + o.current]
        tabs = list
        switchTo(base + o.current)
        if (old) old.destroy()
    }

    // Заглушки на время до первой вкладки: привязкам к dir и pane не нужен null
    DirModel { id: stubDir }
    FilePane { id: stubPane; visible: false; model: stubDir }

    Component {
        id: tabComponent
        FolderTab {
            id: t
            anchors.fill: parent
            // Смена вкладки — короткое перетекание; текущая поверх уходящей
            readonly property bool shown: win.tab === t
            // Новая вкладка сначала рисуется едва видимой — её первый кадр самый тяжёлый, —
            // и проявляется уже потом, ровно, а не рывками поверх этого кадра.
            // Первая вкладка при запуске — сразу: окно и так появляется целиком
            property bool fresh: false
            // Первая вкладка становится текущей уже после создания: без этого она проявлялась
            // бы из нуля, а на холодном старте кадры долгие — и список «вплывал» секундами
            property bool fades: false
            opacity: shown ? (fresh ? 0.01 : 1) : 0
            visible: opacity > 0
            z: shown ? 1 : 0
            Behavior on opacity { enabled: t.fades; NumberAnimation { duration: Theme.dur(140); easing.type: Easing.OutCubic } }
            onShownChanged: if (shown && !fades) Qt.callLater(function () { t.fades = true })
            title: t.dir.searchQuery ? qsTr("Поиск «%1»").arg(t.dir.searchQuery) : win.folderName(t.dir.path)
            onMessage: (m) => win.toast(m)
            Component.onCompleted: {
                if (win.tabs.length > 0) {
                    t.fades = true
                    t.fresh = true
                    Theme.afterFrame(t, function () { t.fresh = false })
                }
                t.pane.gridMode = win.gridMode
                t.pane.iconSize = win.iconSteps.indexOf(win.iconSize) >= 0 ? win.iconSize : 56
                t.dir.showHidden = win.showHidden
                t.dir.sortKey = win.sortKey
                t.dir.sortDesc = win.sortDesc
                t.pane.ready = true
            }
            // Сортировку меняют и щелчком по заголовку колонки — запоминаем для всех вкладок
            Connections {
                target: t.dir
                function onSortChanged() { win.sortKey = t.dir.sortKey; win.sortDesc = t.dir.sortDesc }
                function onPathChanged() { win.saveSession() }
            }
            Connections {
                target: t.pane
                function onActivated(row) { win.openRow(row) }
                function onQuickLookRequested(row) { win.quickLookAt(row) }
                function onZoomRequested(step) { win.zoom(step) }
                function onContextMenuRequested(row, x, y) {
                    var pt = t.pane.mapToItem(win.contentItem, x, y)
                    if (row < 0) win.folderMenu(pt.x, pt.y)
                    else win.fileMenu(row, pt.x, pt.y)
                }
                function onRenameRequested(p, name, blur) { win.doRename(p, name, blur) }
                function onFilesDropped(paths, dest, move) { win.dropFiles(paths, dest, move) }
                function onSpringOpen(p) { win.go(p) }
                function onOpenInNewTab(p) { win.newTab(p, false) }
                function onTyped(text) {
                    searchField.forceActiveFocus()
                    searchField.text += text
                }
            }
        }
    }

    Connections {
        target: FileOps
        function onFailed(message) { win.toast(message) }
        function onFinished(newPaths) {
            // Перемещение — самое частое, что делают нечаянно (промахнулись папкой), — сразу с отменой
            if (win.moveToast && newPaths.length > 0 && FileOps.undoKind === "move")
                win.undoToast(qsTr("Перемещено: %1").arg(newPaths.length === 1 ? win.folderName(newPaths[0]) : win.many(newPaths.length)))
            win.moveToast = false
            if (!win.tab) return
            win.tab.pendingSelect = newPaths
            dir.reload()
        }
        // Передумали отменять — тут же «Повторить»
        function onUndone(label) {
            win.toast(qsTr("Отменено: %1").arg(label), qsTr("Повторить"),
                      function () { if (FileOps.redoText === label) win.redo() })
        }
        function onRedone(label) { win.toast(qsTr("Повторено: %1").arg(label)) }
        // Распаковали во временную папку — совпадения имён решаем так же, как при переносе
        function onExtracted(tmp, dest, conflicts) {
            if (conflicts.length === 0) FileOps.placeExtracted(tmp, dest)
            else conflictDlg.resolve(conflicts, dest, function (res) { FileOps.placeExtracted(tmp, dest, res) },
                                     function () { FileOps.discardExtracted(tmp) })
        }
    }

    // ── Навигация ────────────────────────────────────────────────────
    function folderName(p) {
        if (p === Places.places[0].path) return qsTr("Домашняя")
        if (p === Places.trashPath) return qsTr("Корзина")
        if (p === "/") return "/"
        return p.substring(p.lastIndexOf("/") + 1)
    }
    // target — вкладка, по умолчанию текущая (фоновая — при открытии в новой вкладке)
    function go(path, record, target) {
        var t = target || tab
        var shown = t === tab
        var p = t.dir.expandPath(path)
        var from = t.dir.path
        // Поднялись выше — выделяем папку, из которой пришли
        var prefix = p === "/" ? "/" : p + "/"
        if (from.startsWith(prefix)) {
            var next = from.indexOf("/", prefix.length)
            t.pendingSelect = [next > 0 ? from.substring(0, next) : from]
        }
        if (shown) searchField.text = ""
        t.dir.path = p   // перечитывает папку сразу, onReloaded забирает pendingSelect
        if (t.dir.path !== p) { t.pendingSelect = []; return }
        // Переход: вглубь — содержимое въезжает справа, наверх — слева
        if (shown && from && p !== from)
            t.pane.playEnter(p.startsWith(from === "/" ? "/" : from + "/") ? 1 : prefix && from.startsWith(prefix) ? -1 : 0)
        // Та же папка — перечитывания не было
        if (t.pendingSelect.length > 0) { t.pane.selectPaths(t.pendingSelect); t.pendingSelect = [] }
        if (shown) pathBar.editing = false
        t.pane.renamingPath = ""
        if (record !== false && t.history[t.histIndex] !== p) {
            t.history = t.history.slice(0, t.histIndex + 1).concat([p])
            t.histIndex = t.history.length - 1
        }
        // Ничего не выделяем: в окне сохранения выделенный файл подменил бы имя в поле,
        // да и в обычной папке выделение первого мешает. Курсор — с первой стрелкой
        if (t.pane.selectedCount === 0) t.pane.clearCurrent()
        if (shown) t.pane.focusView()
    }
    function back()    { if (tab.histIndex > 0) { tab.histIndex--; go(tab.history[tab.histIndex], false) } }
    function forward() { if (tab.histIndex < tab.history.length - 1) { tab.histIndex++; go(tab.history[tab.histIndex], false) } }
    function up() {
        if (dir.searchQuery) { searchField.text = ""; return }
        if (dir.path !== "/") go(dir.path.substring(0, dir.path.lastIndexOf("/")) || "/")
    }

    // ── Вкладки ──────────────────────────────────────────────────────
    // Новая встаёт справа от текущей, как в браузере; activate=false — в фоне
    function newTab(path, activate) {
        // В окне выбора вкладок нет — «в новой вкладке» просто переходит
        if (picking && tabs.length > 0) { go(path); return tab }
        var t = tabComponent.createObject(tabStack)
        var at = tabs.length === 0 ? 0 : current + 1
        var list = tabs.slice()
        list.splice(at, 0, t)
        tabs = list
        go(path, true, t)
        if (activate) switchTo(at)
        return t
    }
    function switchTo(i) {
        if (i < 0 || i >= tabs.length) return
        searchDebounce.stop()
        pathBar.editing = false
        tab = tabs[i]
        // У поля поиска одно на окно — показываем запрос этой вкладки
        searchField.text = dir.searchQuery
        searchDebounce.stop()
        pane.focusView()
    }
    function closeTab(i) {
        if (tabs.length <= 1) { win.close(); return }
        var closing = tabs[i]
        var list = tabs.slice()
        list.splice(i, 1)
        var next = i < current ? current - 1 : Math.min(current, list.length - 1)
        // Сначала текущая — уже из нового списка, потом сам список
        tab = list[next]
        tabs = list
        switchTo(next)
        closing.destroy()
    }
    function moveTab(from, to) {
        var list = tabs.slice()
        list.splice(to, 0, list.splice(from, 1)[0])
        tabs = list   // текущая вкладка та же, её номер пересчитается сам
    }
    function cycleTab(step) { switchTo((current + step + tabs.length) % tabs.length) }

    // ── Действия ─────────────────────────────────────────────────────
    readonly property bool inTrash: dir.path === Places.trashPath || dir.path.startsWith(Places.trashPath + "/")
    function many(n) { return qsTr("%n объект(ов)", "", n) }

    function openRow(row) {
        if (row < 0) return
        var p = dir.pathAt(row)
        if (dir.isDirAt(row)) go(p)
        else if (picking) { pane.selectRow(row); pickAccept() }
        else openFile(row)
    }
    // Быстрый просмотр (Space): клавиши приходят только в открытую вкладку
    function quickLookAt(row) { quickLook.show(dir, row) }
    // Программой по умолчанию; если её для этого типа нет — спрашиваем, чем открыть
    function openFile(row) {
        if (!Apps.open(dir.pathAt(row), dir.mimeTypeAt(row)))
            openWithDlg.showFor([dir.pathAt(row)], dir.mimeTypeAt(row), dir.mimeCommentAt(row))
    }
    function openSelected() {
        var paths = pane.selectedPaths()
        if (paths.length === 1) { openRow(dir.indexOfPath(paths[0])); return }
        paths.forEach(function (p) { var r = dir.indexOfPath(p); if (!dir.isDirAt(r)) openFile(r) })
    }
    function trashSelected() {
        var paths = pane.selectedPaths()
        if (paths.length === 0) return
        if (inTrash) { deleteForever(paths); return }
        tab.pendingRow = dir.indexOfPath(paths[0])
        var err = FileOps.trash(paths)
        if (err) toast(err)
        else undoToast(qsTr("В корзине: %1").arg(paths.length === 1 ? folderName(paths[0]) : many(paths.length)))
        dir.reload()
    }
    // ── Корзина ──────────────────────────────────────────────────────
    function deleteForever(paths) {
        confirm.ask(paths.length === 1 ? qsTr("Удалить «%1» навсегда?").arg(folderName(paths[0]))
                                       : qsTr("Удалить навсегда %n объект(ов)?", "", paths.length),
                    qsTr("Это действие нельзя отменить."), qsTr("Удалить"), function () {
            tab.pendingRow = dir.indexOfPath(paths[0])
            FileOps.deleteForever(paths)
        })
    }
    function restoreSelected() {
        var paths = pane.selectedPaths()
        if (paths.length === 0 || !dir.trashRoot) return
        tab.pendingRow = dir.indexOfPath(paths[0])
        var res = FileOps.restore(paths)
        if (res.error) toast(res.error)
        else if (res.paths.length === 1) {
            var p = res.paths[0]
            toast(qsTr("Восстановлено в %1").arg((p.substring(0, p.lastIndexOf("/")) || "/").replace(/^\/home\/[^/]+/, "~")))
        } else toast(qsTr("Восстановлено: %1").arg(many(res.paths.length)))
        dir.reload()
    }
    function emptyTrash() {
        if (!Places.trashFull) { toast(qsTr("Корзина уже пуста")); return }
        confirm.ask(qsTr("Очистить корзину?"), qsTr("Всё, что в ней лежит, будет удалено навсегда."), qsTr("Очистить"),
                    function () { FileOps.emptyTrash() })
    }
    function openWith() {
        var paths = pane.selectedPaths()
        if (paths.length === 0) return
        var row = dir.indexOfPath(paths[0])
        openWithDlg.showFor(paths, dir.mimeTypeAt(row), dir.mimeCommentAt(row))
    }
    function paste(target) {
        if (target === Places.trashPath || target.startsWith(Places.trashPath + "/")) {
            toast(qsTr("В корзину файлы убирают клавишей Del")); return
        }
        var paths = FileOps.clipboardPaths()
        if (paths.length === 0) return
        startTransfer(paths, target, FileOps.clipboardIsCut, function (res) { FileOps.paste(target, res) })
    }
    // Совпадения имён в папке назначения — сначала спросить, что с ними делать
    property bool moveToast: false
    function startTransfer(paths, dest, move, run) {
        if (FileOps.busy) { toast(qsTr("Дождитесь окончания текущей операции")); return }
        var go = function (res) { win.moveToast = move; run(res) }
        var cs = FileOps.conflicts(paths, dest)
        if (cs.length === 0) go({})
        else conflictDlg.resolve(cs, dest, go)
    }
    // ── Отмена (Ctrl+Z) и повтор (Ctrl+Shift+Z, Ctrl+Y) ─────────────
    function undo() {
        moveToast = false
        if (!FileOps.undoText) { toast(qsTr("Отменять нечего")); return }
        pane.flyOut(FileOps.undoTargets())
        FileOps.undo()
    }
    function redo() {
        moveToast = false
        if (!FileOps.redoText) { toast(qsTr("Повторять нечего")); return }
        FileOps.redo()
    }
    // Уведомление с кнопкой «Отменить» — только пока последней остаётся эта операция
    function undoToast(text) {
        var label = FileOps.undoText
        toast(text, qsTr("Отменить"), function () { if (FileOps.undoText === label) win.undo() })
    }

    // Перетащили файлы: в корзину — убрать, иначе скопировать или переместить
    function dropFiles(paths, dest, move) {
        if (dest === Places.trashPath) {
            paths = paths.filter(function (p) { return !p.startsWith(Places.trashPath + "/") })
            if (paths.length === 0) return
            var err = FileOps.trash(paths)
            if (err) toast(err)
            else undoToast(qsTr("В корзине: %1").arg(paths.length === 1 ? folderName(paths[0]) : many(paths.length)))
            dir.reload()
            return
        }
        // Из корзины тянуть можно только копией: перемещение потеряло бы запись, откуда файл
        if (paths.some(function (p) { return p.startsWith(Places.trashPath + "/") })) move = false
        // Уже лежат здесь — перемещать некуда
        if (move && paths.every(function (p) { return (p.substring(0, p.lastIndexOf("/")) || "/") === dest })) return
        startTransfer(paths, dest, move, function (res) { FileOps.transfer(paths, dest, move, res) })
    }

    function renameSelected() {
        if (inTrash) return
        var paths = pane.selectedPaths()
        if (paths.length === 1) pane.renamingPath = paths[0]
    }
    function doRename(path, newName, blur) {
        pane.renamingPath = ""
        var err = FileOps.rename(path, newName)
        if (err) toast(err)
        else {
            tab.pendingSelect = [path.substring(0, path.lastIndexOf("/") + 1) + newName.trim()]
            dir.reload()
        }
        if (!blur) pane.focusView()
    }
    function newFolder() {
        if (inTrash) return
        if (dir.searchQuery) searchField.text = ""
        var p = FileOps.makeDir(dir.path, qsTr("Новая папка"))
        if (!p) { toast(qsTr("Не удалось создать папку")); return }
        tab.pendingSelect = [p]
        dir.reload()
        pane.renamingPath = p
    }
    function copySelected(cut) {
        var paths = pane.selectedPaths()
        if (paths.length === 0) return
        // Вырезанное из корзины потеряло бы запись, откуда оно, — только «Восстановить»
        if (cut && inTrash) { toast(qsTr("Из корзины — через «Восстановить» или копированием")); return }
        FileOps.copyToClipboard(paths, cut)
        toast((cut ? qsTr("Вырезано: %1") : qsTr("Скопировано: %1")).arg(paths.length === 1 ? folderName(paths[0]) : many(paths.length)))
    }
    function showProperties() {
        var paths = pane.selectedPaths()
        if (paths.length === 0) { props.showFor(dir.path, "folder"); return }
        props.showFor(paths[0], dir.iconAt(dir.indexOfPath(paths[0])))
    }
    function toggleBookmark(path) {
        if (Places.isBookmarked(path)) { Places.removeBookmark(path); toast(qsTr("Закладка убрана")) }
        else { Places.addBookmark(path); toast(qsTr("Добавлено в закладки")) }
    }

    function trashFileMenu(x, y) {
        var paths = pane.selectedPaths()
        var single = paths.length === 1
        menu.show(x, y, [
            { text: qsTr("Открыть"), icon: "\u{f07c}", shortcut: "Enter", act: openSelected },
            { text: qsTr("Восстановить"), icon: "\u{f0e2}", enabled: dir.trashRoot, act: restoreSelected },
            { separator: true },
            { text: qsTr("Копировать"), icon: "\u{f0c5}", shortcut: "Ctrl+C", act: function () { copySelected(false) } },
            { text: qsTr("Копировать путь"), icon: "\u{f0c1}", act: function () { FileOps.copyText(paths.join("\n")); toast(qsTr("Путь скопирован")) } },
            { separator: true },
            { text: qsTr("Удалить навсегда"), icon: "\u{f1f8}", shortcut: "Del", danger: true, act: function () { deleteForever(paths) } },
            { separator: true },
            { text: qsTr("Свойства"), icon: "\u{f05a}", shortcut: "Alt+Enter", enabled: single, act: showProperties },
        ])
    }
    function fileMenu(row, x, y) {
        if (inTrash) { trashFileMenu(x, y); return }
        var paths = pane.selectedPaths()
        var single = paths.length === 1
        var isDir = single && dir.isDirAt(row)
        var p = paths[0]
        var items = [
            { text: qsTr("Открыть"), icon: "\u{f07c}", shortcut: "Enter", act: openSelected },
            { text: qsTr("Открыть с помощью…"), icon: "\u{f1b2}", act: openWith },
        ]
        if (dir.searchQuery)
            items.push({ text: qsTr("Показать в папке"), icon: "\u{f07b}", enabled: single, act: function () {
                var parent = p.substring(0, p.lastIndexOf("/")) || "/"
                go(parent); tab.pendingSelect = [p]; dir.reload()
            } })
        if (isDir) {
            items.push({ text: qsTr("Открыть в новой вкладке"), icon: "\u{f0db}", act: function () { newTab(p, true) } })
            items.push({ text: qsTr("Открыть в терминале"), icon: "\u{f120}", act: function () { FileOps.openTerminal(p) } })
        }
        items.push({ separator: true })
        items.push({ text: qsTr("Вырезать"), icon: "\u{f0c4}", shortcut: "Ctrl+X", act: function () { copySelected(true) } })
        items.push({ text: qsTr("Копировать"), icon: "\u{f0c5}", shortcut: "Ctrl+C", act: function () { copySelected(false) } })
        if (isDir)
            items.push({ text: qsTr("Вставить в папку"), icon: "\u{f0ea}", enabled: FileOps.clipboardHasFiles,
                         act: function () { paste(p) } })
        items.push({ text: qsTr("Копировать путь"), icon: "\u{f0c1}", act: function () { FileOps.copyText(paths.join("\n")); toast(qsTr("Путь скопирован")) } })
        // Архивы — если есть чем (bsdtar или 7z). Сжимаем только лежащее в одной папке
        var here = p.substring(0, p.lastIndexOf("/")) || "/"
        var name = p.substring(p.lastIndexOf("/") + 1)
        var archives = []
        if (single && !isDir && FileOps.canExtract(dir.mimeTypeAt(row))) {
            archives.push({ text: qsTr("Распаковать сюда"), icon: "\u{f187}", act: function () { FileOps.extract(p, here, false) } })
            archives.push({ text: qsTr("Распаковать в «%1»").arg(FileOps.archiveBaseName(name)), icon: "\u{f187}",
                            act: function () { FileOps.extract(p, here, true) } })
        }
        var oneDir = paths.every(function (q) { return (q.substring(0, q.lastIndexOf("/")) || "/") === here })
        if (oneDir && FileOps.canCompress("zip"))
            archives.push({ text: qsTr("Сжать в zip"), icon: "\u{f1c6}", act: function () { FileOps.compress(paths, "zip") } })
        if (oneDir && FileOps.canCompress("tar.zst"))
            archives.push({ text: qsTr("Сжать в tar.zst"), icon: "\u{f1c6}", act: function () { FileOps.compress(paths, "tar.zst") } })
        if (archives.length > 0) items = items.concat([{ separator: true }], archives)
        items.push({ separator: true })
        items.push({ text: qsTr("Переименовать"), icon: "\u{f040}", shortcut: "F2", enabled: single, act: renameSelected })
        if (isDir)
            items.push({ text: Places.isBookmarked(p) ? qsTr("Убрать из закладок") : qsTr("В закладки"), icon: "\u{f02e}",
                         shortcut: "Ctrl+D", act: function () { toggleBookmark(p) } })
        items.push({ text: qsTr("В корзину"), icon: "\u{f1f8}", shortcut: "Del", danger: true, act: trashSelected })
        items.push({ separator: true })
        items.push({ text: qsTr("Свойства"), icon: "\u{f05a}", shortcut: "Alt+Enter", enabled: single, act: showProperties })
        menu.show(x, y, items)
    }
    function folderMenu(x, y) {
        function sortItem(label, key) {
            return { text: label, checked: dir.sortKey === key, act: function () {
                if (win.sortKey === key) win.sortDesc = !win.sortDesc
                else { win.sortKey = key; win.sortDesc = false }
            } }
        }
        var undoItem = { text: FileOps.undoText ? qsTr("Отменить %1").arg(FileOps.undoText) : qsTr("Отменить"), icon: "\u{f0e2}",
                         shortcut: "Ctrl+Z", enabled: FileOps.undoText !== "" && !FileOps.busy, act: undo }
        // «Повторить» — только когда есть что: обычно отменённого нет
        var undoItems = [undoItem].concat(FileOps.redoText ? [{ text: qsTr("Повторить %1").arg(FileOps.redoText), icon: "\u{f01e}",
                         shortcut: "Ctrl+Shift+Z", enabled: !FileOps.busy, act: redo }] : [])
        var top = inTrash ? undoItems.concat([
            { text: qsTr("Очистить корзину"), icon: "\u{f1f8}", danger: true, enabled: Places.trashFull, act: emptyTrash },
            { separator: true },
        ]) : undoItems.concat([
            { text: qsTr("Новая папка"), icon: "\u{f07b}", shortcut: "Ctrl+Shift+N", act: newFolder },
            { text: qsTr("Вставить"), icon: "\u{f0ea}", shortcut: "Ctrl+V", enabled: FileOps.clipboardHasFiles,
              act: function () { paste(dir.path) } },
            { text: qsTr("Открыть в терминале"), icon: "\u{f120}", shortcut: "F4", act: function () { FileOps.openTerminal(dir.path) } },
            { separator: true },
        ])
        menu.show(x, y, top.concat([
            sortItem(qsTr("По имени"), "name"), sortItem(qsTr("По дате"), "date"), sortItem(qsTr("По размеру"), "size"), sortItem(qsTr("По типу"), "type"),
            { separator: true },
            { text: qsTr("Скрытые файлы"), checked: dir.showHidden, icon: "\u{f070}", shortcut: "Ctrl+H",
              act: function () { win.showHidden = !win.showHidden } },
            { text: Places.isBookmarked(dir.path) ? qsTr("Убрать из закладок") : qsTr("В закладки"), icon: "\u{f02e}", shortcut: "Ctrl+D",
              act: function () { toggleBookmark(dir.path) } },
            { text: qsTr("Свойства папки"), icon: "\u{f05a}", act: function () { props.showFor(dir.path, "folder") } },
            { separator: true },
            { text: qsTr("Настройки Shelf"), icon: "\u{f013}", shortcut: "Ctrl+,", act: function () { settingsDlg.open() } },
        ]))
    }

    // actionText/action — кнопка в уведомлении («Отменить»); с ней оно висит дольше
    property var toastAction: null
    function toast(text, actionText, action) {
        toastText.text = text
        toastBtn.text = actionText || ""
        toastAction = action || null
        toastBox.opacity = 1
        toastTimer.interval = action ? 6000 : 2600
        toastTimer.restart()
    }

    // ── Горячие клавиши ──────────────────────────────────────────────
    Shortcut { sequences: ["Alt+Left", "Back"]; onActivated: win.back() }
    Shortcut { sequences: ["Alt+Right", "Forward"]; onActivated: win.forward() }
    Shortcut { sequences: ["Alt+Up", "Backspace"]; onActivated: win.up() }
    Shortcut { sequence: "Ctrl+L"; onActivated: pathBar.startEdit() }
    Shortcut { sequences: ["Ctrl+F", "/"]; onActivated: { searchField.forceActiveFocus(); searchField.selectAll() } }
    Shortcut { sequence: "Ctrl+H"; onActivated: win.showHidden = !win.showHidden }
    Shortcut { sequences: ["F5", "Ctrl+R"]; onActivated: dir.reload() }
    Shortcut { sequence: "Ctrl+1"; onActivated: win.gridMode = true }
    Shortcut { sequence: "Ctrl+2"; onActivated: win.gridMode = false }
    // Без StandardKey.ZoomIn/ZoomOut: это те же Ctrl++ и Ctrl+-, а одинаковые сочетания
    // в одном Shortcut Qt считает неоднозначными и вместо activated шлёт activatedAmbiguously
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; onActivated: win.zoom(1) }
    Shortcut { sequences: ["Ctrl+-"]; onActivated: win.zoom(-1) }
    Shortcut { sequence: "Ctrl+0"; onActivated: win.zoom(0) }
    Shortcut { sequence: "F2"; onActivated: win.renameSelected() }
    Shortcut { sequence: "Delete"; onActivated: win.trashSelected() }
    Shortcut { sequence: StandardKey.Copy; onActivated: win.copySelected(false) }
    Shortcut { sequence: StandardKey.Cut; onActivated: win.copySelected(true) }
    Shortcut { sequence: StandardKey.Paste; onActivated: win.paste(dir.path) }
    Shortcut { sequence: StandardKey.SelectAll; onActivated: pane.selectAll() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: !win.picking; onActivated: win.undo() }
    Shortcut { sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]; enabled: !win.picking; onActivated: win.redo() }
    Shortcut { sequence: "Ctrl+Shift+N"; onActivated: win.newFolder() }
    Shortcut { sequence: "Ctrl+D"; onActivated: win.toggleBookmark(dir.path) }
    Shortcut { sequences: ["Alt+Return", "Alt+Enter"]; onActivated: win.showProperties() }
    Shortcut { sequence: "F9"; onActivated: win.sidebarShown = !(win.sidebarShown ?? win.width >= 760) }
    Shortcut { sequence: "F4"; onActivated: FileOps.openTerminal(dir.path) }
    Shortcut { sequence: "Ctrl+T"; enabled: !win.picking; onActivated: win.newTab(dir.path, true) }
    Shortcut { sequence: "Ctrl+W"; onActivated: win.closeTab(win.current) }
    Shortcut { sequence: "Ctrl+Q"; onActivated: win.close() }
    // Окно выбора: Ctrl+Enter — «Открыть / Сохранить / Выбрать» откуда угодно
    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter"]; enabled: win.picking; onActivated: win.pickAccept() }
    Shortcut { sequence: "Ctrl+,"; onActivated: settingsDlg.open() }
    Shortcut { sequences: ["Ctrl+Tab", "Ctrl+PgDown"]; onActivated: win.cycleTab(1) }
    Shortcut { sequences: ["Ctrl+Shift+Tab", "Ctrl+Backtab", "Ctrl+PgUp"]; onActivated: win.cycleTab(-1) }
    // Alt+1…8 — вкладка по номеру, Alt+9 — последняя
    Repeater {
        model: 9
        Item {
            required property int index
            Shortcut {
                sequence: "Alt+" + (index + 1)
                onActivated: win.switchTo(index === 8 ? win.tabs.length - 1 : index)
            }
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (FileOps.busy) FileOps.cancel()
            else if (searchField.text) { searchField.text = ""; pane.focusView() }
            // В окне выбора Esc без выделения — «Отмена», как в любом диалоге
            else if (win.picking && pane.selectedCount === 0) Picker.cancel()
            else pane.clearSelection()
        }
    }

    // ── Вид ──────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, Theme.glassAlpha)
        Behavior on color { ColorAnimation { duration: 300 } }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
            // В узком тайле боковая панель съедает треть окна — прячем (F9 — вручную).
            // Прячется и появляется, плавно меняя ширину
            readonly property bool shown: win.sidebarShown ?? win.width >= 760
            Layout.preferredWidth: shown ? 210 : 0
            Behavior on Layout.preferredWidth { NumberAnimation { duration: Theme.dur(200); easing.type: Easing.OutCubic } }
            visible: Layout.preferredWidth > 0.5
            clip: true
            Layout.fillHeight: true
            currentPath: dir.searchQuery ? "" : dir.path
            onNavigate: (p) => win.go(p)
            onOpenInNewTab: (p) => win.newTab(p, false)
            onOpenSettings: settingsDlg.open()
            onFilesDropped: (paths, dest, move) => win.dropFiles(paths, dest, move)
            onTrashMenu: (x, y) => {
                var pt = mapToItem(win.contentItem, x, y)
                menu.show(pt.x, pt.y, [
                    { text: qsTr("Открыть"), icon: "\u{f07c}", act: function () { win.go(Places.trashPath) } },
                    { separator: true },
                    { text: qsTr("Очистить корзину"), icon: "\u{f1f8}", danger: true, enabled: Places.trashFull, act: win.emptyTrash },
                ])
            }
            onBookmarkMenu: (p, x, y) => {
                var pt = mapToItem(win.contentItem, x, y)
                menu.show(pt.x, pt.y, [
                    { text: qsTr("Открыть"), icon: "\u{f07c}", act: function () { win.go(p) } },
                    { text: qsTr("Открыть в новой вкладке"), icon: "\u{f0db}", act: function () { win.newTab(p, true) } },
                    { text: qsTr("Открыть в терминале"), icon: "\u{f120}", act: function () { FileOps.openTerminal(p) } },
                    { separator: true },
                    { text: qsTr("Убрать из закладок"), icon: "\u{f02e}", danger: true, act: function () { Places.removeBookmark(p) } },
                ])
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            spacing: 0

            TabStrip {
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                // С одной вкладкой окно выглядит как раньше; со второй — полоска выезжает
                readonly property bool shown: win.tabs.length > 1
                Layout.topMargin: shown ? 6 : 0
                Layout.preferredHeight: shown ? 34 : 0
                Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.dur(180); easing.type: Easing.OutCubic } }
                Behavior on Layout.topMargin { NumberAnimation { duration: Theme.dur(180); easing.type: Easing.OutCubic } }
                opacity: shown ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.dur(180) } }
                visible: Layout.preferredHeight > 0.5
                clip: true
                tabs: win.tabs
                currentTab: win.tab
                onActivate: (i) => win.switchTo(i)
                onCloseTab: (i) => win.closeTab(i)
                onMoveTab: (from, to) => win.moveTab(from, to)
                onNewTab: win.newTab(win.dir.path, true)
                onFilesDropped: (paths, dest, move) => win.dropFiles(paths, dest, move)
            }

            // Панель: назад/вперёд/вверх | путь | поиск | вид
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.margins: 8
                spacing: 4

                GlassButton { text: "\u{f060}"; tooltip: qsTr("Назад (Alt+←)"); enabled: (win.tab?.histIndex ?? 0) > 0; onClicked: win.back() }
                GlassButton { text: "\u{f061}"; tooltip: qsTr("Вперёд (Alt+→)"); enabled: win.tab ? win.tab.histIndex < win.tab.history.length - 1 : false; onClicked: win.forward() }
                GlassButton { text: "\u{f062}"; tooltip: qsTr("Вверх (Alt+↑)"); enabled: dir.path !== "/"; onClicked: win.up() }

                PathBar {
                    id: pathBar
                    Layout.fillWidth: true
                    Layout.minimumWidth: 100
                    Layout.preferredHeight: 32
                    Layout.leftMargin: 4
                    path: dir.path
                    onNavigate: (p) => win.go(p)
                }

                // Поиск по текущей папке и вглубь
                Rectangle {
                    // В узком (тайловом) окне поиск уступает место пути. Ширину считаем от окна:
                    // от ширины самой панели нельзя — она зависит от этого же поля, и Layout зацикливается
                    Layout.preferredWidth: Math.min(searchField.activeFocus || searchField.text ? 260 : 190,
                                                    Math.max(120, (win.width - (sidebar.visible ? sidebar.width : 0) - 16) * 0.3))
                    Layout.preferredHeight: 32
                    radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                    color: Theme.material ? Theme.surface0 : Theme.ink(0.04)
                    border.width: 1
                    border.color: searchField.activeFocus ? Theme.accent : Theme.restBorder
                    Behavior on Layout.preferredWidth { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

                    Text {
                        id: searchIcon
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: dir.searching ? "\u{f110}" : "\u{f002}"
                        color: Theme.subtext0
                        font.family: Theme.iconFont
                        font.pixelSize: Theme.fontSize - 2
                        RotationAnimation on rotation {
                            running: dir.searching; loops: Animation.Infinite
                            from: 0; to: 360; duration: 900
                            onRunningChanged: if (!running) searchIcon.rotation = 0
                        }
                    }
                    TextInput {
                        id: searchField
                        anchors { left: searchIcon.right; leftMargin: 8; right: clearBtn.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
                        clip: true
                        color: Theme.text
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.base
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                        onTextChanged: searchDebounce.restart()
                        Keys.onEscapePressed: { text = ""; pane.focusView() }
                        Keys.onDownPressed: { pane.selectRow(0); pane.focusView() }
                        Keys.onReturnPressed: { pane.selectRow(0); pane.focusView() }

                        Text {
                            visible: !searchField.text
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Поиск в «%1»").arg(win.folderName(dir.path))
                            elide: Text.ElideRight
                            width: parent.width
                            color: Theme.overlay1
                            font: searchField.font
                        }
                    }
                    Text {
                        id: clearBtn
                        anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                        visible: searchField.text
                        text: "\u{f00d}"
                        color: Theme.subtext0
                        font.family: Theme.iconFont
                        font.pixelSize: Theme.fontSize - 2
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: Qt.PointingHandCursor
                            onClicked: { searchField.text = ""; pane.focusView() }
                        }
                    }
                    Timer {
                        id: searchDebounce
                        interval: 250
                        onTriggered: dir.searchQuery = searchField.text
                    }
                }

                GlassButton {
                    Layout.leftMargin: 4
                    text: pane.gridMode ? "\u{f03a}" : "\u{f00a}"
                    tooltip: pane.gridMode ? qsTr("Списком (Ctrl+2)") : qsTr("Значками (Ctrl+1)")
                    onClicked: win.gridMode = !win.gridMode
                }
                GlassButton {
                    text: dir.showHidden ? "\u{f06e}" : "\u{f070}"
                    active: dir.showHidden
                    tooltip: qsTr("Скрытые файлы (Ctrl+H)")
                    onClicked: win.showHidden = !win.showHidden
                }
            }

            // В корзине: что можно сделать с её содержимым
            Rectangle {
                // Раскрывается при входе в корзину и сворачивается при выходе
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.bottomMargin: win.inTrash ? 6 : 0
                Layout.preferredHeight: win.inTrash ? 40 : 0
                Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.dur(180); easing.type: Easing.OutCubic } }
                Behavior on Layout.bottomMargin { NumberAnimation { duration: Theme.dur(180); easing.type: Easing.OutCubic } }
                opacity: win.inTrash ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.dur(160) } }
                visible: Layout.preferredHeight > 0.5
                clip: true
                radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                color: Theme.material ? Theme.surface0 : Theme.ink(0.04)
                border.width: 1
                border.color: Theme.restBorder

                Text {
                    id: trashIcon
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u{f1f8}"
                    color: Theme.subtext0
                    font.family: Theme.iconFont
                    font.pixelSize: Theme.fontSize - 1
                }
                Text {
                    anchors { left: trashIcon.right; leftMargin: 10; right: trashButtons.left; rightMargin: 10; verticalCenter: parent.verticalCenter }
                    text: !dir.trashRoot ? qsTr("Внутри удалённой папки — восстановить её можно из корня корзины")
                        : Places.trashFull ? qsTr("Del удаляет навсегда, «Восстановить» возвращает на прежнее место")
                        : qsTr("Корзина пуста")
                    elide: Text.ElideRight
                    color: Theme.subtext0
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 3
                }
                Row {
                    id: trashButtons
                    anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
                    spacing: 4
                    TextButton {
                        implicitHeight: 32
                        text: qsTr("Восстановить")
                        visible: dir.trashRoot
                        enabled: pane.selectedCount > 0
                        onClicked: win.restoreSelected()
                    }
                    TextButton {
                        implicitHeight: 32
                        text: qsTr("Очистить корзину")
                        danger: true
                        enabled: Places.trashFull && !FileOps.busy
                        onClicked: win.emptyTrash()
                    }
                }
            }

            // Сюда tabComponent кладёт вкладки; видна только текущая
            Item {
                id: tabStack
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            // Окно выбора: имя файла, параметры приложения, тип файлов, Отмена / действие
            Rectangle {
                visible: win.picking
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.topMargin: 4
                Layout.preferredHeight: 48
                radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                color: Theme.material ? Theme.surface0 : Theme.ink(0.04)
                border.width: 1
                border.color: Theme.restBorder

                RowLayout {
                    anchors { fill: parent; leftMargin: 8; rightMargin: 6 }
                    spacing: 8

                    // Имя файла (сохранение)
                    Rectangle {
                        visible: win.pickSave
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                        color: Theme.material ? Theme.surface1 : Theme.ink(0.05)
                        border.width: 1
                        border.color: pickName.activeFocus ? Theme.accent : Theme.restBorder
                        TextInput {
                            id: pickName
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            verticalAlignment: Text.AlignVCenter
                            clip: true
                            color: Theme.text
                            selectionColor: Theme.accent
                            selectedTextColor: Theme.base
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 1
                            // Имя без расширения выделено — сразу печатаешь своё
                            function selectBase() {
                                var dot = text.lastIndexOf(".")
                                select(0, dot > 0 ? dot : text.length)
                                forceActiveFocus()
                            }
                            Keys.onReturnPressed: win.pickAccept()
                            Keys.onEnterPressed: win.pickAccept()
                            Text {
                                visible: !pickName.text
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Имя файла")
                                color: Theme.overlay1
                                font: pickName.font
                            }
                        }
                    }
                    Text {
                        visible: !win.pickSave
                        Layout.fillWidth: true
                        text: win.pickMode === "saveFiles" ? qsTr("Файлы сохранятся в «%1»: %2").arg(win.folderName(dir.path)).arg(Picker.files.join(", "))
                            : win.pickMode === "folder" ? qsTr("Папка: «%1»").arg(win.folderName(pane.selectedCount === 1 && dir.isDirAt(dir.indexOfPath(pane.selectedPaths()[0])) ? pane.selectedPaths()[0] : dir.path))
                            : pane.selectedCount > 0 ? qsTr("Выбрано: %1").arg(pane.selectedCount)
                            : Picker.multiple ? qsTr("Выберите файлы") : qsTr("Выберите файл")
                        elide: Text.ElideRight
                        color: Theme.subtext0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 2
                    }

                    // Параметры от приложения: флажки и списки
                    Repeater {
                        model: win.picking ? Picker.choices : []
                        Item {
                            id: choiceItem
                            required property var modelData
                            readonly property bool flag: modelData.options.length === 0
                            readonly property string value: win.pickChoices[modelData.id] ?? modelData.value
                            Layout.preferredHeight: 34
                            implicitWidth: flag ? flagRow.implicitWidth : choiceBtn.implicitWidth
                            Row {
                                id: flagRow
                                visible: choiceItem.flag
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8
                                ToggleSwitch {
                                    anchors.verticalCenter: parent.verticalCenter
                                    checked: choiceItem.value === "true"
                                    onToggled: (on) => { var c = Object.assign({}, win.pickChoices); c[choiceItem.modelData.id] = on ? "true" : "false"; win.pickChoices = c }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: choiceItem.modelData.label
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSize - 2
                                }
                            }
                            TextButton {
                                id: choiceBtn
                                visible: !choiceItem.flag
                                anchors.verticalCenter: parent.verticalCenter
                                implicitHeight: 34
                                text: {
                                    var o = choiceItem.modelData.options.find(function (x) { return x[0] === choiceItem.value })
                                    return choiceItem.modelData.label + ": " + (o ? o[1] : choiceItem.value)
                                }
                                onClicked: {
                                    var pt = mapToItem(win.contentItem, 0, 0)
                                    menu.show(pt.x, pt.y, choiceItem.modelData.options.map(function (o) {
                                        return { text: o[1], checked: o[0] === choiceItem.value, act: function () {
                                            var c = Object.assign({}, win.pickChoices); c[choiceItem.modelData.id] = o[0]; win.pickChoices = c } }
                                    }))
                                }
                            }
                        }
                    }

                    // Тип файлов
                    TextButton {
                        visible: win.picking && Picker.filters.length > 0 && !win.pickFolder
                        Layout.preferredHeight: 34
                        implicitHeight: 34
                        Layout.maximumWidth: 240
                        text: (Picker.filters[win.pickFilter] || {}).name || qsTr("Все файлы")
                        onClicked: {
                            var pt = mapToItem(win.contentItem, 0, 0)
                            menu.show(pt.x, pt.y, Picker.filters.map(function (f, i) {
                                return { text: f.name, checked: i === win.pickFilter, act: function () { win.pickFilter = i } }
                            }))
                        }
                    }
                    TextButton {
                        Layout.preferredHeight: 34
                        implicitHeight: 34
                        text: qsTr("Отмена")
                        onClicked: Picker.cancel()
                    }
                    TextButton {
                        Layout.preferredHeight: 34
                        implicitHeight: 34
                        text: win.pickAcceptText
                        primary: true
                        enabled: win.pickSave ? pickName.text.trim().length > 0
                               : win.pickMode === "open" ? pane.selectedCount > 0 : true
                        onClicked: win.pickAccept()
                    }
                }
            }

            // Строка состояния / прогресс операции
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 30

                // Предложение вернуть вкладки после аварийного закрытия
                Row {
                    id: restoreNotice
                    visible: win.restoreOffer !== null && !FileOps.busy
                    // Проявляется, чуть выезжая слева
                    onVisibleChanged: if (visible) noticeIn.restart()
                    transform: Translate { id: noticeShift }
                    ParallelAnimation {
                        id: noticeIn
                        NumberAnimation { target: restoreNotice; property: "opacity"; from: 0; to: 1; duration: Theme.dur(260); easing.type: Easing.OutCubic }
                        NumberAnimation { target: noticeShift; property: "x"; from: -14; to: 0; duration: Theme.dur(300); easing.type: Easing.OutCubic }
                    }
                    anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                    spacing: 8
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\u{f1da}"
                        color: Theme.accent
                        font.family: Theme.iconFont
                        font.pixelSize: Theme.fontSize - 3
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: win.restoreOffer ? qsTr("Shelf закрылся неожиданно — вернуть %n вкладку(и)?", "", win.restoreOffer.paths.length) : ""
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 3
                    }
                    TextButton {
                        anchors.verticalCenter: parent.verticalCenter
                        implicitHeight: 24
                        text: qsTr("Восстановить")
                        primary: true
                        onClicked: win.restoreTabs()
                    }
                    GlassButton {
                        anchors.verticalCenter: parent.verticalCenter
                        implicitHeight: 24; implicitWidth: 26
                        text: "\u{f00d}"
                        iconSize: Theme.fontSize - 3
                        tooltip: qsTr("Не восстанавливать")
                        onClicked: win.restoreOffer = null
                    }
                }
                Text {
                    // В окне выбора то же пишет нижняя панель. Статус и прогресс операции
                    // сменяют друг друга перетеканием, а не рывком
                    opacity: !FileOps.busy && !restoreNotice.visible && !win.picking ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: Theme.dur(160); easing.type: Easing.OutCubic } }
                    anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                    readonly property var sel: pane.selectedCount > 0 ? pane.selectedPaths() : []
                    text: sel.length > 0
                        ? qsTr("Выбрано: %1").arg(sel.length) + (dir.sizeOf(sel) ? "  ·  " + dir.sizeOf(sel) : "")
                        : (dir.searchQuery ? qsTr("Найдено: %1") : qsTr("Объектов: %1")).arg(dir.count)
                    color: Theme.subtext0
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 3
                }
                Text {
                    opacity: FileOps.busy ? 0 : 1
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: Theme.dur(160); easing.type: Easing.OutCubic } }
                    anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                    text: dir.freeSpace
                    color: Theme.subtext0
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 3
                }

                RowLayout {
                    opacity: FileOps.busy ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: Theme.dur(160); easing.type: Easing.OutCubic } }
                    anchors { fill: parent; leftMargin: 16; rightMargin: 10 }
                    spacing: 10
                    Text {
                        Layout.preferredWidth: parent.width * 0.5
                        text: FileOps.jobText
                        elide: Text.ElideMiddle
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 3
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 6
                        radius: 3
                        color: Theme.ink(0.10)
                        Rectangle {
                            width: parent.width * FileOps.progress
                            height: parent.height
                            radius: 3
                            color: Theme.accent
                            Behavior on width { NumberAnimation { duration: 100 } }
                        }
                    }
                    GlassButton {
                        implicitHeight: 24; implicitWidth: 26
                        text: "\u{f00d}"
                        tooltip: qsTr("Отменить (Esc)")
                        onClicked: FileOps.cancel()
                    }
                }
            }
        }
    }

    // Тост: ошибки и подтверждения
    Rectangle {
        id: toastBox
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 44 }
        width: Math.min(toastText.implicitWidth + 32 + (toastBtn.visible ? toastBtn.width + 12 : 0), parent.width - 40)
        height: 36
        radius: Theme.btnRadius(height, false, Theme.widgetRadius)
        color: Theme.material ? Theme.surface2 : Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.96)
        border.width: 1
        border.color: Theme.material ? "transparent" : Theme.glassBorder
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.dur(200); easing.type: Easing.OutCubic } }
        // Поднимается снизу, пока проявляется, и опускается, пока гаснет
        transform: Translate { y: (1 - toastBox.opacity) * 12 }
        Text {
            id: toastText
            anchors { left: parent.left; leftMargin: 16; right: toastBtn.visible ? toastBtn.left : parent.right
                      rightMargin: toastBtn.visible ? 10 : 16; verticalCenter: parent.verticalCenter }
            horizontalAlignment: toastBtn.visible ? Text.AlignLeft : Text.AlignHCenter
            elide: Text.ElideMiddle
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }
        TextButton {
            id: toastBtn
            visible: text !== "" && win.toastAction !== null
            anchors { right: parent.right; rightMargin: 5; verticalCenter: parent.verticalCenter }
            implicitHeight: 26
            primary: true
            onClicked: {
                var f = win.toastAction
                win.toastAction = null
                toastBox.opacity = 0
                if (f) f()
            }
        }
        Timer { id: toastTimer; interval: 2600; onTriggered: { toastBox.opacity = 0; win.toastAction = null } }
    }

    ContextMenu { id: menu; parent: win.contentItem }
    PropertiesDialog { id: props; parent: win.contentItem }
    QuickLook {
        id: quickLook
        parent: win.contentItem
        onOpenRequested: (row) => win.openRow(row)
        // Листают — выделение идёт следом: закрыли, и курсор на последнем просмотренном
        onRowChanged: if (quickLook.visible && quickLook.model === win.dir) win.pane.selectRow(quickLook.row)
    }
    ConfirmDialog {
        id: confirm
        parent: win.contentItem
        onClosed: pane.focusView()
    }
    ConflictDialog {
        id: conflictDlg
        parent: win.contentItem
        onClosed: pane.focusView()
    }
    SettingsDialog {
        id: settingsDlg
        parent: win.contentItem
        onClosed: pane.focusView()
        onFirstRunRequested: firstRun.open()
        onMessage: (text) => win.toast(text)
    }
    FirstRun {
        id: firstRun
        parent: win.contentItem
        onFinished: (message) => {
            setupState.done = true
            if (message) win.toast(message)
            pane.focusView()
        }
    }
    OpenWithDialog {
        id: openWithDlg
        parent: win.contentItem
        onFailed: (m) => win.toast(m)
        onClosed: pane.focusView()
    }

    Component.onCompleted: {
        Theme.afterFrame(win.contentItem, function () { Theme.imagesInBackground = true })
        newTab(startPath || Places.places[0].path, true)
        if (startSelect) { tab.pendingSelect = [startSelect]; dir.reload() }
        if (picking) {
            applyPickFilter()
            pane.singleSelect = !(pickMode === "open" && Picker.multiple)
            var cs = {}
            Picker.choices.forEach(function (c) { cs[c.id] = c.value })
            pickChoices = cs
            if (pickSave) { pickName.text = Picker.currentName; pickName.selectBase() }
            return
        }
        // Одна вкладка там же, где открылись сейчас, — предлагать нечего
        var o = Session.takeOrphan()
        if (o.paths && !(o.paths.length === 1 && o.paths[0] === dir.path)) restoreOffer = o
        if (!setupState.done) firstRun.open()
    }
}
