import QtQuick
import QtQuick.Effects
import Shelf

// Значок в углу иконки (ссылка, замок): тёмная стеклянная таблетка
// с белым силуэтом из icons/badges. В светлой теме наоборот — светлая
// таблетка и тёмный силуэт, чтобы значок не терялся на белом стекле иконки.
Rectangle {
    id: root

    property string name
    property int size: 18

    width: size
    height: size
    radius: size / 2
    color: Theme.light ? Qt.rgba(1, 1, 1, 0.9) : Qt.rgba(0.08, 0.08, 0.12, 0.72)
    border.width: 1
    border.color: Theme.light ? Qt.rgba(0, 0, 0, 0.12) : Qt.rgba(1, 1, 1, 0.22)

    Image {
        anchors.centerIn: parent
        width: Math.round(root.size * 0.62)
        height: width
        source: Qt.resolvedUrl("icons/badges/" + root.name + ".svg")
        // Вдвое крупнее и со сглаживанием — на 11 px иначе края рвутся
        sourceSize: Qt.size(width * 2, height * 2)
        smooth: true
        mipmap: true
        opacity: 0.92
        layer.enabled: Theme.light
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: Qt.darker(Theme.accent, 1.6)
        }
    }
}
