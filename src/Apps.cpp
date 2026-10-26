#include "Apps.h"

#include "Terminal.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

#include <algorithm>

namespace {
QString unescape(const QString &v)
{
    QString out;
    out.reserve(v.size());
    for (int i = 0; i < v.size(); ++i) {
        if (v[i] != QLatin1Char('\\') || i + 1 == v.size()) {
            out += v[i];
            continue;
        }
        const QChar c = v[++i];
        out += c == QLatin1Char('s') ? QChar(' ') : c == QLatin1Char('n') ? QChar('\n') : c == QLatin1Char('t') ? QChar('\t') : c;
    }
    return out;
}

// Группы ini-файла: группа → ключ → значение. Нам нужны только простые ключи.
using Ini = QHash<QString, QHash<QString, QString>>;

Ini readIni(const QString &path, const QSet<QString> &groups)
{
    Ini ini;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return ini;
    QTextStream in(&f);
    QString group;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            group = line.mid(1, line.size() - 2);
            continue;
        }
        if (!groups.contains(group))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq > 0)
            ini[group].insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return ini;
}

QStringList splitList(const QString &v)
{
    return v.split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

// Name[ru_RU] → Name[ru] → Name
QString localized(const QHash<QString, QString> &g, const QString &key)
{
    const QString loc = QLocale().name(); // ru_RU
    for (const QString &k : {key + QLatin1Char('[') + loc + QLatin1Char(']'),
                             key + QLatin1Char('[') + loc.section(QLatin1Char('_'), 0, 0) + QLatin1Char(']'), key}) {
        const auto it = g.constFind(k);
        if (it != g.cend())
            return unescape(*it);
    }
    return {};
}

QStringList currentDesktops()
{
    QStringList res;
    for (const QString &d : qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'), Qt::SkipEmptyParts))
        res << d.toLower();
    return res;
}

// Порядок по спецификации mime-apps: сначала пользовательские, сначала для текущего DE
QStringList mimeappsFiles()
{
    QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    for (const QString &d : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation))
        dirs << d;
    QStringList files;
    for (const QString &d : std::as_const(dirs)) {
        for (const QString &de : currentDesktops())
            files << d + QLatin1Char('/') + de + QStringLiteral("-mimeapps.list");
        files << d + QStringLiteral("/mimeapps.list");
    }
    return files;
}
}

Apps::Apps(QObject *parent)
    : QObject(parent)
{
}

void Apps::scan()
{
    // Диалог открывают редко, а установка программы должна сразу попадать в список
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_scannedAt < 3000 && !m_apps.isEmpty())
        return;
    m_scannedAt = now;
    m_apps.clear();
    m_byId.clear();

    const QStringList desktops = currentDesktops();
    const QSet<QString> groups = {QStringLiteral("Desktop Entry")};
    // Первый каталог в списке важнее: ~/.local/share/applications перекрывает /usr/share
    for (const QString &base : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        QDirIterator it(base, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            // id: путь относительно applications/, «/» → «-» (kde/foo.desktop → kde-foo.desktop)
            const QString id = file.mid(base.size() + 1).replace(QLatin1Char('/'), QLatin1Char('-'));
            if (m_byId.contains(id))
                continue;
            const auto g = readIni(file, groups).value(QStringLiteral("Desktop Entry"));
            App a;
            a.id = id;
            m_byId.insert(id, -1); // и скрытый файл перекрывает системный
            if (g.value(QStringLiteral("Type")) != QLatin1String("Application") ||
                g.value(QStringLiteral("Hidden")) == QLatin1String("true") || g.value(QStringLiteral("Exec")).isEmpty())
                continue;
            const QString tryExec = g.value(QStringLiteral("TryExec"));
            if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty() && !QFileInfo(tryExec).isExecutable())
                continue;
            const QStringList only = splitList(g.value(QStringLiteral("OnlyShowIn")).toLower());
            const QStringList notIn = splitList(g.value(QStringLiteral("NotShowIn")).toLower());
            const bool shownHere = (only.isEmpty() || std::any_of(only.cbegin(), only.cend(), [&](const QString &d) { return desktops.contains(d); }))
                                   && std::none_of(notIn.cbegin(), notIn.cend(), [&](const QString &d) { return desktops.contains(d); });
            a.file = file;
            a.name = localized(g, QStringLiteral("Name"));
            a.comment = localized(g, QStringLiteral("GenericName"));
            if (a.comment.isEmpty())
                a.comment = localized(g, QStringLiteral("Comment"));
            a.icon = g.value(QStringLiteral("Icon"));
            a.exec = unescape(g.value(QStringLiteral("Exec")));
            a.terminal = g.value(QStringLiteral("Terminal")) == QLatin1String("true");
            a.mimeTypes = splitList(g.value(QStringLiteral("MimeType")));
            a.noDisplay = g.value(QStringLiteral("NoDisplay")) == QLatin1String("true") || !shownHere;
            if (a.name.isEmpty())
                continue;
            m_byId[id] = int(m_apps.size());
            m_apps.append(a);
        }
    }
}

Apps::Assoc Apps::readAssociations() const
{
    Assoc res;
    const QSet<QString> groups = {QStringLiteral("Default Applications"), QStringLiteral("Added Associations"),
                                  QStringLiteral("Removed Associations")};
    auto merge = [](QHash<QString, QStringList> &into, const QHash<QString, QString> &from) {
        for (auto it = from.cbegin(); it != from.cend(); ++it)
            for (const QString &id : splitList(it.value()))
                if (!into[it.key()].contains(id))
                    into[it.key()] << id;
    };
    for (const QString &f : mimeappsFiles()) {
        const Ini ini = readIni(f, groups);
        merge(res.defaults, ini.value(QStringLiteral("Default Applications")));
        merge(res.added, ini.value(QStringLiteral("Added Associations")));
        merge(res.removed, ini.value(QStringLiteral("Removed Associations")));
    }
    // Старый формат: defaults.list в /usr/share/applications — в самом конце
    for (const QString &d : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation))
        merge(res.defaults, readIni(d + QStringLiteral("/defaults.list"), {QStringLiteral("Default Applications")})
                                .value(QStringLiteral("Default Applications")));
    return res;
}

QString Apps::defaultFor(const QString &mime, const Assoc &assoc) const
{
    // Первое установленное из списка; для text/x-c++src подойдёт и умолчание text/plain.
    // Контейнеры не наследуем: .pptx, .xlsx, .odt, .epub — это ZIP внутри, но открывать
    // их архиватором или проводником (у zip по умолчанию часто Nautilus) — не то
    QStringList chain = {mime};
    const QMimeType mt = QMimeDatabase().mimeTypeForName(mime);
    if (mt.isValid()) {
        chain << mt.aliases();
        for (const QString &anc : mt.allAncestors())
            if (anc != QLatin1String("application/zip") && anc != QLatin1String("application/octet-stream"))
                chain << anc;
    }
    for (const QString &m : std::as_const(chain))
        for (const QString &id : assoc.defaults.value(m))
            if (m_byId.value(id, -1) >= 0)
                return id;
    // Нет явного умолчания — первое заявившее этот тип, как делает xdg-open
    for (const QString &m : std::as_const(chain)) {
        for (const QString &id : assoc.added.value(m))
            if (m_byId.value(id, -1) >= 0 && !assoc.removed.value(m).contains(id))
                return id;
        for (const App &a : m_apps)
            if (a.mimeTypes.contains(m) && !assoc.removed.value(m).contains(a.id))
                return a.id;
    }
    return {};
}

QVariantMap Apps::toMap(const App &a, bool isDefault)
{
    return {{QStringLiteral("id"), a.id}, {QStringLiteral("name"), a.name}, {QStringLiteral("comment"), a.comment},
            {QStringLiteral("icon"), a.icon}, {QStringLiteral("isDefault"), isDefault}};
}

QVariantList Apps::appsFor(const QString &mimeType)
{
    scan();
    const Assoc assoc = readAssociations();
    const QString def = defaultFor(mimeType, assoc);

    QStringList ids;
    if (!def.isEmpty())
        ids << def;
    auto add = [&](const QString &id) {
        if (m_byId.value(id, -1) >= 0 && !ids.contains(id))
            ids << id;
    };

    QStringList chain = {mimeType};
    const QMimeType mt = QMimeDatabase().mimeTypeForName(mimeType);
    if (mt.isValid()) {
        chain << mt.aliases();
        // application/octet-stream — предок почти всего, по нему подходит что угодно
        for (const QString &anc : mt.allAncestors())
            if (anc != QLatin1String("application/octet-stream"))
                chain << anc;
    }
    // Точные совпадения идут раньше совпадений по родительским типам
    for (const QString &m : std::as_const(chain)) {
        const QStringList removed = assoc.removed.value(m);
        for (const QString &id : assoc.added.value(m))
            if (!removed.contains(id))
                add(id);
        QList<const App *> group;
        for (const App &a : m_apps)
            if (!a.noDisplay && a.mimeTypes.contains(m) && !removed.contains(a.id))
                group << &a;
        std::sort(group.begin(), group.end(), [](const App *x, const App *y) {
            return QString::localeAwareCompare(x->name, y->name) < 0;
        });
        for (const App *a : std::as_const(group))
            add(a->id);
    }

    QVariantList res;
    for (const QString &id : std::as_const(ids))
        res << toMap(m_apps.at(m_byId.value(id)), id == def);
    return res;
}

QVariantList Apps::allApps()
{
    scan();
    QList<const App *> list;
    for (const App &a : m_apps)
        if (!a.noDisplay)
            list << &a;
    std::sort(list.begin(), list.end(), [](const App *x, const App *y) {
        return QString::localeAwareCompare(x->name, y->name) < 0;
    });
    QVariantList res;
    for (const App *a : std::as_const(list))
        res << toMap(*a, false);
    return res;
}

QString Apps::defaultName(const QString &mimeType)
{
    scan();
    const QString id = defaultFor(mimeType, readAssociations());
    return id.isEmpty() ? QString() : m_apps.at(m_byId.value(id)).name;
}

bool Apps::open(const QString &path, const QString &mimeType)
{
    scan();
    const QString id = defaultFor(mimeType, readAssociations());
    return !id.isEmpty() && launch(id, {path});
}

bool Apps::launch(const QString &appId, const QStringList &paths)
{
    scan();
    const int i = m_byId.value(appId, -1);
    if (i < 0)
        return false;
    const App &a = m_apps.at(i);
    if (!a.terminal)
        return QProcess::startDetached(QStringLiteral("gio"), QStringList{QStringLiteral("launch"), a.file} + paths);
    // Консольная программа открывается в папке первого файла, как в Thunar
    const QString dir = paths.isEmpty() ? QString() : QFileInfo(paths.first()).absolutePath();
    bool ok = true;
    for (const QStringList &cmd : commandLines(a, paths))
        ok = Terminal::run(cmd, dir) && ok;
    return ok;
}

// Exec по спецификации Desktop Entry: кавычки, %f/%F — пути, %u/%U — URL;
// с %f/%u на каждый файл свой запуск
QList<QStringList> Apps::commandLines(const App &a, const QStringList &paths)
{
    QStringList args;
    QString cur;
    bool quoted = false, started = false;
    for (int i = 0; i < a.exec.size(); ++i) {
        const QChar c = a.exec[i];
        if (quoted) {
            if (c == QLatin1Char('\\') && i + 1 < a.exec.size())
                cur += a.exec[++i];
            else if (c == QLatin1Char('"'))
                quoted = false;
            else
                cur += c;
        } else if (c == QLatin1Char('"')) {
            quoted = started = true;
        } else if (c.isSpace()) {
            if (started)
                args << cur;
            cur.clear();
            started = false;
        } else {
            cur += c;
            started = true;
        }
    }
    if (started)
        args << cur;

    QStringList urls;
    for (const QString &p : paths)
        urls << QUrl::fromLocalFile(p).toString(QUrl::FullyEncoded);

    const bool perFile = std::any_of(args.cbegin(), args.cend(), [](const QString &x) {
        return x == QLatin1String("%f") || x == QLatin1String("%u");
    });
    const int runs = perFile ? std::max<int>(1, int(paths.size())) : 1;
    QList<QStringList> res;
    for (int r = 0; r < runs; ++r) {
        QStringList cmd;
        for (const QString &x : std::as_const(args)) {
            if (x == QLatin1String("%f") || x == QLatin1String("%u")) {
                if (r < paths.size())
                    cmd << (x == QLatin1String("%f") ? paths.at(r) : urls.at(r));
            } else if (x == QLatin1String("%F")) {
                cmd << paths;
            } else if (x == QLatin1String("%U")) {
                cmd << urls;
            } else if (x == QLatin1String("%i")) {
                if (!a.icon.isEmpty())
                    cmd << QStringLiteral("--icon") << a.icon;
            } else {
                QString y = x;
                y.replace(QLatin1String("%c"), a.name).replace(QLatin1String("%k"), a.file);
                // Устаревшие %d %D %n %N %v %m просто выкидываем
                static const QRegularExpression dropped(QStringLiteral("%[dDnNvm]"));
                y.remove(dropped);
                y.replace(QLatin1String("%%"), QLatin1String("%"));
                if (!y.isEmpty())
                    cmd << y;
            }
        }
        res << cmd;
    }
    return res;
}

bool Apps::setDefault(const QString &appId, const QString &mimeType)
{
    if (appId.isEmpty() || mimeType.isEmpty())
        return false;
    QProcess p;
    p.start(QStringLiteral("xdg-mime"), {QStringLiteral("default"), appId, mimeType});
    const bool ok = p.waitForFinished(3000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    if (ok)
        emit defaultsChanged();
    return ok;
}

namespace {
const QString kShelfDesktop = QStringLiteral("io.github.nook_apps.Shelf.desktop");
const QString kDirectory = QStringLiteral("inode/directory");
}

bool Apps::defaultFileManager()
{
    // Список приложений мог измениться (Shelf только что установили) — сканируем заново
    m_scannedAt = 0;
    scan();
    return defaultFor(kDirectory, readAssociations()) == kShelfDesktop;
}

bool Apps::makeDefaultFileManager()
{
    return setDefault(kShelfDesktop, kDirectory) && defaultFileManager();
}
