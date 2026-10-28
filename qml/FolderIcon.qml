import QtQuick
import QtQuick.Effects
import Shelf

// Стеклянная папка (tools/gen_icons.py) вместо папок из темы иконок. У XDG-папок
// поверх — эмблема в том же стиле. В светлой теме белое стекло
// не видно на светлом фоне — перекрашиваем в тёмное.
// open — приоткрыта (наведение/выделение): у обеих иконок одна задняя
// стенка, поэтому перетекание выглядит как откидывание передней.
Item {
    id: root

    // Имя иконки из DirModel: "folder-download,folder" и т.п.
    property string iconName: "folder"
    property int size: 48
    property bool open: false

    width: size
    height: size

    // Эмблемы для XDG- и особых папок (git, проекты, настройки) — в icons/emblems/<имя иконки>.svg
    readonly property string emblem: {
        const n = iconName.split(",")[0]
        return ["user-home", "user-desktop", "folder-documents", "folder-download",
                "folder-pictures", "folder-music", "folder-videos",
                "folder-templates", "folder-publicshare",
                "folder-projects", "folder-git", "folder-config"].includes(n) ? n : ""
    }
    // Папки сборки и зависимостей (node_modules, build, .cache…) — блёклые
    readonly property bool dim: iconName.startsWith("folder-dim")
    opacity: dim ? 0.45 : 1.0

    // Брошенные в папку файлы: листки проскальзывают между стенками, потом папка захлопывается.
    // Пока они падают, папка открыта, даже если курсор уже ушёл
    property var swallowed: []
    readonly property bool swallowing: swallowed.length > 0
    function swallow(paths) {
        if (!Theme.animations || paths.length === 0) return
        var items = []
        for (var i = 0; i < Math.min(paths.length, 3); ++i)
            items.push({ name: paths[i].substring(paths[i].lastIndexOf("/") + 1), mime: FileOps.mimeTypeOf(paths[i]) })
        swallowed = items
        swallowDone.restart()
    }
    Timer {
        id: swallowDone
        // Последний листок: задержка + падение + угасание (см. делегат ниже)
        interval: (root.swallowed.length - 1) * 90 + 130 + 340 + 200
        onTriggered: root.swallowed = []
    }

    // 0 — закрыта, 1 — открыта
    property real openness: open || swallowing ? 1 : 0
    Behavior on openness { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    // Открытая чуть «приподнимается» — с пружинкой
    scale: open || swallowing ? 1.06 : 1.0
    Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack; easing.overshoot: 2.5 } }

    // Стенки подкрашиваются каждая своим слоем: между ними проходят листки, у них свой оттенок
    component Tinted: Item {
        anchors.fill: parent
        layer.enabled: Theme.iconTinted("folder")
        layer.effect: MultiEffect {
            colorization: Theme.iconColorization("folder")
            colorizationColor: Theme.iconColor("folder")
            brightness: Theme.iconBrightness("folder")
        }
    }

    // Задняя стенка неподвижна; передняя — отдельной картинкой в кадре своего
    // прямоугольника, откидывается тем же отображением, что и эмблема.
    // Двух наложенных картинок закрытой и открытой папки больше нет
    Tinted {
        Image {
            anchors.fill: parent
            source: Qt.resolvedUrl("icons/folder-back.svg")
            sourceSize: Qt.size(root.size * 2, root.size * 2)
            smooth: true
            mipmap: true
        }
    }

    // Листки падают сверху и уходят за переднюю стенку; ниже дна папки их обрезает
    Item {
        visible: root.swallowing
        y: -root.size
        width: root.size
        height: root.size + root.frontClosed[3][1] * root.size / 24
        clip: true

        Repeater {
            model: root.swallowed
            FileIcon {
                id: sheet
                required property var modelData
                required property int index
                readonly property real spread: index - (root.swallowed.length - 1) / 2
                size: Math.max(12, Math.round(root.size * 0.5))
                mimeType: modelData.mime
                fileName: modelData.name
                x: (root.size - size) / 2 + spread * root.size * 0.14
                // Над папкой → между стенками, верхушка торчит над передней (иначе сквозь
                // стекло кажется, что листок лежит перед папкой) → утоплен целиком
                readonly property real fromY: root.size - size * 0.95
                readonly property real peekY: root.size + root.frontOpen[0][1] * root.size / 24 - size * 0.3
                readonly property real toY: peekY + size * 0.4
                y: fromY
                rotation: spread * 9
                opacity: 0

                SequentialAnimation {
                    running: true
                    PauseAnimation { duration: sheet.index * 90 }
                    ParallelAnimation {
                        NumberAnimation { target: sheet; property: "opacity"; to: 1; duration: 130 }
                        NumberAnimation { target: sheet; property: "y"; from: sheet.fromY + 6; to: sheet.fromY; duration: 130; easing.type: Easing.OutCubic }
                    }
                    ParallelAnimation {
                        NumberAnimation { target: sheet; property: "y"; to: sheet.peekY; duration: 340; easing.type: Easing.InOutCubic }
                        NumberAnimation { target: sheet; property: "rotation"; to: 0; duration: 340; easing.type: Easing.OutCubic }
                    }
                    // Сквозь стекло листок ещё виден — гаснет, уходя вглубь
                    ParallelAnimation {
                        NumberAnimation { target: sheet; property: "y"; to: sheet.toY; duration: 200; easing.type: Easing.InCubic }
                        NumberAnimation { target: sheet; property: "opacity"; to: 0; duration: 200; easing.type: Easing.InCubic }
                    }
                }
            }
        }
    }

    Tinted {
        // Кадр картинки — стенка с полями frontPad со всех сторон (см. folder-front.svg)
        Image {
            id: frontItem
            width: (root.frontClosed[1][0] - root.frontClosed[0][0] + 2 * root.frontPad) * root.size / 24
            height: (root.frontClosed[3][1] - root.frontClosed[0][1] + 2 * root.frontPad) * root.size / 24
            source: Qt.resolvedUrl("icons/folder-front.svg")
            sourceSize: Qt.size(width * 2, height * 2)
            smooth: true
            // Без mipmap: под перспективой GPU брал уменьшенную копию, и стенка мылилась
            antialiasing: true
            transform: Matrix4x4 { matrix: root.frontMatrix(root.openness) }
        }

        // Эмблема нарисована на передней стенке и откидывается вместе с ней:
        // её углы переносятся проективным отображением «закрытая стенка → стенка
        // сейчас», поэтому перспектива не ломается и эмблема не вылезает за стенку
        GlassEmblem {
            id: emblemItem
            visible: root.emblem !== "" && root.size >= 32
            width: Math.round(root.size * 0.46)
            height: width
            source: root.emblem ? Qt.resolvedUrl("icons/emblems/" + root.emblem + ".svg") : ""
            antialiasing: true
            transform: Matrix4x4 { matrix: root.emblemMatrix(root.openness) }
        }
    }

    // Передняя стенка в сетке 24 × 24 (как в tools/gen_icons.py): закрытая — прямоугольник,
    // открытая — трапеция, откинутая к зрителю. Углы: левый-верх, правый-верх, правый-низ, левый-низ
    readonly property var frontClosed: [[2.5, 8.4], [21.5, 8.4], [21.5, 20.0], [2.5, 20.0]]
    readonly property var frontOpen: [[1.6, 10.6], [22.4, 10.6], [20.7, 20.0], [3.3, 20.0]]
    // Поля вокруг стенки в кадре folder-front.svg, в единицах сетки 24
    readonly property real frontPad: 0.5
    // Место эмблемы на закрытой стенке, в долях стенки
    readonly property real emblemCenterU: 0.5
    readonly property real emblemCenterV: 0.5

    // Отображение единичного квадрата на четырёхугольник q (Heckbert):
    // x = (a·u + b·v + c) / (g·u + h·v + 1), y = (d·u + e·v + f) / (g·u + h·v + 1)
    function squareToQuad(q) {
        var x0 = q[0][0], y0 = q[0][1], x1 = q[1][0], y1 = q[1][1]
        var x2 = q[2][0], y2 = q[2][1], x3 = q[3][0], y3 = q[3][1]
        var dx3 = x0 - x1 + x2 - x3, dy3 = y0 - y1 + y2 - y3
        if (Math.abs(dx3) < 1e-9 && Math.abs(dy3) < 1e-9)
            return { a: x1 - x0, b: x3 - x0, c: x0, d: y1 - y0, e: y3 - y0, f: y0, g: 0, h: 0 }
        var dx1 = x1 - x2, dx2 = x3 - x2, dy1 = y1 - y2, dy2 = y3 - y2
        var den = dx1 * dy2 - dx2 * dy1
        var g = (dx3 * dy2 - dx2 * dy3) / den
        var h = (dx1 * dy3 - dx3 * dy1) / den
        return { a: x1 - x0 + g * x1, b: x3 - x0 + h * x3, c: x0,
                 d: y1 - y0 + g * y1, e: y3 - y0 + h * y3, f: y0, g: g, h: h }
    }
    function apply(m, u, v) {
        var w = m.g * u + m.h * v + 1
        return [(m.a * u + m.b * v + m.c) / w, (m.d * u + m.e * v + m.f) / w]
    }
    // Передняя стенка сейчас, в пикселях: между закрытой и открытой
    function wall(t) {
        var k = size / 24, q = []
        for (var i = 0; i < 4; ++i)
            q.push([(frontClosed[i][0] + (frontOpen[i][0] - frontClosed[i][0]) * t) * k,
                    (frontClosed[i][1] + (frontOpen[i][1] - frontClosed[i][1]) * t) * k])
        return q
    }
    // Матрица для Item.transform: локальный прямоугольник 0…w × 0…h → четырёхугольник q
    function quadMatrix(w, h, q) {
        var m = squareToQuad(q)
        return Qt.matrix4x4(m.a / w, m.b / h, 0, m.c,
                            m.d / w, m.e / h, 0, m.f,
                            0, 0, 1, 0,
                            m.g / w, m.h / h, 0, 1)
    }
    // Кадр передней стенки: её углы с полями — через то же отображение стенки
    function frontMatrix(t) {
        var panel = squareToQuad(wall(t))
        var pu = frontPad / (frontClosed[1][0] - frontClosed[0][0])
        var pv = frontPad / (frontClosed[3][1] - frontClosed[0][1])
        return quadMatrix(frontItem.width, frontItem.height,
                          [apply(panel, -pu, -pv), apply(panel, 1 + pu, -pv), apply(panel, 1 + pu, 1 + pv), apply(panel, -pu, 1 + pv)])
    }
    function emblemMatrix(t) {
        var k = size / 24
        var panel = squareToQuad(wall(t))
        // Эмблема — квадрат в долях закрытой стенки; её углы едут вместе со стенкой
        var cw = (frontClosed[1][0] - frontClosed[0][0]) * k, ch = (frontClosed[3][1] - frontClosed[0][1]) * k
        var du = emblemItem.width / cw / 2, dv = emblemItem.height / ch / 2
        var u0 = emblemCenterU - du, u1 = emblemCenterU + du, v0 = emblemCenterV - dv, v1 = emblemCenterV + dv
        return quadMatrix(emblemItem.width, emblemItem.height,
                          [apply(panel, u0, v0), apply(panel, u1, v0), apply(panel, u1, v1), apply(panel, u0, v1)])
    }
}
