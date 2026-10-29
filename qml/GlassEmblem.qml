import QtQuick
import Shelf

// Эмблема как вставка из матового стекла. Сама эмблема — плоский белый
// силуэт, он служит маской для мягкой заливки: плотнее сверху,
// прозрачнее снизу, без бликов и обводок. Под вставкой — лёгкая размытая
// тень, будто она чуть приподнята над стенкой. Всё это запекает
// image://glass (src/GlassIcons.h) — одна картинка на эмблему, а не эффекты на слоях.
Item {
    id: root

    // Картинка icons/<name>.svg: "emblems/folder-git", "lang/python"…
    property string name
    // Подкраска вместе с иконкой под эмблемой: Theme.groupTint(…), Theme.inkTint или ""
    property string tint: ""
    // Глубина тени: 1 px на 22 px эмблемы (столько она занимает в папке 48 px)
    property real depth: Math.max(1, width / 22)

    // Картинка шире эмблемы на поля под тень — как автоотступ у MultiEffect
    Image {
        readonly property int pad: 12
        x: -pad
        y: -pad
        width: root.width + 2 * pad
        height: root.height + 2 * pad
        visible: root.name !== ""
        source: !visible ? "" : Theme.glassIcon(root.name, "glass=" + root.width + "&depth=" + root.depth.toFixed(2)
                                                          + (root.tint ? "&" + root.tint : ""))
        // Вдвое крупнее и со сглаживанием: края мягкие, без ступенек
        sourceSize: Qt.size(width * 2, height * 2)
        smooth: true
        mipmap: true
    }
}
