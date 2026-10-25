#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Что есть в системе — для первой настройки: оболочка, matugen, миниатюры, утилиты.
// И подключение Shelf к matugen: свой шаблон + блок в ~/.config/matugen/config.toml.
class Environment : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hyprland READ hyprland CONSTANT)
    Q_PROPERTY(bool quickshell READ quickshell CONSTANT)
    Q_PROPERTY(bool matugen READ matugen CONSTANT)
    Q_PROPERTY(bool matugenConnected READ matugenConnected NOTIFY changed)
    Q_PROPERTY(bool blur READ blur CONSTANT)
    Q_PROPERTY(bool tumbler READ tumbler CONSTANT)
    Q_PROPERTY(bool ffmpegthumbnailer READ ffmpegthumbnailer CONSTANT)
    Q_PROPERTY(bool gio READ gio CONSTANT)
    Q_PROPERTY(bool xdgUtils READ xdgUtils CONSTANT)
    Q_PROPERTY(QString distro READ distro CONSTANT)
    // Бэкенд портала установлен там, где его ищет xdg-desktop-portal
    Q_PROPERTY(bool portalBackend READ portalBackend CONSTANT)
    // Портал отдаёт выбор файлов Shelf (FileChooser=shelf)
    Q_PROPERTY(bool portalPicker READ portalPicker NOTIFY changed)

public:
    explicit Environment(QObject *parent = nullptr);

    bool hyprland() const;
    bool quickshell() const;
    bool matugen() const;
    bool matugenConnected() const;
    bool blur() const;
    bool tumbler() const;
    bool ffmpegthumbnailer() const;
    bool gio() const;
    bool xdgUtils() const;
    QString distro() const { return m_distro; }

    // Команда установки пакета для этого дистрибутива (для подсказки, сами не ставим)
    Q_INVOKABLE QString installCommand(const QString &package) const;
    // Возвращает пустую строку при успехе, иначе текст ошибки
    Q_INVOKABLE QString connectMatugen();

    bool portalBackend() const;
    bool portalPicker() const;
    // Включить/выключить окна выбора Shelf; пустая строка — получилось
    Q_INVOKABLE QString setPortalPicker(bool on);
    // Команда, которой ставят описание портала, если Shelf собран в ~/.local
    Q_INVOKABLE QString portalInstallCommand() const;

    static QString matugenConfig();
    // Один раз: старый идентификатор org.stasyao.shelf → io.github.nook_apps.Shelf
    // (проводник по умолчанию, правила окна выбора, старый ярлык в ~/.local)
    static void migrateAppId();

signals:
    void changed();

private:
    QString m_distro;     // ID из os-release
    QString m_distroLike; // ID_LIKE
    mutable int m_blur = -1;
};
