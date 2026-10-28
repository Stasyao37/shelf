#!/usr/bin/env python3
"""Проверка переводов: все строки из qsTr()/tr()/translate() есть в словарях i18n/.

  tools/i18n.py            — проверить; код возврата 1, если чего-то не хватает
  tools/i18n.py --missing  — напечатать недостающее заготовкой для словаря

Исходные строки — русские. В en.json нужна каждая; в ru.json — только строки
с числом (qsTr("…%n…", "", n), tr("…", nullptr, n)): у них формы слова
— массив, в ru.json их три (1, 2–4, 5), в en.json две (1, остальные).
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FORMS = {"en": 2, "ru": 3}

# Литерал строки C++/QML с экранированием: "…\"…"
LIT = r'"((?:[^"\\]|\\.)*)"'
CALLS = [
    # qsTr("…") / qsTr("…", "уточнение", n)
    re.compile(r'\bqsTr\(\s*' + LIT + r'(\s*,\s*' + LIT + r')?(\s*,)?'),
    # tr("…") / tr("…", nullptr, n)
    re.compile(r'(?<![\w.])tr\(\s*' + LIT + r'(\s*,\s*(?:nullptr|' + LIT + r'))?(\s*,)?'),
    # QCoreApplication::translate("Контекст", "…", nullptr, n)
    re.compile(r'\btranslate\(\s*"[^"]*"\s*,\s*' + LIT + r'(\s*,\s*(?:nullptr|' + LIT + r'))?(\s*,)?'),
]


def unescape(s):
    return s.encode("utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8") if "\\" in s else s


def strip_comments(text):
    # Строки в комментариях не переводятся: убираем // и /* */ вне литералов
    out, i, n = [], 0, len(text)
    while i < n:
        if text.startswith("//", i):
            i = text.find("\n", i)
            i = n if i < 0 else i
        elif text.startswith("/*", i):
            i = text.find("*/", i) + 2
        elif text[i] in "\"'":
            q, j = text[i], i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def collect():
    """{ключ: (числовая ли, где встретилась)}"""
    found = {}
    files = sorted(ROOT.glob("qml/*.qml")) + sorted(ROOT.glob("src/**/*.cpp")) + sorted(ROOT.glob("src/**/*.h"))
    for f in files:
        text = strip_comments(f.read_text(encoding="utf-8"))
        for rx in CALLS:
            for m in rx.finditer(text):
                src = unescape(m.group(1))
                disamb = m.group(3) if len(m.groups()) >= 3 else None
                numerus = bool(m.groups()[-1]) or "%n" in src
                key = src + "|" + unescape(disamb) if disamb else src
                line = text.count("\n", 0, m.start()) + 1
                prev = found.get(key)
                found[key] = (numerus or (prev and prev[0]), prev[1] if prev else f"{f.relative_to(ROOT)}:{line}")
    return found


def main():
    found = collect()
    dicts = {lang: json.loads((ROOT / "i18n" / f"{lang}.json").read_text(encoding="utf-8")) for lang in FORMS}
    problems, missing = [], {lang: {} for lang in FORMS}
    for key, (numerus, where) in sorted(found.items()):
        for lang, d in dicts.items():
            # Русскому словарю нужны только формы для чисел
            if lang == "ru" and not numerus:
                continue
            v = d.get(key)
            if v is None or v == "" or v == []:
                problems.append(f"{lang}: нет перевода «{key}» ({where})")
                missing[lang][key] = [key] * FORMS[lang] if numerus else key
            elif numerus and (not isinstance(v, list) or len(v) != FORMS[lang]):
                problems.append(f"{lang}: «{key}» — с числом, нужно {FORMS[lang]} формы списком ({where})")
            elif not numerus and isinstance(v, list):
                problems.append(f"{lang}: «{key}» — без числа, а в словаре список форм ({where})")
    for lang, d in dicts.items():
        for key in d:
            if key not in found:
                problems.append(f"{lang}: «{key}» больше нигде не используется")

    if "--missing" in sys.argv:
        print(json.dumps(missing, ensure_ascii=False, indent=2))
        return 0
    for p in problems:
        print(p)
    print(f"строк: {len(found)}, проблем: {len(problems)}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
