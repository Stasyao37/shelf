#include "DirModel.h"
#include "IconProvider.h"
#include "Picker.h"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTextStream>

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
    // До QGuiApplication: ему нужен экран, а --version должен работать и по SSH, и в скриптах
    for (int i = 1; i < argc; ++i)
        if (qstrcmp(argv[i], "--version") == 0) {
            QTextStream(stdout) << "Shelf " << SHELF_VERSION << '\n';
            return 0;
        }

    // shelf --pick запрос.json — окно выбора файлов для xdg-desktop-portal-shelf
    QString pickRequest;
    for (int i = 1; i + 1 < argc; ++i)
        if (qstrcmp(argv[i], "--pick") == 0)
            pickRequest = QFile::decodeName(argv[i + 1]);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("shelf"));
    app.setOrganizationName(QStringLiteral("shelf"));
    app.setApplicationDisplayName(QStringLiteral("Shelf"));
    app.setApplicationVersion(QStringLiteral(SHELF_VERSION));

    // Становится app_id окна в Wayland — по нему Hyprland и .desktop узнают приложение
    app.setDesktopFileName(QStringLiteral("org.stasyao.shelf"));

    const QString iconTheme = gtkIconTheme();
    if (!iconTheme.isEmpty())
        QIcon::setThemeName(iconTheme);
    QIcon::setFallbackThemeName(QStringLiteral("Adwaita"));
    // Своя иконка из hicolor; до установки — та, что в теме
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("org.stasyao.shelf"),
                                       QIcon::fromTheme(QStringLiteral("system-file-manager"))));

    // Значки интерфейса — вшитые символы Nerd Font: не зависим от установленных шрифтов
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Shelf/fonts/ShelfIcons.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Shelf/fonts/ShelfIconsPropo.ttf"));

    // Свои контролы рисуем сами; Basic не тянет вид qt6ct/kvantum
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    if (!pickRequest.isEmpty()) {
        if (!Picker::load(pickRequest))
            return 2;
        // Свой app_id — чтобы Hyprland держал окно выбора плавающим, по центру
        app.setDesktopFileName(QStringLiteral("org.stasyao.shelf.picker"));
    }

    // shelf [папка | файл | file://…] — для файла открываем его папку и выделяем его
    QString start = pickRequest.isEmpty() ? app.arguments().value(1, QDir::homePath())
                                          : Picker().currentFolder();
    if (start.isEmpty())
        start = QDir::homePath();
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
