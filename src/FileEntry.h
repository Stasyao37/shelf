#pragma once

#include <QDateTime>
#include <QFileInfo>
#include <QMimeType>
#include <QString>

class QMimeDatabase;

// Одна строка списка файлов. Собирается и в GUI-потоке (листинг папки),
// и в потоке поиска — поэтому без QObject.
struct FileEntry {
    QString name;
    QString path;
    QString dir;        // родительская папка (для результатов поиска)
    QString mimeType;
    QString mimeComment;
    QString icon;       // "имя,запасное" для image://icon/
    QDateTime mtime;
    qint64 size = 0;
    int childCount = -1; // только для папок в обычном листинге
    bool isDir = false;
    bool isHidden = false;
    bool isLink = false;
    // Нельзя прочитать, или только чтение в папке, куда писать можно
    // (в /usr и других системных папках замок был бы на каждом файле)
    bool isLocked = false;

    static FileEntry fromInfo(const QFileInfo &fi, const QMimeDatabase &db, bool countChildren);
};

QString formatSize(qint64 bytes);
QString formatDate(const QDateTime &dt);
// Тип файла: по расширению, для незнакомых — по содержимому
QMimeType mimeForFile(const QFileInfo &fi, const QMimeDatabase &db);
// Имя иконки папки: у XDG-папок свои (folder-download, user-home…)
QString folderIcon(const QString &path);
