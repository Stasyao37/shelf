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
    // Под какую ширину рисовать (при зуме — целевая, см. renderSize у FileIcon)
    property real renderWidth: width
    // Глубина тени на точку ширины: 1 px на 22 px эмблемы (столько она занимает в папке 48 px)
    property real depthRatio: 1 / 22

    // Картинка шире эмблемы на поля под тень — как автоотступ у MultiEffect
    // Картинка нарисована под renderWidth; k — во сколько раз её растянуть под нынешнюю ширину
    GlassImage {
        readonly property int pad: 12
        readonly property real k: root.renderWidth > 0 ? root.width / root.renderWidth : 1
        x: -pad * k
        y: -pad * k
        width: (root.renderWidth + 2 * pad) * k
        height: (root.renderWidth + 2 * pad) * k
        visible: root.name !== ""
        // Одним выражением из renderWidth: по отдельным привязкам адрес успевал собраться
        // с промежуточной глубиной — и картинка рисовалась лишний раз
        source: !visible ? "" : Theme.glassIcon(root.name, "glass=" + root.renderWidth
                                                          + "&depth=" + Math.max(1, root.renderWidth * root.depthRatio).toFixed(2)
                                                          + (root.tint ? "&" + root.tint : ""))
        // Вдвое крупнее и со сглаживанием: края мягкие, без ступенек
        sourceSize: Qt.size((root.renderWidth + 2 * pad) * 2, (root.renderWidth + 2 * pad) * 2)
        smooth: true
        mipmap: true
    }
}
