#pragma once

#include <QDir>
#include <QFileInfo>
#include <QStringList>

// Лаунчеры Hyprland и D-Bus запускают Shelf с коротким PATH — без папок, которые
// оболочка добавляет при входе. Тогда matugen из ~/.cargo/bin или терминал из
// ~/.local/bin «не установлены», хотя они есть. Дописываем такие папки в конец PATH
// (системные остаются первыми); потомки — программы, открытые из Shelf, — получают его же
inline void extendUserPath()
{
    const QString home = QDir::homePath();
    QStringList path = qEnvironmentVariable("PATH").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    const QStringList extra = {home + QStringLiteral("/.local/bin"), home + QStringLiteral("/.cargo/bin"),
                               home + QStringLiteral("/go/bin"), home + QStringLiteral("/.nix-profile/bin"),
                               home + QStringLiteral("/bin")};
    bool changed = false;
    for (const QString &dir : extra) {
        if (!QFileInfo(dir).isDir())
            continue;
        // ~/.cargo/bin/ и ~/.cargo/bin — одна папка
        bool listed = false;
        for (const QString &p : std::as_const(path))
            listed = listed || QDir::cleanPath(p) == dir;
        if (listed)
            continue;
        path << dir;
        changed = true;
    }
    if (changed)
        qputenv("PATH", path.join(QLatin1Char(':')).toLocal8Bit());
}
