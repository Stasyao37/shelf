pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Shelf

// «Открыть с помощью»: подходящие приложения, поиск по всем, «всегда так открывать»
GlassDialog {
    id: dlg

    property var paths: []
    property string mimeType: ""
    property string mimeComment: ""
    property var recommended: []
    property var everything: []
    property bool showAll: false
    property bool remember: false

    signal failed(string message)

    readonly property var apps: {
        var q = filter.text.trim().toLowerCase()
        var src = q || showAll ? everything : recommended
        if (!q) return src
        return src.filter(function (a) {
            return a.name.toLowerCase().indexOf(q) >= 0 || (a.comment || "").toLowerCase().indexOf(q) >= 0
                || a.id.toLowerCase().indexOf(q) >= 0
        })
    }

    function showFor(list, mime, comment) {
        paths = list
        mimeType = mime
        mimeComment = comment
        recommended = Apps.appsFor(mime)
        everything = Apps.allApps()
        showAll = recommended.length === 0
        remember = false
        filter.text = ""
        open()
        appList.currentIndex = 0
    }
    function launch(index) {
        var app = apps[index]
        if (!app) return
        if (remember && !Apps.setDefault(app.id, mimeType))
            failed("Не удалось назначить приложение по умолчанию")
        if (!Apps.launch(app.id, paths))
            failed("Не удалось запустить " + app.name)
        close()
    }
    function iconSource(icon) {
        if (!icon) return "image://icon/application-x-executable"
        if (icon.startsWith("/")) return "file://" + icon
        return "image://icon/" + icon + ",application-x-executable"
    }
    function fileName(p) { return p.substring(p.lastIndexOf("/") + 1) }

    padding: 20
    width: 460
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: filter.forceActiveFocus()

    contentItem: Column {
        spacing: 12

        Row {
            spacing: 12
            FolderIcon {
                visible: dlg.mimeType === "inode/directory"
                size: 36
                iconName: "folder"
            }
            FileIcon {
                visible: dlg.mimeType !== "inode/directory"
                size: 36
                mimeType: dlg.mimeType
                fileName: dlg.paths.length > 0 ? dlg.fileName(dlg.paths[0]) : ""
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: dlg.availableWidth - 48
                spacing: 2
                Text {
                    width: parent.width
                    text: dlg.paths.length === 1 ? "Открыть «" + dlg.fileName(dlg.paths[0]) + "»"
                                                 : "Открыть " + dlg.paths.length + " файлов"
                    elide: Text.ElideMiddle
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize + 1
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    text: dlg.mimeComment
                    elide: Text.ElideRight
                    color: Theme.subtext0
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 2
                }
            }
        }

        // Поиск приложения; стрелки и Enter работают прямо отсюда
        Rectangle {
            width: dlg.availableWidth
            height: 34
            radius: Theme.btnRadius(height, false, Theme.widgetRadius)
            color: Theme.material ? Theme.surface0 : Theme.ink(0.04)
            border.width: 1
            border.color: filter.activeFocus ? Theme.accent : Theme.restBorder
            Text {
                id: filterIcon
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                text: "\u{f002}"
                color: Theme.subtext0
                font.family: Theme.iconFont
                font.pixelSize: Theme.fontSize - 2
            }
            TextInput {
                id: filter
                anchors { left: filterIcon.right; leftMargin: 8; right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
                clip: true
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: Theme.base
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
                onTextChanged: appList.currentIndex = 0
                Keys.onDownPressed: appList.incrementCurrentIndex()
                Keys.onUpPressed: appList.decrementCurrentIndex()
                Keys.onReturnPressed: dlg.launch(appList.currentIndex)
                Keys.onEnterPressed: dlg.launch(appList.currentIndex)
                Text {
                    visible: !filter.text
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Найти приложение"
                    color: Theme.overlay1
                    font: filter.font
                }
            }
        }

        ListView {
            id: appList
            width: dlg.availableWidth
            height: Math.min(Math.max(contentHeight, 44), 44 * 7)
            clip: true
            model: dlg.apps
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Item {
                id: row
                required property var modelData
                required property int index
                readonly property bool current: ListView.isCurrentItem
                width: appList.width
                height: 44

                Rectangle {
                    anchors.fill: parent
                    anchors.rightMargin: 8
                    radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                    color: row.current ? Theme.activeFill(Theme.accent) : rowMa.containsMouse ? Theme.hoverFill : "transparent"
                    border.width: row.current ? 1 : 0
                    border.color: Theme.edge(Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, Theme.activeBorderAlpha))
                }
                Image {
                    id: appIcon
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28; height: 28
                    sourceSize: Qt.size(56, 56)
                    source: dlg.iconSource(row.modelData.icon)
                    asynchronous: true
                    smooth: true
                }
                Column {
                    anchors { left: appIcon.right; leftMargin: 12; right: tag.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    Text {
                        width: parent.width
                        text: row.modelData.name
                        elide: Text.ElideRight
                        color: row.current ? Theme.activeFg(Theme.accent) : Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 1
                    }
                    Text {
                        width: parent.width
                        visible: text.length > 0
                        text: row.modelData.comment || ""
                        elide: Text.ElideRight
                        color: Theme.subtext0
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize - 4
                    }
                }
                Text {
                    id: tag
                    anchors { right: parent.right; rightMargin: 18; verticalCenter: parent.verticalCenter }
                    text: row.modelData.isDefault ? "по умолчанию" : ""
                    color: Theme.overlay1
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 4
                }
                MouseArea {
                    id: rowMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: appList.currentIndex = row.index
                    onDoubleClicked: dlg.launch(row.index)
                }
            }

            Text {
                anchors.centerIn: parent
                visible: appList.count === 0
                text: "Ничего не нашлось"
                color: Theme.overlay1
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }
        }

        // Переключатель «все приложения» — когда не ищем
        Text {
            visible: !filter.text && dlg.recommended.length > 0
            text: dlg.showAll ? "\u{f077}  Только подходящие" : "\u{f078}  Все приложения"
            color: allMa.containsMouse ? Theme.accent : Theme.subtext0
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
            MouseArea {
                id: allMa
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: { dlg.showAll = !dlg.showAll; appList.currentIndex = 0 }
            }
        }

        // «Всегда открывать» — галочка
        Item {
            width: dlg.availableWidth
            height: 26
            visible: dlg.mimeType.length > 0
            Rectangle {
                id: box
                anchors.verticalCenter: parent.verticalCenter
                width: 18; height: 18
                radius: 5
                color: dlg.remember ? Theme.activeFill(Theme.accent) : "transparent"
                border.width: 1
                border.color: dlg.remember ? Theme.accent : Theme.ink(0.25)
                Text {
                    anchors.centerIn: parent
                    visible: dlg.remember
                    text: "\u{f00c}"
                    color: Theme.activeFg(Theme.accent)
                    font.family: Theme.iconFont
                    font.pixelSize: 11
                }
            }
            Text {
                anchors { left: box.right; leftMargin: 10; right: parent.right; verticalCenter: parent.verticalCenter }
                text: "Всегда открывать так: " + (dlg.mimeComment || dlg.mimeType)
                elide: Text.ElideRight
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: dlg.remember = !dlg.remember
            }
        }

        Row {
            anchors.right: parent.right
            spacing: 8
            TextButton { text: "Отмена"; onClicked: dlg.close() }
            TextButton {
                text: "Открыть"
                primary: true
                enabled: appList.count > 0
                onClicked: dlg.launch(appList.currentIndex)
            }
        }
    }
}
