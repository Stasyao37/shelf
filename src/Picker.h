#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Режим выбора файлов (shelf --pick запрос.json): запрос от xdg-desktop-portal-shelf
// и ответ ему — JSON одной строкой в stdout, код выхода 0 (выбрано) или 1 (отмена)
class Picker : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool active READ active CONSTANT)
    // open — файлы, folder — папка, save — сохранить один файл, saveFiles — папка для нескольких
    Q_PROPERTY(QString mode READ mode CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString acceptLabel READ acceptLabel CONSTANT)
    Q_PROPERTY(bool multiple READ multiple CONSTANT)
    // [{ name, globs: [...], mimes: [...] }]
    Q_PROPERTY(QVariantList filters READ filters CONSTANT)
    Q_PROPERTY(int currentFilter READ currentFilter CONSTANT)
    Q_PROPERTY(QString currentName READ currentName CONSTANT)
    Q_PROPERTY(QString currentFolder READ currentFolder CONSTANT)
    Q_PROPERTY(QStringList files READ files CONSTANT)
    // [{ id, label, options: [[id, label], ...], value }]; без options — флажок "true"/"false"
    Q_PROPERTY(QVariantList choices READ choices CONSTANT)

public:
    explicit Picker(QObject *parent = nullptr);

    // Читает запрос; false — файл не прочитан
    static bool load(const QString &requestFile);

    bool active() const;
    QString mode() const;
    QString title() const;
    QString acceptLabel() const;
    bool multiple() const;
    QVariantList filters() const;
    int currentFilter() const;
    QString currentName() const;
    QString currentFolder() const;
    QStringList files() const;
    QVariantList choices() const;

    // choices — [[id, value], ...]
    Q_INVOKABLE void accept(const QStringList &paths, int filter, const QVariantList &choices);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE static bool exists(const QString &path);
};
