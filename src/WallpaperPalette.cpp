#include "WallpaperPalette.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include <array>
#include <cmath>

namespace {
// ── OKLab: светлота и насыщенность в нём ровные на глаз, в отличие от HSL ──
struct Lch {
    double l, c, h; // h — в градусах
};

double toLinear(double c)
{
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double fromLinear(double c)
{
    return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1 / 2.4) - 0.055;
}

Lch toLch(QRgb px)
{
    const double r = toLinear(qRed(px) / 255.0), g = toLinear(qGreen(px) / 255.0), b = toLinear(qBlue(px) / 255.0);
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    const double L = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s;
    const double A = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s;
    const double B = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s;
    double h = std::atan2(B, A) * 180 / M_PI;
    if (h < 0)
        h += 360;
    return {L, std::hypot(A, B), h};
}

// В sRGB или нет; rgb — линейные компоненты
bool toLinearRgb(const Lch &c, std::array<double, 3> &rgb)
{
    const double A = c.c * std::cos(c.h * M_PI / 180), B = c.c * std::sin(c.h * M_PI / 180);
    const double l = std::pow(c.l + 0.3963377774 * A + 0.2158037573 * B, 3);
    const double m = std::pow(c.l - 0.1055613458 * A - 0.0638541728 * B, 3);
    const double s = std::pow(c.l - 0.0894841775 * A - 1.2914855480 * B, 3);
    rgb = {4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
           -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
           -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s};
    for (double v : rgb)
        if (v < -1e-4 || v > 1 + 1e-4)
            return false;
    return true;
}

// Цвет вне sRGB — убавляем насыщенность, светлоту и оттенок не трогаем
QString hex(Lch c)
{
    std::array<double, 3> rgb;
    while (!toLinearRgb(c, rgb) && c.c > 0.001)
        c.c *= 0.95;
    toLinearRgb(c, rgb);
    auto ch = [](double v) { return qBound(0, int(std::lround(fromLinear(qBound(0.0, v, 1.0)) * 255)), 255); };
    return QColor(ch(rgb[0]), ch(rgb[1]), ch(rgb[2])).name();
}

// Светлота и насыщенность встроенных тем (их оттенок — около 260°): роли
// палитры у всех источников совпадают, меняется только оттенок.
// accent — роль акцента: её насыщенность берём от обоев, а не от фона
struct Tone {
    const char *name;
    double l, c;
    double hueShift; // сдвиг оттенка от главного, как blue и teal у встроенной
    bool accent;
};
const Tone kDark[] = {
    {"base", 0.245, 0.032, 0, false},     {"mantle", 0.215, 0.031, 0, false},
    {"crust", 0.185, 0.030, 0, false},    {"surface0", 0.306, 0.041, 0, false},
    {"surface1", 0.360, 0.044, 0, false}, {"surface2", 0.425, 0.047, 0, false},
    {"overlay0", 0.485, 0.044, 0, false}, {"overlay1", 0.567, 0.043, 0, false},
    {"overlay2", 0.648, 0.044, 0, false}, {"subtext0", 0.727, 0.039, 0, false},
    {"subtext1", 0.794, 0.037, 0, false}, {"text", 0.870, 0.029, 0, false},
    {"lavender", 0.755, 0.090, 0, true},  {"blue", 0.725, 0.101, -14, true},
    {"teal", 0.690, 0.078, -54, true},
};
const Tone kLight[] = {
    {"base", 0.958, 0.009, 0, false},     {"mantle", 0.934, 0.011, 0, false},
    {"crust", 0.909, 0.013, 0, false},    {"surface0", 0.925, 0.013, 0, false},
    {"surface1", 0.897, 0.016, 0, false}, {"surface2", 0.863, 0.019, 0, false},
    {"overlay0", 0.743, 0.027, 0, false}, {"overlay1", 0.632, 0.036, 0, false},
    {"overlay2", 0.518, 0.040, 0, false}, {"subtext0", 0.464, 0.040, 0, false},
    {"subtext1", 0.384, 0.040, 0, false}, {"text", 0.299, 0.036, 0, false},
    {"lavender", 0.532, 0.154, 0, true},  {"blue", 0.563, 0.117, -18, true},
    {"teal", 0.583, 0.084, -70, true},
};

QString run(const QString &exe, const QStringList &args)
{
    if (QStandardPaths::findExecutable(exe).isEmpty())
        return {};
    QProcess p;
    p.start(exe, args);
    if (!p.waitForFinished(700))
        return {};
    return QString::fromUtf8(p.readAllStandardOutput());
}

QString expand(QString path)
{
    path = path.trimmed();
    if (path.startsWith(QLatin1String("~/")))
        path = QDir::homePath() + path.mid(1);
    return QFileInfo(path).isFile() ? path : QString();
}

// Командная строка запущенной программы по имени (swaybg -i путь) или пустой список
QStringList cmdlineOf(const QString &name)
{
    for (const QString &pid : QDir(QStringLiteral("/proc")).entryList(QDir::Dirs)) {
        if (!pid.front().isDigit())
            continue;
        QFile f(QStringLiteral("/proc/") + pid + QStringLiteral("/cmdline"));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QList<QByteArray> parts = f.readAll().split('\0');
        if (parts.isEmpty() || QFileInfo(QString::fromLocal8Bit(parts.first())).fileName() != name)
            continue;
        QStringList out;
        for (const QByteArray &a : parts)
            out << QString::fromLocal8Bit(a);
        return out;
    }
    return {};
}
}

namespace WallpaperPalette {

QString current()
{
    const bool hypr = !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE");
    // hyprpaper: «монитор = путь» на строку
    if (hypr && !cmdlineOf(QStringLiteral("hyprpaper")).isEmpty()) {
        const QString out = run(QStringLiteral("hyprctl"), {QStringLiteral("hyprpaper"), QStringLiteral("listactive")});
        for (const QString &line : out.split(QLatin1Char('\n'))) {
            const qsizetype eq = line.indexOf(QLatin1String(" = "));
            if (eq >= 0)
                if (const QString p = expand(line.mid(eq + 3)); !p.isEmpty())
                    return p;
        }
    }
    // swww / awww: «…: currently displaying: image: путь»
    for (const QString &tool : {QStringLiteral("awww"), QStringLiteral("swww")}) {
        if (cmdlineOf(tool + QStringLiteral("-daemon")).isEmpty())
            continue;
        static const QRegularExpression re(QStringLiteral("image: (.+)$"), QRegularExpression::MultilineOption);
        const auto m = re.match(run(tool, {QStringLiteral("query")}));
        if (m.hasMatch())
            if (const QString p = expand(m.captured(1)); !p.isEmpty())
                return p;
    }
    // wpaperd: «монитор: путь» на строку
    if (!cmdlineOf(QStringLiteral("wpaperd")).isEmpty()) {
        const QString out = run(QStringLiteral("wpaperctl"), {QStringLiteral("all-wallpapers")});
        for (const QString &line : out.split(QLatin1Char('\n'))) {
            const qsizetype colon = line.indexOf(QLatin1String(": "));
            if (colon >= 0)
                if (const QString p = expand(line.mid(colon + 2)); !p.isEmpty())
                    return p;
        }
    }
    // swaybg -i путь
    const QStringList swaybg = cmdlineOf(QStringLiteral("swaybg"));
    for (qsizetype i = 0; i + 1 < swaybg.size(); ++i)
        if (swaybg[i] == QLatin1String("-i") || swaybg[i] == QLatin1String("--image"))
            if (const QString p = expand(swaybg[i + 1]); !p.isEmpty())
                return p;

    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    // waypaper помнит последние обои у себя в конфиге
    if (QFileInfo::exists(config + QStringLiteral("/waypaper/config.ini"))) {
        const QSettings w(config + QStringLiteral("/waypaper/config.ini"), QSettings::IniFormat);
        const QString p = expand(w.value(QStringLiteral("Settings/wallpaper")).toString().section(QLatin1Char(','), 0, 0));
        if (!p.isEmpty())
            return p;
    }
    // Конфиг hyprpaper: «wallpaper = монитор,путь» или блок «wallpaper { path = … }»
    QFile hp(config + QStringLiteral("/hypr/hyprpaper.conf"));
    if (hp.open(QIODevice::ReadOnly)) {
        static const QRegularExpression line(QStringLiteral("^\\s*(?:wallpaper\\s*=\\s*[^,]*,|path\\s*=)\\s*(.+?)\\s*$"),
                                             QRegularExpression::MultilineOption);
        auto it = line.globalMatch(QString::fromUtf8(hp.readAll()));
        while (it.hasNext())
            if (const QString p = expand(it.next().captured(1)); !p.isEmpty())
                return p;
    }
    return {};
}

QVariantMap fromImage(const QImage &source, bool dark)
{
    if (source.isNull())
        return {};
    const QImage img = source.scaled(96, 96, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                           .convertToFormat(QImage::Format_RGB32);

    // Гистограмма оттенков, вес — насыщенность. Почти чёрное и почти белое
    // не считаем: их оттенок случаен
    constexpr int kBins = 36;
    std::array<double, kBins> weight{}, chroma{};
    double total = 0;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const Lch c = toLch(row[x]);
            if (c.l < 0.12 || c.l > 0.97 || c.c < 0.02)
                continue;
            const int bin = int(c.h / (360.0 / kBins)) % kBins;
            const double w = c.c * c.c; // яркие пятна важнее большой серой площади
            weight[bin] += w;
            chroma[bin] += c.c * w;
            total += c.c;
        }
    }
    // Сглаживаем по соседям, чтобы оттенок на границе двух корзин не терялся
    int best = -1;
    double bestScore = 0;
    for (int i = 0; i < kBins; ++i) {
        const double score = weight[i] + 0.5 * (weight[(i + 1) % kBins] + weight[(i + kBins - 1) % kBins]);
        if (score > bestScore) {
            bestScore = score;
            best = i;
        }
    }

    // Насыщенность обоев: средняя по пикселям, 0 — серые
    const double colorful = total / (img.width() * img.height());
    double hue = 262; // серые обои — холодный нейтральный, как у встроенных тем
    double seedC = 0;
    if (best >= 0 && colorful > 0.006) {
        // Точный оттенок — средний по корзине и соседям, с весами
        double sx = 0, sy = 0;
        for (int d = -1; d <= 1; ++d) {
            const int i = (best + d + kBins) % kBins;
            const double h = (i + 0.5) * (360.0 / kBins) * M_PI / 180;
            sx += weight[i] * std::cos(h);
            sy += weight[i] * std::sin(h);
        }
        hue = std::atan2(sy, sx) * 180 / M_PI;
        if (hue < 0)
            hue += 360;
        seedC = weight[best] > 0 ? chroma[best] / weight[best] : 0;
    }
    // Фон: у серых обоев почти без оттенка, у ярких — как у встроенных и чуть больше
    const double bgK = qBound(0.25, colorful / 0.05, 1.25);
    // Акцент: от насыщенности главного цвета, но не блёклый и не кислотный
    const double accentK = seedC > 0 ? qBound(0.6, seedC / 0.12, 1.3) : 0.5;

    QVariantMap out;
    for (const Tone &t : dark ? kDark : kLight) {
        double h = std::fmod(hue + t.hueShift + 360, 360);
        out.insert(QLatin1String(t.name), hex({t.l, t.c * (t.accent ? accentK : bgK), h}));
    }
    return out;
}

QVariantMap fromFile(const QString &path, bool dark)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    // JPEG декодируется сразу уменьшенным — 4K-обои читаются за миллисекунды
    const QSize size = reader.size();
    if (size.isValid() && size.width() > 512)
        reader.setScaledSize(size.scaled(512, 512, Qt::KeepAspectRatio));
    return fromImage(reader.read(), dark);
}

}
