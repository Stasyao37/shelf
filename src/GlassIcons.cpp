#include "GlassIcons.h"

#include <QImageReader>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

QImage readSvg(const QString &name, QSize size)
{
    QImageReader reader(QStringLiteral(":/qt/qml/Shelf/icons/%1.svg").arg(name));
    reader.setScaledSize(size);
    return reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

// Размытие по Гауссу в обе стороны — для тени эмблемы
std::vector<float> gaussian(const std::vector<float> &src, int w, int h, double sigma)
{
    const int r = int(std::ceil(sigma * 3));
    std::vector<float> kernel(2 * r + 1);
    float sum = 0;
    for (int i = -r; i <= r; ++i)
        sum += kernel[i + r] = float(std::exp(-i * i / (2 * sigma * sigma)));
    for (float &k : kernel)
        k /= sum;

    std::vector<float> tmp(src.size()), out(src.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float v = 0;
            for (int i = -r; i <= r; ++i)
                v += kernel[i + r] * src[y * w + std::clamp(x + i, 0, w - 1)];
            tmp[y * w + x] = v;
        }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float v = 0;
            for (int i = -r; i <= r; ++i)
                v += kernel[i + r] * tmp[std::clamp(y + i, 0, h - 1) * w + x];
            out[y * w + x] = v;
        }
    return out;
}

} // namespace

void GlassIcons::tint(QImage &img, const QColor &color, double colorization, double brightness)
{
    const float k = float(std::clamp(color.alphaF() * colorization, 0.0, 1.0));
    const float cr = float(color.redF()), cg = float(color.greenF()), cb = float(color.blueF());
    const float br = float(brightness);
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb p = line[x];
            const float a = qAlpha(p) / 255.0f;
            if (a == 0)
                continue;
            float r = qRed(p) / 255.0f + br * a;
            float g = qGreen(p) / 255.0f + br * a;
            float b = qBlue(p) / 255.0f + br * a;
            const float gray = r * 0.299f + g * 0.587f + b * 0.114f;
            r = gray * cr * k + r * (1 - k);
            g = gray * cg * k + g * (1 - k);
            b = gray * cb * k + b * (1 - k);
            auto byte = [](float v) { return int(std::lround(std::clamp(v, 0.0f, 1.0f) * 255)); };
            line[x] = qRgba(byte(r), byte(g), byte(b), qAlpha(p));
        }
    }
}

QImage GlassIcons::glass(const QImage &silhouette, double scale, double depth)
{
    const int pad = int(std::lround(glassPad * scale));
    const int w = silhouette.width() + 2 * pad, h = silhouette.height() + 2 * pad;

    // Заливка по силуэту: белое стекло, плотнее сверху (0.92) и прозрачнее снизу (0.62).
    // Маска — smoothstep по альфе силуэта, как maskThresholdMin 0.5 + maskSpreadAtMin 1
    std::vector<float> alpha(size_t(w) * h, 0.0f);
    for (int y = 0; y < silhouette.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(silhouette.constScanLine(y));
        const float fill = 0.92f + (0.62f - 0.92f) * (y + 0.5f) / silhouette.height();
        for (int x = 0; x < silhouette.width(); ++x) {
            const float m = qAlpha(line[x]) / 255.0f;
            alpha[size_t(y + pad) * w + x + pad] = fill * m * m * (3 - 2 * m);
        }
    }

    // Тень — сдвинутая вниз альфа заливки, размытая как у MultiEffect (shadowBlur 0.6,
    // blurMax 12): смесь резкой копии и двух уровней размытия с его весами
    const int dy = int(std::lround(depth * scale));
    std::vector<float> shifted(alpha.size(), 0.0f);
    for (int y = dy; y < h; ++y)
        std::copy_n(alpha.begin() + size_t(y - dy) * w, w, shifted.begin() + size_t(y) * w);
    const auto blur1 = gaussian(shifted, w, h, 1.4 * scale);
    const auto blur2 = gaussian(shifted, w, h, 3.1 * scale);
    constexpr float w0 = 0.795f, w1 = 0.805f, w2 = 0.405f, wsum = w0 + w1 + w2;

    // Стекло поверх тени (rgba(0, 0, 0, 0.28)) — формула шейдера MultiEffect
    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * w + x;
            const float a = alpha[i];
            const float shadow = (w0 * shifted[i] + w1 * blur1[i] + w2 * blur2[i]) / wsum;
            const float aa = std::min(1.0f, a + (1 - a) * (1 - shadow));
            const int rgb = int(std::lround(a * aa * 255));
            line[x] = qRgba(rgb, rgb, rgb, int(std::lround((0.28f * (1 - aa) + a * aa) * 255)));
        }
    }
    return out;
}

QImage GlassIcons::requestImage(const QString &id, QSize *size, const QSize &requested)
{
    const qsizetype q = id.indexOf(QLatin1Char('?'));
    const QString name = id.left(q);
    const QUrlQuery query(q < 0 ? QString() : id.mid(q + 1));
    const QSize want = requested.isValid() && !requested.isEmpty() ? requested : QSize(48, 48);

    QImage img;
    if (query.hasQueryItem(QStringLiteral("glass"))) {
        const double width = query.queryItemValue(QStringLiteral("glass")).toDouble();
        const double scale = want.width() / (width + 2 * glassPad);
        const int side = std::max(1, int(std::lround(width * scale)));
        img = glass(readSvg(name, QSize(side, side)), scale, query.queryItemValue(QStringLiteral("depth")).toDouble());
    } else {
        img = readSvg(name, want);
    }

    if (query.hasQueryItem(QStringLiteral("tint")))
        tint(img, QColor(QLatin1Char('#') + query.queryItemValue(QStringLiteral("tint"))),
             query.queryItemValue(QStringLiteral("k")).toDouble(), query.queryItemValue(QStringLiteral("b")).toDouble());

    if (size)
        *size = img.size();
    return img;
}
