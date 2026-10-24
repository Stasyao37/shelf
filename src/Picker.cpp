#include "Picker.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QUrl>

namespace {
QJsonObject g_request;
bool g_active = false;
bool g_done = false;
}

Picker::Picker(QObject *parent)
    : QObject(parent)
{
}

bool Picker::load(const QString &requestFile)
{
    QFile f(requestFile);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    g_request = QJsonDocument::fromJson(f.readAll()).object();
    g_active = !g_request.isEmpty();
    return g_active;
}

bool Picker::active() const { return g_active; }
QString Picker::mode() const { return g_request.value("mode").toString(QStringLiteral("open")); }
QString Picker::title() const { return g_request.value("title").toString(); }
QString Picker::acceptLabel() const { return g_request.value("accept").toString(); }
bool Picker::multiple() const { return g_request.value("multiple").toBool(); }
QVariantList Picker::filters() const { return g_request.value("filters").toArray().toVariantList(); }
int Picker::currentFilter() const { return g_request.value("currentFilter").toInt(-1); }
QString Picker::currentName() const { return g_request.value("currentName").toString(); }
QString Picker::currentFolder() const { return g_request.value("currentFolder").toString(); }
QVariantList Picker::choices() const { return g_request.value("choices").toArray().toVariantList(); }

QStringList Picker::files() const
{
    QStringList out;
    for (const QJsonValue &v : g_request.value("files").toArray())
        out << v.toString();
    return out;
}

void Picker::accept(const QStringList &paths, int filter, const QVariantList &choices)
{
    if (g_done)
        return;
    g_done = true;
    QJsonArray uris, chosen;
    for (const QString &p : paths)
        uris.append(QUrl::fromLocalFile(p).toString(QUrl::FullyEncoded));
    for (const QVariant &c : choices)
        chosen.append(QJsonArray::fromVariantList(c.toList()));
    const QJsonObject res{{"uris", uris}, {"filter", filter}, {"choices", chosen}};
    QTextStream(stdout) << QJsonDocument(res).toJson(QJsonDocument::Compact) << '\n';
    QCoreApplication::exit(0);
}

void Picker::cancel()
{
    if (g_done)
        return;
    g_done = true;
    QCoreApplication::exit(1);
}

bool Picker::exists(const QString &path)
{
    return QFileInfo::exists(path);
}
