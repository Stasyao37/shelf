#include "DirModel.h"
#include "IconProvider.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

namespace {
// Тема иконок та же, что у GTK-приложений
QString gtkIconTheme()
{
    QProcess p;
    p.start(QStringLiteral("gsettings"), {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"),
                                          QStringLiteral("icon-theme")});
    if (!p.waitForFinished(1000))
        return {};
    QString name = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (name.size() >= 2 && name.startsWith(QLatin1Char('\'')))
        name = name.mid(1, name.size() - 2);
    return name;
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("shelf"));
    app.setOrganizationName(QStringLiteral("shelf"));
    app.setApplicationDisplayName(QStringLiteral("Shelf"));
    // Становится app_id окна в Wayland — по нему Hyprland и .desktop узнают приложение
    app.setDesktopFileName(QStringLiteral("org.stasyao.shelf"));

    const QString iconTheme = gtkIconTheme();
    if (!iconTheme.isEmpty())
        QIcon::setThemeName(iconTheme);
    QIcon::setFallbackThemeName(QStringLiteral("Adwaita"));
    // Своя иконка из hicolor; до установки — та, что в теме
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("org.stasyao.shelf"),
                                       QIcon::fromTheme(QStringLiteral("system-file-manager"))));

    // Свои контролы рисуем сами; Basic не тянет вид qt6ct/kvantum
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // shelf [папка | файл | file://…] — для файла открываем его папку и выделяем его
    QString start = app.arguments().value(1, QDir::homePath());
    QString select;
    start = DirModel::expandPath(start);
    if (!QFileInfo(start).isDir()) {
        select = start;
        start = QFileInfo(start).absolutePath();
        if (!QFileInfo(start).isDir())
            start = QDir::homePath();
    }

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.setInitialProperties({{QStringLiteral("startPath"), start}, {QStringLiteral("startSelect"), select}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Shelf", "Main");
    return app.exec();
}
