#include "Language.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>
#include <QTranslator>

namespace {
// Поддерживаемые языки и их названия на них самих. Новый язык — i18n/<код>.json
// в CMakeLists.txt и строка здесь
struct Lang {
    const char *code;
    const char *name;
};
constexpr Lang kLanguages[] = {{"ru", "Русский"}, {"en", "English"}};

// Номер формы для %n: в словаре у таких строк массив форм в этом порядке
int pluralForm(const QString &lang, int n)
{
    if (lang == QLatin1String("ru")) {
        const int d = n % 10, h = n % 100;
        return d == 1 && h != 11 ? 0 : d >= 2 && d <= 4 && (h < 12 || h > 14) ? 1 : 2;
    }
    return n == 1 ? 0 : 1;
}

// Словарь i18n/<код>.json: { "исходная строка": "перевод" | ["форма", ...] }.
// Контекст не нужен — одинаковые строки переводятся одинаково; если всё же нужно
// разное — ключ «строка|уточнение» (disambiguation в qsTr)
class JsonTranslator : public QTranslator
{
public:
    bool load(const QString &lang)
    {
        m_lang = lang;
        QFile f(QStringLiteral(":/i18n/%1.json").arg(lang));
        if (!f.open(QIODevice::ReadOnly))
            return false;
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QStringList forms;
            if (it->isArray())
                for (const QJsonValue &v : it->toArray())
                    forms << v.toString();
            else
                forms << it->toString();
            m_map.insert(it.key(), forms);
        }
        return true;
    }

    QString translate(const char *, const char *sourceText, const char *disambiguation, int n) const override
    {
        const QString src = QString::fromUtf8(sourceText);
        auto it = m_map.constEnd();
        if (disambiguation && *disambiguation)
            it = m_map.constFind(src + QLatin1Char('|') + QString::fromUtf8(disambiguation));
        if (it == m_map.constEnd())
            it = m_map.constFind(src);
        if (it == m_map.constEnd() || it->isEmpty())
            return {};
        // %n заменит сам Qt
        return n >= 0 ? it->value(qMin(pluralForm(m_lang, n), int(it->size()) - 1)) : it->first();
    }

    bool isEmpty() const override { return m_map.isEmpty(); }

private:
    QString m_lang;
    QHash<QString, QStringList> m_map;
};

JsonTranslator *s_translator = nullptr;
QString s_current;

bool supported(const QString &code)
{
    for (const Lang &l : kLanguages)
        if (code == QLatin1String(l.code))
            return true;
    return false;
}

QString savedChoice()
{
    return QSettings().value(QStringLiteral("view/language")).toString();
}
}

Language::Language(QObject *parent)
    : QObject(parent)
{
    connect(notifier(), &LanguageNotifier::changed, this, &Language::changed);
}

QString Language::choice() const
{
    const QString c = savedChoice();
    return supported(c) ? c : QString();
}

void Language::setChoice(const QString &code)
{
    const QString c = supported(code) ? code : QString();
    if (c == choice())
        return;
    QSettings().setValue(QStringLiteral("view/language"), c);
    apply(c);
    // Все qsTr в QML — заново, без перезапуска
    if (QQmlEngine *engine = qmlEngine(this))
        engine->retranslate();
}

QString Language::current()
{
    return s_current.isEmpty() ? systemLanguage() : s_current;
}

QVariantList Language::options() const
{
    QVariantList list;
    for (const Lang &l : kLanguages)
        list << QVariantMap{{QStringLiteral("code"), QString::fromLatin1(l.code)},
                            {QStringLiteral("name"), QString::fromUtf8(l.name)}};
    return list;
}

void Language::init()
{
    const QString c = savedChoice();
    apply(supported(c) ? c : QString());
}

void Language::apply(const QString &choice)
{
    const QString lang = supported(choice) ? choice : systemLanguage();
    if (s_translator) {
        QCoreApplication::removeTranslator(s_translator);
        delete s_translator;
        s_translator = nullptr;
    }
    // У русского свой словарь тоже есть: в нём формы слов при числах
    auto *t = new JsonTranslator;
    if (t->load(lang) && QCoreApplication::installTranslator(t))
        s_translator = t;
    else
        delete t;
    // Даты, размеры и названия типов файлов (QMimeType::comment) — на том же языке
    QLocale::setDefault(QLocale(lang));
    const bool changedLang = s_current != lang;
    s_current = lang;
    if (changedLang)
        emit notifier()->changed();
}

QString Language::systemLanguage()
{
    for (const QString &ui : QLocale::system().uiLanguages()) {
        const QString code = ui.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0).toLower();
        if (supported(code))
            return code;
    }
    return QStringLiteral("en");
}

LanguageNotifier *Language::notifier()
{
    static LanguageNotifier n;
    return &n;
}
