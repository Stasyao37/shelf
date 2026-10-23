#pragma once

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// Какой терминал открывать: настройка [apps] terminal → $TERMINAL → xdg-terminal-exec →
// первый найденный из известных. Знает, как каждому передать папку и команду.
class Terminal : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString current READ current CONSTANT)
    Q_PROPERTY(QStringList available READ available CONSTANT)

public:
    explicit Terminal(QObject *parent = nullptr);

    // Имя программы, которую сейчас откроем (пусто — терминала нет вовсе)
    static QString current();
    // Установленные терминалы из известных — для настроек
    static QStringList available();

    // Терминал в папке
    static bool openIn(const QString &dir);
    // Консольная программа (Terminal=true в .desktop) в терминале
    static bool run(const QStringList &command, const QString &dir = {});
};
