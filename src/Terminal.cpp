#include "Terminal.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace {
struct Known {
    const char *exe;
    QStringList (*cwd)(const QString &dir);   // аргументы «открыть в папке»
    QStringList (*exec)(const QStringList &cmd);
};

// Порядок — порядок выбора, когда ничего не настроено
const Known kKnown[] = {
    {"kitty", [](const QString &d) { return QStringList{QStringLiteral("--directory"), d}; },
     [](const QStringList &c) { return QStringList{QStringLiteral("--")} + c; }},
    {"foot", [](const QString &d) { return QStringList{QStringLiteral("--working-directory"), d}; },
     [](const QStringList &c) { return c; }},
    {"alacritty", [](const QString &d) { return QStringList{QStringLiteral("--working-directory"), d}; },
     [](const QStringList &c) { return QStringList{QStringLiteral("-e")} + c; }},
    {"wezterm", [](const QString &d) { return QStringList{QStringLiteral("start"), QStringLiteral("--cwd"), d}; },
     [](const QStringList &c) { return QStringList{QStringLiteral("start"), QStringLiteral("--")} + c; }},
    {"ghostty", [](const QString &d) { return QStringList{QStringLiteral("--working-directory=") + d}; },
     [](const QStringList &c) { return QStringList{QStringLiteral("-e")} + c; }},
    {"konsole", [](const QString &d) { return QStringList{QStringLiteral("--workdir"), d}; },
     [](const QStringList &c) { return QStringList{QStringLiteral("-e")} + c; }},
    {"gnome-terminal", [](const QString &d) { return QStringList{QStringLiteral("--working-directory=") + d}; },
     [](const QStringList &c) { return QStringList{QStringLiteral("--")} + c; }},
    // Сам выбирает терминал по xdg-terminals.list; папку берёт из рабочего каталога
    {"xdg-terminal-exec", [](const QString &) { return QStringList{}; },
     [](const QStringList &c) { return c; }},
};

const Known *known(const QString &exe)
{
    const QString name = QFileInfo(exe).fileName();
    for (const Known &k : kKnown)
        if (name == QLatin1String(k.exe))
            return &k;
    return nullptr;
}

bool installed(const QString &exe)
{
    return !QStandardPaths::findExecutable(exe).isEmpty() || QFileInfo(exe).isExecutable();
}

// $TERMINAL бывает с аргументами («kitty -1») — берём первое слово
QString fromEnv()
{
    return QProcess::splitCommand(qEnvironmentVariable("TERMINAL")).value(0);
}
}

Terminal::Terminal(QObject *parent)
    : QObject(parent)
{
}

QString Terminal::current()
{
    const QString chosen = QSettings().value(QStringLiteral("apps/terminal")).toString();
    if (!chosen.isEmpty() && installed(chosen))
        return chosen;
    const QString env = fromEnv();
    if (!env.isEmpty() && installed(env))
        return env;
    if (installed(QStringLiteral("xdg-terminal-exec")))
        return QStringLiteral("xdg-terminal-exec");
    for (const Known &k : kKnown)
        if (installed(QLatin1String(k.exe)))
            return QLatin1String(k.exe);
    return {};
}

QStringList Terminal::available()
{
    QStringList res;
    for (const Known &k : kKnown)
        if (installed(QLatin1String(k.exe)))
            res << QLatin1String(k.exe);
    return res;
}

bool Terminal::openIn(const QString &dir)
{
    const QString exe = current();
    if (exe.isEmpty())
        return false;
    const Known *k = known(exe);
    // Незнакомому терминалу папку передаём рабочим каталогом — это понимают все
    return QProcess::startDetached(exe, k ? k->cwd(dir) : QStringList{}, dir);
}

bool Terminal::run(const QStringList &command, const QString &dir)
{
    const QString exe = current();
    if (exe.isEmpty() || command.isEmpty())
        return false;
    const Known *k = known(exe);
    // Для незнакомого — общепринятое «-e команда»
    const QStringList args = k ? k->exec(command) : QStringList{QStringLiteral("-e")} + command;
    return QProcess::startDetached(exe, args, dir.isEmpty() ? QDir::homePath() : dir);
}
