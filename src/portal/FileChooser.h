#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QList>
#include <QObject>
#include <QVariantMap>

// Типы из спецификации org.freedesktop.impl.portal.FileChooser
struct FilterRule {
    uint type = 0;    // 0 — маска (*.png), 1 — MIME-тип
    QString pattern;
};
struct Filter {
    QString name;
    QList<FilterRule> rules;
};
using FilterList = QList<Filter>;
struct Choice {
    QString id;
    QString value;
};
using ChoiceList = QList<Choice>;

QDBusArgument &operator<<(QDBusArgument &a, const FilterRule &r);
const QDBusArgument &operator>>(const QDBusArgument &a, FilterRule &r);
QDBusArgument &operator<<(QDBusArgument &a, const Filter &f);
const QDBusArgument &operator>>(const QDBusArgument &a, Filter &f);
QDBusArgument &operator<<(QDBusArgument &a, const Choice &c);
const QDBusArgument &operator>>(const QDBusArgument &a, Choice &c);

Q_DECLARE_METATYPE(FilterRule)
Q_DECLARE_METATYPE(Filter)
Q_DECLARE_METATYPE(Choice)

// Бэкенд портала: на каждый запрос запускает `shelf --pick` и отвечает, когда окно закрылось
class FileChooser : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit FileChooser(QObject *parent);

    uint version() const { return 4; }

public Q_SLOTS:
    uint OpenFile(const QDBusObjectPath &handle, const QString &appId, const QString &parentWindow,
                  const QString &title, const QVariantMap &options, const QDBusMessage &message,
                  QVariantMap &results);
    uint SaveFile(const QDBusObjectPath &handle, const QString &appId, const QString &parentWindow,
                  const QString &title, const QVariantMap &options, const QDBusMessage &message,
                  QVariantMap &results);
    uint SaveFiles(const QDBusObjectPath &handle, const QString &appId, const QString &parentWindow,
                   const QString &title, const QVariantMap &options, const QDBusMessage &message,
                   QVariantMap &results);

Q_SIGNALS:
    // Ни одного открытого окна — можно завершаться
    void idle();
    // Открылось окно выбора
    void busy();

private:
    void start(const QString &mode, const QDBusObjectPath &handle, const QString &title,
               const QVariantMap &options, const QDBusMessage &message);
    int m_running = 0;
};
