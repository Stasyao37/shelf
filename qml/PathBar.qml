import QtQuick
import Shelf

// Хлебные крошки; Ctrl+L (или клик по пустому месту) — ввод пути текстом
Rectangle {
    id: bar

    property string path: ""
    property bool editing: false
    signal navigate(string path)

    radius: Theme.btnRadius(height, false, Theme.widgetRadius)
    color: editing ? Theme.surface0 : (Theme.material ? Theme.surface0 : Theme.ink(0.04))
    border.width: 1
    border.color: editing ? Theme.accent : Theme.restBorder

    function startEdit() {
        editing = true
        input.text = path
        input.forceActiveFocus()
        input.selectAll()
    }

    readonly property var crumbs: {
        var home = Places.places[0].path
        var res = []
        var rest = path
        var acc = ""
        if (path === home || path.startsWith(home + "/")) {
            res.push({ name: qsTr("Домашняя"), path: home, icon: "\u{f015}" })
            rest = path.slice(home.length)
            acc = home
        } else {
            res.push({ name: "", path: "/", icon: "\u{f0a0}" })
        }
        rest.split("/").forEach(function (s) {
            if (!s) return
            acc += "/" + s
            res.push({ name: s, path: acc, icon: "" })
        })
        return res
    }

    // Сегменты сверяются по пути: при переходе меняются только отличающиеся
    KeyedModel { id: keyed; source: bar.crumbs; keyRole: "path" }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.IBeamCursor
        onClicked: bar.startEdit()
    }

    Flickable {
        id: crumbView
        visible: !bar.editing
        anchors.fill: parent
        anchors.leftMargin: 4; anchors.rightMargin: 4
        contentWidth: row.width
        clip: true
        interactive: false
        // Длинный путь: видно хвост, а не начало
        contentX: Math.max(0, contentWidth - width)

        Row {
            id: row
            height: crumbView.height
            Repeater {
                model: keyed.model
                Row {
                    id: crumb
                    required property string key
                    required property bool entering
                    required property bool leaving
                    readonly property var modelData: { keyed.rev; return keyed.item(key) }
                    readonly property bool last: key === bar.path
                    // Новый сегмент раскрывается и проявляется, ушедший — сворачивается;
                    // общая часть пути стоит на месте. Только ширина и прозрачность
                    property real shown: entering ? 0 : 1
                    height: row.height
                    width: implicitWidth * shown
                    opacity: shown
                    clip: shown < 1
                    Component.onCompleted: if (entering) Theme.afterFrame(crumb, function () { enterAnim.start() })
                    NumberAnimation {
                        id: enterAnim
                        target: crumb; property: "shown"; to: 1; duration: Theme.dur(220); easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        running: crumb.leaving
                        target: crumb; property: "shown"; to: 0; duration: Theme.dur(180); easing.type: Easing.InCubic
                    }

                    Text {
                        // Перед каждым сегментом, кроме первого: у первого («/» или «Домашняя») — значок
                        visible: crumb.modelData.icon === ""
                        anchors.verticalCenter: parent.verticalCenter
                        text: "\u{f054}"
                        color: Theme.overlay1
                        font.family: Theme.iconFont
                        font.pixelSize: Theme.fontSize - 6
                        leftPadding: 2; rightPadding: 2
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        height: parent.height - 8
                        width: label.implicitWidth + 16
                        radius: Theme.btnRadius(height, false, 6)
                        color: cma.containsMouse ? Theme.hoverFill : "transparent"
                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: crumb.modelData.icon && crumb.modelData.name
                                ? crumb.modelData.icon + "  " + crumb.modelData.name
                                : (crumb.modelData.icon || crumb.modelData.name || "")
                            color: crumb.last ? Theme.text : Theme.subtext0
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize - 1
                            font.weight: crumb.last ? Font.DemiBold : Font.Normal
                        }
                        MouseArea {
                            id: cma
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            enabled: !crumb.leaving
                            onClicked: bar.navigate(crumb.key)
                        }
                    }
                }
            }
        }
    }

    TextInput {
        id: input
        visible: bar.editing
        anchors.fill: parent
        anchors.leftMargin: 12; anchors.rightMargin: 12
        verticalAlignment: Text.AlignVCenter
        clip: true
        color: Theme.text
        selectionColor: Theme.accent
        selectedTextColor: Theme.base
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize - 1
        onAccepted: { bar.editing = false; bar.navigate(text) }
        Keys.onEscapePressed: bar.editing = false
        onActiveFocusChanged: if (!activeFocus) bar.editing = false
    }
}
