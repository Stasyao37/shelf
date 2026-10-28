// xdg-desktop-portal-shelf: бэкенд org.freedesktop.impl.portal.FileChooser.
// Запускается шиной по запросу (D-Bus activation) и завершается, когда окна выбора закрыты.
#include "FileChooser.h"
#include "../UserPath.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    // Шина запускает бэкенд с коротким PATH — shelf из ~/.local/bin иначе не найти
    extendUserPath();
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("xdg-desktop-portal-shelf"));

    QObject root;
    auto *chooser = new FileChooser(&root);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), &root, QDBusConnection::ExportAdaptors)
        || !bus.registerService(QStringLiteral("org.freedesktop.impl.portal.desktop.shelf"))) {
        QTextStream(stderr) << "xdg-desktop-portal-shelf: не удалось занять имя на шине: "
                            << bus.lastError().message() << '\n';
        return 1;
    }

    // Простаиваем 30 с — выходим; следующий запрос запустит заново
    QTimer quit;
    quit.setSingleShot(true);
    quit.setInterval(30000);
    QObject::connect(&quit, &QTimer::timeout, &app, &QCoreApplication::quit);
    QObject::connect(chooser, &FileChooser::idle, &quit, qOverload<>(&QTimer::start));
    // Пока окно выбора открыто — не выходим
    QObject::connect(chooser, &FileChooser::busy, &quit, &QTimer::stop);
    quit.start();

    return app.exec();
}
