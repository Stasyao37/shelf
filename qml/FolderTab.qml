import QtQuick
import Shelf

// Одна вкладка: своя папка, история, выделение и поиск.
// Что делать с файлами, по-прежнему решает Main — он подключается к pane.
Item {
    id: tab

    property alias dir: tabDir
    property alias pane: tabPane
    property string title: ""
    property var history: []
    property int histIndex: -1
    // Что выделить после ближайшего перечитывания папки
    property var pendingSelect: []
    property int pendingRow: -1

    signal message(string text)

    DirModel {
        id: tabDir
        onReloaded: {
            if (tab.pendingSelect.length > 0) {
                tabPane.selectPaths(tab.pendingSelect)
                tab.pendingSelect = []
            } else if (tab.pendingRow >= 0) {
                tabPane.selectRow(tab.pendingRow)
                tab.pendingRow = -1
            } else {
                tabPane.prune()
            }
        }
        onErrorChanged: if (error) tab.message(error)
    }

    FilePane {
        id: tabPane
        anchors.fill: parent
        model: tabDir
        focus: true
    }
}
