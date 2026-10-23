#pragma once

#include <QDateTime>
#include <QString>

// Домашняя корзина по спецификации freedesktop: files/ — сами объекты,
// info/<имя>.trashinfo — откуда и когда удалены.
namespace Trash {

struct Info {
    QString origPath;
    QDateTime deleted;
    bool valid() const { return !origPath.isEmpty(); }
};

QString root();      // ~/.local/share/Trash
QString filesDir();  // ~/.local/share/Trash/files
// Объект лежит прямо в files/ — у него есть .trashinfo, его можно восстановить
bool isTopLevel(const QString &path);
bool contains(const QString &path);
QString infoPath(const QString &path);
Info readInfo(const QString &path);
// Убирает .trashinfo и строку из кэша directorysizes
void forget(const QString &path);

}
