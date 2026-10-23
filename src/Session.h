#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Вкладки окна на случай аварийного закрытия. Каждый процесс держит свой файл
// в ~/.local/state/shelf/sessions; при обычном закрытии окна он удаляется.
// Файл, чей процесс уже не жив (или из прошлой загрузки), — повод предложить восстановление.
class Session : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Session(QObject *parent = nullptr);

    Q_INVOKABLE void save(const QStringList &paths, int current);
    // Окно закрыли сами — восстанавливать нечего
    Q_INVOKABLE void clear();
    // Самая свежая осиротевшая сессия: { paths, current } с существующими папками, либо пусто.
    // Файл сразу удаляется — два окна, запущенные подряд, не предложат одно и то же
    Q_INVOKABLE QVariantMap takeOrphan();

private:
    static QString dir();
    static QString bootId();
    QString ownFile() const;
};
