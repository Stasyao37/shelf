"""Запуск: python tools/preview_icons.py docs/icons.png  (нужны cairosvg и Pillow)

Превью стеклянного набора для README: 3 ряда по 9 иконок на тёмной плашке."""
import io
import os
import sys

import cairosvg
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROWS = [
    ["folder", "folder-open", "file", "document", "pdf", "spreadsheet", "presentation", "image", "picture"],
    ["audio", "video", "font", "book", "code", "script", "config", "html", "package"],
    ["disc", "database", "key", "certificate", "executable", "exe", "drive", "usb", "trash-can"],
]
W, H, ICON, STEP, X0, Y0 = 1340, 500, 96, 140, 62, 62
TOP, BOTTOM = (42, 31, 64), (22, 26, 46)

out = Image.new("RGBA", (W, H))
bg = Image.new("RGBA", (W, H))
draw = ImageDraw.Draw(bg)
for y in range(H):
    t = y / (H - 1)
    draw.line([(0, y), (W, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(TOP, BOTTOM)) + (255,))
mask = Image.new("L", (W, H))
ImageDraw.Draw(mask).rounded_rectangle([0, 0, W - 1, H - 1], radius=28, fill=255)
out.paste(bg, (0, 0), mask)

for r, row in enumerate(ROWS):
    for c, name in enumerate(row):
        png = cairosvg.svg2png(url=os.path.join(ROOT, "icons", name + ".svg"), output_width=ICON, output_height=ICON)
        out.alpha_composite(Image.open(io.BytesIO(png)).convert("RGBA"), (X0 + c * STEP, Y0 + r * STEP))

out.save(sys.argv[1])
