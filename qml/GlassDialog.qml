import QtQuick
import QtQuick.Controls.Basic
import Shelf

// Общая основа диалогов: стеклянная подложка, затемнение под окном и анимация —
// окно проявляется и чуть подрастает до своего размера, затемнение идёт вместе с ним
Popup {
    id: dlg

    // Насколько затемнять окно под диалогом
    property real shade: 0.25

    anchors.centerIn: parent
    modal: true
    focus: true

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, dlg.shade)
        opacity: dlg.opacity
    }

    background: Rectangle {
        color: Theme.material ? Theme.surface1 : Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.97)
        radius: Theme.widgetRadius + 4
        border.width: 1
        border.color: Theme.material ? "transparent" : Theme.glassBorder
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dur(180); easing.type: Easing.OutCubic }
        NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: Theme.dur(220); easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.dur(130); easing.type: Easing.InCubic }
        NumberAnimation { property: "scale"; to: 0.97; duration: Theme.dur(130); easing.type: Easing.InCubic }
    }
}
