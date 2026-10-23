"""Запуск: python tools/subset_icons.py <JetBrainsMonoNerdFont-Regular.ttf> <JetBrainsMonoNerdFontPropo-Regular.ttf>
(нужен fontTools: pip install fonttools)

Значки интерфейса — символы Nerd Font (\\u{f…} в qml/). Чтобы Shelf не зависел от
установленного шрифта, из JetBrainsMono Nerd Font вырезаются только эти символы и
вшиваются в программу под своими именами. Берём тот же шрифт, что раньше стоял в
Theme.qml: метрики и контуры те же, значки не сдвигаются ни на пиксель.
Обычный вариант — для стекла, Propo — для Material."""
import glob
import os
import re
import sys

from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "fonts")


def used_codepoints():
    cps = set()
    for f in glob.glob(os.path.join(ROOT, "qml", "*.qml")):
        for m in re.finditer(r"\\u\{([0-9a-fA-F]+)\}", open(f, encoding="utf-8").read()):
            cps.add(int(m.group(1), 16))
    return sorted(cps)


def rename(font, family):
    # 1/16 — семейство, 4 — полное имя, 6 — PostScript
    ps = family.replace(" ", "")
    for rec in font["name"].names:
        if rec.nameID in (1, 16):
            rec.string = family
        elif rec.nameID == 4:
            rec.string = family + " Regular"
        elif rec.nameID == 6:
            rec.string = ps + "-Regular"
        elif rec.nameID == 3:
            rec.string = ps + ";subset"


def build(src, family, out, cps):
    font = TTFont(src)
    opts = subset.Options()
    opts.layout_features = []
    opts.name_IDs = ["*"]
    opts.name_languages = ["*"]
    opts.notdef_outline = True
    # Хинтинг оставляем: без него значки растеризуются иначе, чем раньше
    opts.hinting = True
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=cps)
    sub.subset(font)
    rename(font, family)
    font.save(out)
    missing = [c for c in cps if c not in font.getBestCmap()]
    print(f"{out}: {len(cps) - len(missing)} символов, {os.path.getsize(out)} байт")
    if missing:
        sys.exit("нет в шрифте: " + " ".join(f"{c:x}" for c in missing))


cps = used_codepoints()
os.makedirs(OUT, exist_ok=True)
build(sys.argv[1], "Shelf Icons", os.path.join(OUT, "ShelfIcons.ttf"), cps)
build(sys.argv[2], "Shelf Icons Propo", os.path.join(OUT, "ShelfIconsPropo.ttf"), cps)
