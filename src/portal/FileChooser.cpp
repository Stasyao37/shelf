#include "FileChooser.h"

#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUrl>

QDBusArgument &operator<<(QDBusArgument &a, const FilterRule &r)
{
    a.beginStructure();
    a << r.type << r.pattern;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, FilterRule &r)
{
    a.beginStructure();
    a >> r.type >> r.pattern;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const Filter &f)
{
    a.beginStructure();
    a << f.name << f.rules;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, Filter &f)
{
    a.beginStructure();
    a >> f.name >> f.rules;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const Choice &c)
{
    a.beginStructure();
    a << c.id << c.value;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, Choice &c)
{
    a.beginStructure();
    a >> c.id >> c.value;
    a.endStructure();
    return a;
}

namespace {
// Request по handle: портал зовёт Close(), когда приложение передумало
class Request : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")
public:
    explicit Request(QObject *parent, QProcess *proc)
        : QDBusAbstractAdaptor(parent), m_proc(proc) {}
public Q_SLOTS:
    void Close()
    {
        if (m_proc)
            m_proc->terminate();
    }
private:
    QPointer<QProcess> m_proc;
};

// a{sv} с путями приходят как ay с завершающим нулём
QString bytesToPath(const QVariant &v)
{
    QByteArray b = v.toByteArray();
    while (b.endsWith('\0'))
        b.chop(1);
    return QFile::decodeName(b);
}

template <typename T>
T demarshal(const QVariant &v)
{
    T out{};
    if (v.canConvert<QDBusArgument>())
        v.value<QDBusArgument>() >> out;
    else
        out = v.value<T>();
    return out;
}

QJsonObject filterJson(const Filter &f)
{
    QJsonArray globs, mimes;
    for (const FilterRule &r : f.rules)
        (r.type == 1 ? mimes : globs).append(r.pattern);
    return {{"name", f.name}, {"globs", globs}, {"mimes", mimes}};
}

QString shelfExe()
{
    // Тот Shelf, что установлен рядом с бэкендом; иначе — из PATH
    const QString installed = QStringLiteral(SHELF_EXE);
    if (QFileInfo(installed).isExecutable())
        return installed;
    return QStandardPaths::findExecutable(QStringLiteral("shelf"));
}
}

FileChooser::FileChooser(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    qDBusRegisterMetaType<FilterRule>();
    qDBusRegisterMetaType<QList<FilterRule>>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<FilterList>();
    qDBusRegisterMetaType<Choice>();
    qDBusRegisterMetaType<ChoiceList>();
}

uint FileChooser::OpenFile(const QDBusObjectPath &handle, const QString &, const QString &, const QString &title,
                           const QVariantMap &options, const QDBusMessage &message, QVariantMap &)
{
    start(options.value(QStringLiteral("directory")).toBool() ? QStringLiteral("folder") : QStringLiteral("open"),
          handle, title, options, message);
    return 2;
}

uint FileChooser::SaveFile(const QDBusObjectPath &handle, const QString &, const QString &, const QString &title,
                           const QVariantMap &options, const QDBusMessage &message, QVariantMap &)
{
    start(QStringLiteral("save"), handle, title, options, message);
    return 2;
}

uint FileChooser::SaveFiles(const QDBusObjectPath &handle, const QString &, const QString &, const QString &title,
                            const QVariantMap &options, const QDBusMessage &message, QVariantMap &)
{
    start(QStringLiteral("saveFiles"), handle, title, options, message);
    return 2;
}

void FileChooser::start(const QString &mode, const QDBusObjectPath &handle, const QString &title,
                        const QVariantMap &options, const QDBusMessage &message)
{
    message.setDelayedReply(true);
    auto fail = [message] {
        QDBusConnection::sessionBus().send(message.createReply({uint(2), QVariantMap{}}));
    };

    // Запрос для окна выбора
    QJsonObject req{{"mode", mode},
                    {"title", title},
                    {"accept", options.value(QStringLiteral("accept_label")).toString()},
                    {"multiple", options.value(QStringLiteral("multiple")).toBool()}};
    FilterList filters = demarshal<FilterList>(options.value(QStringLiteral("filters")));
    int current = -1;
    if (options.contains(QStringLiteral("current_filter"))) {
        const Filter cf = demarshal<Filter>(options.value(QStringLiteral("current_filter")));
        for (int i = 0; i < filters.size() && current < 0; ++i)
            if (filters[i].name == cf.name)
                current = i;
        if (current < 0 && !cf.rules.isEmpty()) {
            filters.append(cf);
            current = int(filters.size()) - 1;
        }
    }
    QJsonArray jf;
    for (const Filter &f : std::as_const(filters))
        jf.append(filterJson(f));
    req["filters"] = jf;
    req["currentFilter"] = current;
    req["currentName"] = options.value(QStringLiteral("current_name")).toString();
    if (options.contains(QStringLiteral("current_folder")))
        req["currentFolder"] = bytesToPath(options.value(QStringLiteral("current_folder")));
    if (options.contains(QStringLiteral("current_file"))) {
        const QString file = bytesToPath(options.value(QStringLiteral("current_file")));
        req["currentFolder"] = QFileInfo(file).absolutePath();
        req["currentName"] = QFileInfo(file).fileName();
    }
    if (options.contains(QStringLiteral("files"))) {
        QJsonArray names;
        const QDBusArgument arg = options.value(QStringLiteral("files")).value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QByteArray b;
            arg >> b;
            while (b.endsWith('\0'))
                b.chop(1);
            names.append(QFileInfo(QFile::decodeName(b)).fileName());
        }
        arg.endArray();
        req["files"] = names;
    }
    // choices: a(ssa(ss)s) — пункт: id, подпись, варианты (пусто — флажок), значение по умолчанию
    if (options.contains(QStringLiteral("choices"))) {
        QJsonArray choices;
        const QDBusArgument arg = options.value(QStringLiteral("choices")).value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QString id, label, def;
            QJsonArray variants;
            arg.beginStructure();
            arg >> id >> label;
            arg.beginArray();
            while (!arg.atEnd()) {
                QString vid, vlabel;
                arg.beginStructure();
                arg >> vid >> vlabel;
                arg.endStructure();
                variants.append(QJsonArray{vid, vlabel});
            }
            arg.endArray();
            arg >> def;
            arg.endStructure();
            choices.append(QJsonObject{{"id", id}, {"label", label}, {"options", variants}, {"value", def}});
        }
        arg.endArray();
        req["choices"] = choices;
    }

    const QString exe = shelfExe();
    auto *reqFile = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/shelf-pick-XXXXXX.json"), this);
    if (exe.isEmpty() || !reqFile->open()) {
        delete reqFile;
        fail();
        return;
    }
    reqFile->write(QJsonDocument(req).toJson(QJsonDocument::Compact));
    reqFile->flush();

    auto *proc = new QProcess(this);
    // stdout — ответ окна выбора, stderr — в журнал бэкенда
    proc->setProcessChannelMode(QProcess::ForwardedErrorChannel);

    auto *requestObj = new QObject(proc);
    new Request(requestObj, proc);
    QDBusConnection::sessionBus().registerObject(handle.path(), requestObj, QDBusConnection::ExportAdaptors);

    ++m_running;
    emit busy();
    const FilterList sent = filters;
    connect(proc, &QProcess::finished, this, [this, proc, reqFile, message, handle, sent](int code, QProcess::ExitStatus st) {
        QDBusConnection::sessionBus().unregisterObject(handle.path());
        uint response = 1;
        QVariantMap results;
        if (st == QProcess::NormalExit && code == 0) {
            const QByteArray out = proc->readAllStandardOutput().trimmed().split('\n').last();
            const QJsonObject res = QJsonDocument::fromJson(out).object();
            QStringList uris;
            for (const QJsonValue &u : res.value("uris").toArray())
                uris << u.toString();
            if (!uris.isEmpty()) {
                response = 0;
                results[QStringLiteral("uris")] = uris;
                const int fi = res.value("filter").toInt(-1);
                if (fi >= 0 && fi < sent.size())
                    results[QStringLiteral("current_filter")] = QVariant::fromValue(sent[fi]);
                ChoiceList chosen;
                for (const QJsonValue &c : res.value("choices").toArray())
                    chosen.append({c.toArray().at(0).toString(), c.toArray().at(1).toString()});
                if (!chosen.isEmpty())
                    results[QStringLiteral("choices")] = QVariant::fromValue(chosen);
            }
        } else if (st != QProcess::NormalExit || code > 1) {
            response = 2;
        }
        QDBusConnection::sessionBus().send(message.createReply({response, results}));
        reqFile->deleteLater();
        proc->deleteLater();
        if (--m_running == 0)
            emit idle();
    });
    proc->start(exe, {QStringLiteral("--pick"), reqFile->fileName()});
    if (!proc->waitForStarted(5000))
        proc->kill(); // finished придёт с кодом ошибки — ответим 2
}

#include "FileChooser.moc"
