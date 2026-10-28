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
    QString origin;     // в корзине: папка, откуда удалён
    QDateTime deleted;  // в корзине: когда удалён
    qint64 size = 0;
    int childCount = -1; // только для папок в обычном листинге
    bool isDir = false;
    bool isHidden = false;
    bool isLink = false;
    // Нельзя прочитать, или только чтение в папке, куда писать можно
    // (в /usr и других системных папках замок был бы на каждом файле)
    bool isLocked = false;
    // Тип ещё не определён до конца — его доопределяет фон (DirModel)
    bool typePending = false;

    enum class Typing {
        Full,   // по имени, незнакомые — ещё и по содержимому
        ByName, // только по имени; незнакомые помечаются typePending — файл не открываем
        Later,  // никак: typePending. Даже поиск по имени бывает дорог — при нескольких
                // базах типов (flatpak, ~/.local/share/mime) Qt тратит ~0,25 мс на файл
    };
    static FileEntry fromInfo(const QFileInfo &fi, const QMimeDatabase &db, bool countChildren,
                              Typing typing = Typing::Full);
    void setMime(const QMimeType &mt);
};

QString formatSize(qint64 bytes);
QString formatDate(const QDateTime &dt);
// Тип файла: по расширению, для незнакомых — по содержимому (если sniff)
QMimeType mimeForFile(const QFileInfo &fi, const QMimeDatabase &db, bool sniff = true);
// Имя иконки папки: у XDG-папок свои (folder-download, user-home…)
QString folderIcon(const QString &path);
