#pragma once

#include <QImage>
#include <QString>
#include <QVariantMap>

// Палитра из обоев без matugen: какие обои стоят сейчас и цвета по ним.
// Цвета — в именах палитры шелла (base, surface0, lavender…), как у встроенных.
namespace WallpaperPalette {

// Путь к текущим обоям или пустая строка. Спрашивает по очереди hyprpaper (hyprctl),
// swww / awww (query), wpaperd (wpaperctl), swaybg по командной строке,
// потом конфиги waypaper и hyprpaper
QString current();

// Палитра по картинке: оттенок — самый заметный насыщенный цвет обоев,
// светлоты — фиксированные, как у встроенных тем, чтобы текст всегда читался.
// Серые обои дают почти нейтральную палитру с холодным оттенком
QVariantMap fromImage(const QImage &image, bool dark);

// Прочитать файл уменьшенным (быстро и для 4K) и посчитать палитру
QVariantMap fromFile(const QString &path, bool dark);

}
