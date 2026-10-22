import QtQuick
import QtQuick.Effects

// Эмблема как вставка из матового стекла. Сама эмблема — плоский белый
// силуэт, здесь он служит маской для мягкой заливки: плотнее сверху,
// прозрачнее снизу, без бликов и обводок. Под вставкой — лёгкая размытая
// тень, будто она чуть приподнята над стенкой.
Item {
    id: root

    property url source
    // Глубина тени: 1 px на 22 px эмблемы (столько она занимает в папке 48 px)
    property real depth: Math.max(1, width / 22)

    // Маска рисуется вдвое крупнее и сжимается со сглаживанием: края мягкие, без ступенек
    Image {
        id: shape
        anchors.fill: parent
        visible: false
        source: root.source
        sourceSize: Qt.size(width * 2, height * 2)
        smooth: true
        mipmap: true
        layer.enabled: true
        layer.smooth: true
    }

    Item {
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.rgba(0, 0, 0, 0.28)
            shadowBlur: 0.6
            blurMax: 12
            shadowVerticalOffset: root.depth
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.92) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.62) }
            }
            layer.enabled: true
            layer.smooth: true
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: shape
                // Плавный переход по альфе силуэта вместо жёсткого порога
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1.0
            }
        }
    }
}
