#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Сигнал о смене языка для тех, кто строит строки в C++ (DirModel, Places)
class LanguageNotifier : public QObject
{
    Q_OBJECT
signals:
    void changed();
};

// Язык интерфейса: как в системе или выбранный в настройках (view/language в shelf.conf).
// Исходные строки — русские; переводы — i18n/<код>.json, вшитые в программу.
// Смена применяется сразу: QML перечитывает qsTr, модели — свои строки (changed)
class Language : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Выбор: "" — как в системе, иначе код языка
    Q_PROPERTY(QString choice READ choice WRITE setChoice NOTIFY changed)
    // Язык, на котором сейчас интерфейс
    Q_PROPERTY(QString current READ current NOTIFY changed)
    // Язык системы, если он поддерживается, иначе английский
    Q_PROPERTY(QString system READ system CONSTANT)
    // [{ code, name }] — поддерживаемые языки, каждый своим названием
    Q_PROPERTY(QVariantList options READ options CONSTANT)

public:
    explicit Language(QObject *parent = nullptr);

    QString choice() const;
    void setChoice(const QString &code);
    static QString current();
    QString system() const { return systemLanguage(); }
    QVariantList options() const;

    // До загрузки QML: ставит переводчик по сохранённому выбору
    static void init();
    // Без сохранения — для тестов и init()
    static void apply(const QString &choice);
    // Язык системы, если он поддерживается, иначе английский
    static QString systemLanguage();
    static LanguageNotifier *notifier();

signals:
    void changed();
};
