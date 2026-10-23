import QtQuick
import QtQuick.Controls.Basic
import Shelf

GlassDialog {
    id: dlg

    property var info: ({})
    property string icon: ""

    function showFor(path, iconName) {
        info = Object.assign(FileOps.info(path), FileOps.trashInfo(path))
        icon = iconName
        open()
    }

    padding: 20
    width: 440
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var rows: [
        ["Тип", info.type], ["Размер", info.size], ["Содержимое", info.contents],
        [info.origDir ? "Откуда" : "Где", (info.origDir || info.dir || "").replace(/^\/home\/[^/]+/, "~")],
        ["Удалён", info.deleted], ["Ссылка на", info.link],
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
