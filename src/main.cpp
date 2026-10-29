#include "DirModel.h"
#include "Environment.h"
#include "GlassIcons.h"
#include "IconProvider.h"
#include "Language.h"
#include "Picker.h"
#include "UserPath.h"

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

    // До всего остального: по PATH ищутся matugen, терминалы, tumbler и программы для файлов
    extendUserPath();
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("shelf"));
    app.setOrganizationName(QStringLiteral("shelf"));
    app.setApplicationDisplayName(QStringLiteral("Shelf"));
    app.setApplicationVersion(QStringLiteral(SHELF_VERSION));
    // Язык — до QML: первые же строки окна должны быть уже переведены
    Language::init();

    // Становится app_id окна в Wayland — по нему Hyprland и .desktop узнают приложение
    app.setDesktopFileName(QStringLiteral("io.github.nook_apps.Shelf"));

    const QString iconTheme = gtkIconTheme();
    if (!iconTheme.isEmpty())
        QIcon::setThemeName(iconTheme);
    QIcon::setFallbackThemeName(QStringLiteral("Adwaita"));
    // Своя иконка из hicolor; до установки — та, что в теме
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.nook_apps.Shelf"),
                                       QIcon::fromTheme(QStringLiteral("system-file-manager"))));

    // Значки интерфейса — вшитые символы Nerd Font: не зависим от установленных шрифтов
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Shelf/fonts/ShelfIcons.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Shelf/fonts/ShelfIconsPropo.ttf"));

    // qt6ct кладёт в шрифт приложения имя начертания («Regular»), его наследуют
    // все шрифты, и оно перебивает жирность: Font.DemiBold и font.bold рисовались
    // бы обычным. Начертание выбираем по весу сами. Вес тоже обычный: у Shelf свои
    // шрифты (Theme.fontFamily), а вес из qt6ct (там Medium у Fira Code) задан для
    // другого шрифта и без «Regular» сделал бы весь текст жирнее
    QFont font = QGuiApplication::font();
    if (!font.styleName().isEmpty() || font.weight() != QFont::Normal) {
        font.setStyleName(QString());
        font.setWeight(QFont::Normal);
        QGuiApplication::setFont(font);
    }

    // Переезд на io.github.nook_apps.Shelf: старые назначения и правила — на новое имя
    if (pickRequest.isEmpty())
        Environment::migrateAppId();

    // Свои контролы рисуем сами; Basic не тянет вид qt6ct/kvantum
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    if (!pickRequest.isEmpty()) {
        if (!Picker::load(pickRequest))
            return 2;
        // Свой app_id — чтобы Hyprland держал окно выбора плавающим, по центру
        app.setDesktopFileName(QStringLiteral("io.github.nook_apps.Shelf.Picker"));
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
    engine.addImageProvider(QStringLiteral("glass"), new GlassIcons);
    engine.setInitialProperties({{QStringLiteral("startPath"), start}, {QStringLiteral("startSelect"), select}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Shelf", "Main");
    return app.exec();
}
