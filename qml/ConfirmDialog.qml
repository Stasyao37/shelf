import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Подтверждение необратимого действия. Enter — да, Esc — отмена.
GlassDialog {
    id: dlg

    property string title: ""
    property string message: ""
    property string actionText: qsTr("Удалить")
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

    padding: 22
    width: 420
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: okBtn.forceActiveFocus()
    onClosed: acceptFn = null

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
                text: qsTr("Отмена")
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
