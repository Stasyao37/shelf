import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Подтверждение необратимого действия. Enter — да, Esc — отмена.
Popup {
    id: dlg

    property string title: ""
    property string message: ""
    property string actionText: "Удалить"
    property var acceptFn: null

    function ask(t, msg, action, accept) {
        title = t
        message = msg
        actionText = action
        acceptFn = accept
        open()
    }
    function accept() {
        var f = acceptFn
        close()
        if (f) f()
    }

    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 22
    width: 420
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: okBtn.forceActiveFocus()
    onClosed: acceptFn = null

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.25) }

    background: Rectangle {
        color: Theme.material ? Theme.surface1 : Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.97)
        radius: Theme.widgetRadius + 4
        border.width: 1
        border.color: Theme.material ? "transparent" : Theme.glassBorder
    }

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 12
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "\u{f071}"
                color: Theme.red
                font.family: Theme.iconFont
                font.pixelSize: Theme.fontSize + 6
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: dlg.availableWidth - 40
                text: dlg.title
                wrapMode: Text.Wrap
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.weight: Font.DemiBold
            }
        }
        Text {
            width: dlg.availableWidth
            text: dlg.message
            visible: text.length > 0
            wrapMode: Text.Wrap
            color: Theme.subtext0
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }
        Item { width: 1; height: 6 }
        Row {
            anchors.right: parent.right
            spacing: 8
            TextButton {
                text: "Отмена"
                onClicked: dlg.close()
            }
            TextButton {
                id: okBtn
                text: dlg.actionText
                danger: true
                onClicked: dlg.accept()
            }
        }
    }
}
