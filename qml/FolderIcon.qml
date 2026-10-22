import QtQuick
import QtQuick.Effects
import Shelf

// Стеклянная папка (icons8) вместо папок из темы иконок. У XDG-папок
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

    // 0 — закрыта, 1 — открыта
    property real openness: open ? 1 : 0
    Behavior on openness { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    // Открытая чуть «приподнимается» — с пружинкой
    scale: open ? 1.06 : 1.0
    Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack; easing.overshoot: 2.5 } }

    Item {
        anchors.fill: parent
        layer.enabled: Theme.light
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: Qt.darker(Theme.accent, 1.6)
        }

        Image {
            anchors.fill: parent
            source: Qt.resolvedUrl("icons/folder.svg")
            sourceSize: Qt.size(root.size, root.size)
            opacity: 1 - root.openness
            visible: opacity > 0
        }
        Image {
            anchors.fill: parent
            source: Qt.resolvedUrl("icons/folder-open.svg")
            sourceSize: Qt.size(root.size, root.size)
            opacity: root.openness
            visible: opacity > 0
        }

        GlassEmblem {
            visible: root.emblem !== "" && root.size >= 32
            width: Math.round(root.size * 0.46)
            height: width
            anchors.horizontalCenter: parent.horizontalCenter
            // Центр передней стенки: у открытой она начинается ниже
            y: parent.height * (0.57 + 0.04 * root.openness) - height / 2
            source: root.emblem ? Qt.resolvedUrl("icons/emblems/" + root.emblem + ".svg") : ""
        }
    }
}
