import QtQuick
import QtQuick.Effects
import Shelf

// Миниатюра вместо иконки: вписана в квадрат иконки, углы скруглены,
// тонкая светлая рамка и мягкая тень — как у стеклянных иконок рядом.
// У видео поверх — кнопка воспроизведения, чтобы кадр не путали с фото.
Item {
    id: root

    property string source
    property int size: 56
    property bool video: false
    // Готова ли картинка: пока грузится или не смогла — снаружи показывают иконку
    readonly property bool ready: img.status === Image.Ready

    width: size
    height: size

    Image {
        id: img
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(root.size * 2, root.size * 2)
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
        visible: false
        layer.enabled: true
    }

    // Скругление по видимой части картинки, а не по всему квадрату
    Item {
        id: mask
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.centerIn: parent
            width: img.paintedWidth
            height: img.paintedHeight
            radius: Math.max(3, Math.min(width, height) * 0.12)
        }
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.ready
        source: img
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.35)
        shadowBlur: 0.5
        shadowVerticalOffset: 1
    }

    Rectangle {
        visible: root.ready
        anchors.centerIn: parent
        width: img.paintedWidth
        height: img.paintedHeight
        radius: Math.max(3, Math.min(width, height) * 0.12)
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, Theme.light ? 0.5 : 0.18)
    }

    Rectangle {
        visible: root.ready && root.video && root.size >= 32
        anchors.centerIn: parent
        width: Math.round(root.size * 0.38)
        height: width
        radius: width / 2
        color: Qt.rgba(0.08, 0.08, 0.12, 0.55)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.3)
        Image {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: 1
            width: Math.round(parent.width * 0.55)
            height: width
            source: Qt.resolvedUrl("icons/badges/play.svg")
            sourceSize: Qt.size(width * 2, height * 2)
            smooth: true
            opacity: 0.95
        }
    }
}
