import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Перенос наткнулся на файлы с теми же именами: по одному вопросу на каждый,
// «для всех» отвечает разом за оставшиеся. Ответы — FileOps.transfer (resolutions).
// Esc — отмена всего переноса
GlassDialog {
    id: dlg

    property var items: []
    property int at: 0
    property string destName: ""
    property bool forAll: false
    property var answers: ({})
    property var doneFn: null
    readonly property var cur: items[at] || ({})
    readonly property int left: items.length - at

    function resolve(list, destPath, done) {
        items = list
        at = 0
        answers = {}
        forAll = false
        destName = destPath.substring(destPath.lastIndexOf("/") + 1) || "/"
        doneFn = done
        open()
    }
    function answer(how) {
        var a = Object.assign({}, answers)
        var last = forAll ? items.length : at + 1
        // Папку, внутри которой лежит переносимое, заменить нельзя — для неё «оставить оба»
        for (var i = at; i < last; ++i)
            a[items[i].name] = how === "replace" && !items[i].canReplace ? "keep" : how
        answers = a
        if (last < items.length) { at = last; return }
        var f = doneFn
        close()
        if (f) f(a)
    }

    padding: 22
    width: Math.min(560, parent ? parent.width - 40 : 560)
    closePolicy: Popup.CloseOnEscape
    onOpened: (cur.canReplace ? replaceBtn : keepBtn).forceActiveFocus()
    onAtChanged: (cur.canReplace ? replaceBtn : keepBtn).forceActiveFocus()
    onClosed: doneFn = null

    // Строка сравнения: «Новый  2,4 МБ · 28.10.2026 18:40  новее»
    component Version: Row {
        id: ver
        property string label
        property string info
        property bool newer
        spacing: 10
        Text {
            width: 120
            text: ver.label
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }
        Text {
            text: ver.info
            color: Theme.subtext0
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }
        Text {
            visible: ver.newer
            text: qsTr("новее")
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }
    }

    contentItem: Column {
        spacing: 12

        Row {
            spacing: 12
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "\u{f0c5}"
                color: Theme.accent
                font.family: Theme.iconFont
                font.pixelSize: Theme.fontSize + 6
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: dlg.availableWidth - 40
                text: (dlg.cur.merge ? qsTr("Папка «%1» уже есть в «%2»") : qsTr("«%1» уже есть в «%2»")).arg(dlg.cur.name).arg(dlg.destName)
                wrapMode: Text.Wrap
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.weight: Font.DemiBold
            }
        }

        Column {
            x: 36
            spacing: 4
            Version { label: qsTr("Новый"); info: dlg.cur.srcInfo || ""; newer: dlg.cur.newer === "src" }
            Version { label: qsTr("Тот, что здесь"); info: dlg.cur.dstInfo || ""; newer: dlg.cur.newer === "dst" }
        }

        Text {
            x: 36
            width: dlg.availableWidth - 36
            text: !dlg.cur.canReplace ? qsTr("Внутри этой папки лежит то, что вы переносите, — заменить её нельзя.")
                : dlg.cur.merge ? qsTr("Объединить: содержимое сложится в одну папку, файлы с теми же именами заменятся. Заменённое уйдёт в корзину — Ctrl+Z вернёт всё как было.")
                : qsTr("Заменённый уйдёт в корзину — Ctrl+Z вернёт его.")
            wrapMode: Text.Wrap
            color: Theme.subtext0
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }

        Row {
            x: 36
            visible: dlg.left > 1
            spacing: 10
            ToggleSwitch {
                anchors.verticalCenter: parent.verticalCenter
                checked: dlg.forAll
                onToggled: (on) => dlg.forAll = on
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Для всех совпадений (%1)").arg(dlg.left)
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dlg.forAll = !dlg.forAll }
            }
        }

        Item { width: 1; height: 4 }
        Row {
            anchors.right: parent.right
            spacing: 8
            TextButton {
                text: qsTr("Отмена")
                onClicked: dlg.close()
            }
            TextButton {
                text: qsTr("Пропустить")
                onClicked: dlg.answer("skip")
            }
            TextButton {
                id: keepBtn
                text: qsTr("Оставить оба")
                onClicked: dlg.answer("keep")
            }
            TextButton {
                id: replaceBtn
                visible: dlg.cur.canReplace === true
                text: dlg.cur.merge ? qsTr("Объединить") : qsTr("Заменить")
                primary: true
                onClicked: dlg.answer("replace")
            }
        }
    }
}
