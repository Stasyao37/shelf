import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import Shelf

// Быстрый просмотр по пробелу, как Quick Look: картинка, начало текста,
// крупная миниатюра (PDF, видео — от tumbler) или большой значок со сведениями.
// Стрелки листают соседние файлы, Space и Esc закрывают, Enter открывает
GlassDialog {
    id: dlg

    property DirModel model: null
    property int row: -1
    property var info: ({})
    // image, text, dir, other — FileOps.previewKind
    property string kind: ""
    property string text: ""
    property bool textReady: false
    property bool truncated: false
    property bool binary: false

    // Открыть файл программой (Enter) — решает окно, как при двойном щелчке
    signal openRequested(int row)

    function show(m, r) {
        model = m
        load(r)
        open()
    }
    function load(r) {
        if (!model || r < 0 || r >= model.count) return
        row = r
        info = model.rowInfo(r)
        kind = FileOps.previewKind(info.path, info.mimeType)
        text = ""
        textReady = false
        truncated = false
        binary = false
        if (kind === "text") FileOps.requestTextPreview(info.path)
    }
    function step(d) {
        if (model) load(Math.max(0, Math.min(model.count - 1, row + d)))
    }

    Connections {
        target: FileOps
        function onTextPreview(path, text, truncated, binary) {
            if (path !== dlg.info.path) return // пока читали, перелистнули дальше
            dlg.text = text
            dlg.truncated = truncated
            dlg.binary = binary
            dlg.textReady = true
        }
    }

    width: Math.min(parent.width - 80, 980)
    height: Math.min(parent.height - 80, 760)
    padding: 16
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // Миниатюра tumbler для того, что Qt не нарисует сам (PDF, видео, книги…)
    readonly property string thumb: !visible || kind !== "other" || Thumbnailer.revision < 0 ? ""
        : Thumbnailer.thumbnail(info.path, info.mimeType, Math.min(body.width, body.height))
    readonly property bool showsIcon: kind === "dir" || (kind === "other" && thumb === "")
        || (kind === "image" && picture.status === Image.Error) || (kind === "text" && binary)

    contentItem: Item {
        focus: true
        Keys.onPressed: (e) => {
            switch (e.key) {
            case Qt.Key_Left:
            case Qt.Key_Up:
                dlg.step(-1); break
            case Qt.Key_Right:
            case Qt.Key_Down:
                dlg.step(1); break
            case Qt.Key_Space:
                dlg.close(); break
            case Qt.Key_Return:
            case Qt.Key_Enter:
                dlg.close()
                dlg.openRequested(dlg.row)
                break
            default:
                return
            }
            e.accepted = true
        }

        // Заголовок: имя и сведения одной строкой
        Column {
            id: head
            width: parent.width
            spacing: 2
            Text {
                width: parent.width
                text: dlg.info.name || ""
                elide: Text.ElideMiddle
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                text: [dlg.info.mimeComment, dlg.info.sizeText, dlg.info.dateText].filter(function (s) { return s }).join("  ·  ")
                elide: Text.ElideRight
                color: Theme.subtext0
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }
        }

        Item {
            id: body
            anchors { left: parent.left; right: parent.right; top: head.bottom; topMargin: 12; bottom: parent.bottom }

            // Картинка — в фоне и не крупнее окна: большие фото не держат интерфейс
            Image {
                id: picture
                anchors.fill: parent
                visible: dlg.kind === "image" && status === Image.Ready
                // «#», «%» и пробелы в имени — экранируем, иначе это не тот URL
                source: dlg.visible && dlg.kind === "image" ? "file://" + encodeURIComponent(dlg.info.path).replace(/%2F/g, "/") : ""
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
                autoTransform: true
                smooth: true
                mipmap: true
            }
            Image {
                anchors.fill: parent
                visible: dlg.thumb !== "" && status === Image.Ready
                source: dlg.thumb
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            ScrollView {
                anchors.fill: parent
                visible: dlg.kind === "text" && dlg.textReady && !dlg.binary
                clip: true
                TextArea {
                    text: dlg.text
                    readOnly: true
                    selectByMouse: true
                    // Клавиши остаются у просмотра: стрелки листают файлы, а не двигают курсор
                    activeFocusOnPress: false
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.WrapAnywhere
                    color: Theme.text
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.base
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSize - 1
                    background: null
                }
            }

            Column {
                anchors.centerIn: parent
                visible: dlg.showsIcon
                spacing: 14
                FolderIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: dlg.kind === "dir"
                    size: 128
                    iconName: dlg.info.icon || "folder"
                }
                FileIcon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: dlg.kind !== "dir"
                    size: 128
                    mimeType: dlg.info.mimeType || ""
                    fileName: dlg.info.name || ""
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: dlg.binary
                    text: qsTr("Двоичный файл — просмотра нет")
                    color: Theme.subtext0
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 1
                }
            }
        }

        Text {
            anchors { right: parent.right; bottom: parent.bottom; bottomMargin: -12 }
            visible: dlg.kind === "text" && dlg.truncated && !dlg.binary
            text: qsTr("Показано начало файла")
            color: Theme.subtext0
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 3
        }
    }
}
