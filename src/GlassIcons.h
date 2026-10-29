#pragma once

#include <QColor>
#include <QImage>
#include <QQuickImageProvider>

// image://glass/<имя>[?параметры] — картинка из icons/<имя>.svg, уже подкрашенная.
// Раньше подкраску и стеклянные эмблемы делал MultiEffect на слое каждой иконки:
// десятки эффектов и текстур на папку, хотя одинаковые иконки дают одну картинку.
// Теперь картинка считается здесь один раз, а QQuickPixmapCache хранит её по адресу
// (имя + параметры + размер) — все одинаковые иконки делят одну текстуру.
//
// Параметры:
//   tint=rrggbb&k=<0…1>&b=<яркость> — подкраска как у MultiEffect
//       (colorizationColor, colorization, brightness), формулы — его шейдера
//   glass=<ширина>&depth=<тень> — эмблема из матового стекла (бывший GlassEmblem.qml
//       на MultiEffect): силуэт — маска для заливки «плотнее сверху», под ним мягкая
//       тень. Ширина эмблемы и глубина тени — в точках; картинка шире эмблемы
//       на glassPad с каждой стороны, чтобы тень не обрезалась
class GlassIcons : public QQuickImageProvider
{
public:
    // Поля вокруг эмблемы, в точках: как автоотступ MultiEffect при blurMax 12
    static constexpr int glassPad = 12;

    GlassIcons()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override;

    // Подкраска MultiEffect: яркость, затем смешение с серым, умноженным на цвет.
    // Картинка в premultiplied, как текстура слоя — тогда это то же, что на видеокарте
    static void tint(QImage &img, const QColor &color, double colorization, double brightness);
    // Стеклянная эмблема из силуэта silhouette; scale — пикселей на точку
    static QImage glass(const QImage &silhouette, double scale, double depth);
};
