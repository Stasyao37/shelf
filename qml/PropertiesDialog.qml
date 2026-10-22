import QtQuick
import QtQuick.Controls.Basic
import Shelf

Popup {
    id: dlg

    property var info: ({})
    property string icon: ""

    function showFor(path, iconName) {
        info = FileOps.info(path)
        icon = iconName
        open()
    }

    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 20
    width: 440
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.25) }

    background: Rectangle {
        color: Theme.material ? Theme.surface1 : Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.97)
        radius: Theme.widgetRadius + 4
        border.width: 1
        border.color: Theme.material ? "transparent" : Theme.glassBorder
    }

    readonly property var rows: [
        ["Тип", info.type], ["Размер", info.size], ["Содержимое", info.contents],
        ["Где", (info.dir || "").replace(/^\/home\/[^/]+/, "~")], ["Ссылка на", info.link],
        ["Изменён", info.modified], ["Владелец", info.owner], ["Права", info.permissions]
    ].filter(function (r) { return r[1] })

    contentItem: Column {
        spacing: 14

        Row {
            spacing: 14
            FolderIcon {
                visible: dlg.info.type === "Папка"
                size: 48
                iconName: dlg.icon
            }
            FileIcon {
                visible: dlg.info.type !== "Папка"
                size: 48
                mimeType: dlg.info.mimeType || ""
                fileName: dlg.info.name || ""
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: dlg.availableWidth - 62
                text: dlg.info.name || ""
                wrapMode: Text.WrapAnywhere
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 2
                font.weight: Font.DemiBold
            }
        }

        Grid {
            columns: 2
            columnSpacing: 16
            rowSpacing: 8
            Repeater {
                model: dlg.rows.length * 2
                Text {
                    required property int index
                    readonly property var row: dlg.rows[Math.floor(index / 2)]
                    readonly property bool isLabel: index % 2 === 0
                    width: isLabel ? 100 : dlg.availableWidth - 116
                    text: isLabel ? row[0] : row[1]
                    wrapMode: Text.WrapAnywhere
                    color: isLabel ? Theme.subtext0 : Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                }
            }
        }
    }
}
