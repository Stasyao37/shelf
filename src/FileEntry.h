#pragma once

#include <QDateTime>
#include <QFileInfo>
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

    static FileEntry fromInfo(const QFileInfo &fi, const QMimeDatabase &db, bool countChildren);
};

QString formatSize(qint64 bytes);
QString formatDate(const QDateTime &dt);
// Имя иконки папки: у XDG-папок свои (folder-download, user-home…)
QString folderIcon(const QString &path);
