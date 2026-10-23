"""Запуск: python tools/gen_icons.py icons  (нужен shapely: pip install shapely)

Генератор стеклянных иконок Shelf: формы собираются в shapely, кромка — кольцо
между контуром и контуром, сдвинутым внутрь на 0.5, как у остального набора."""
import sys
from shapely.geometry import Point, LineString, Polygon, box
from shapely.affinity import translate
from shapely.ops import unary_union

OUT = sys.argv[1]

DEFS = """	<defs>
		<linearGradient id="fill" gradientUnits="userSpaceOnUse" x1="2.5" y1="2.5" x2="20" y2="21.5">
			<stop offset="0" stop-color="#FFFFFF" stop-opacity="0.6"/>
			<stop offset="1" stop-color="#FFFFFF" stop-opacity="0.3"/>
		</linearGradient>
		<linearGradient id="edge" gradientUnits="userSpaceOnUse" x1="2.5" y1="2.5" x2="20" y2="21.5">
			<stop offset="0" stop-color="#FFFFFF" stop-opacity="0.6"/>
			<stop offset="0.4927" stop-color="#FFFFFF" stop-opacity="0"/>
			<stop offset="0.9971" stop-color="#FFFFFF" stop-opacity="0.3"/>
		</linearGradient>
	</defs>"""


def rrect(x, y, w, h, r):
    return box(x + r, y + r, x + w - r, y + h - r).buffer(r, quad_segs=16)


def line(pts, w):
    return LineString(pts).buffer(w / 2, quad_segs=16)


def circle(x, y, r):
    return Point(x, y).buffer(r, quad_segs=24)


def sheet():
    # Лист из file.svg: срезанный угол по линии (14,2)–(20,8)
    keep = Polygon([(-10, -10), (2, -10), (40, 28), (-10, 28)])
    return rrect(4, 2, 16, 20, 3).intersection(keep)


def dogear():
    tri = Polygon([(14, 2), (20, 8), (14, 8)])
    return tri.difference(box(14, 6, 16, 8).difference(circle(16, 6, 2)))


def polys(g):
    return [g] if g.geom_type == "Polygon" else list(getattr(g, "geoms", []))


def path(g):
    out = []
    for p in polys(g):
        for ring in [p.exterior, *p.interiors]:
            c = list(ring.coords)[:-1]
            out.append("M" + " L".join(f"{x:.3f},{y:.3f}" for x, y in c) + " Z")
    return " ".join(out)


def layer(g, fill="fill"):
    edge = g.difference(g.buffer(-0.5, quad_segs=16))
    return (f'\t<path fill="url(#{fill})" fill-rule="evenodd" d="{path(g)}"/>\n'
            f'\t<path fill="url(#edge)" fill-rule="evenodd" d="{path(edge)}"/>\n')


def write(name, comment, layers):
    body = "".join(layer(g) for g in layers)
    svg = (f'<?xml version="1.0" encoding="UTF-8"?>\n<!-- {comment}\n'
           f'     Сгенерировано: стекло белым 0.6→0.3, кромка 0.5; детали — второй слой того же стекла -->\n'
           f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="48px" height="48px">\n'
           f'{DEFS}\n{body}</svg>\n')
    open(f"{OUT}/{name}.svg", "w").write(svg)


def cut(base, obj, gap=0.6):
    """Отдельный предмет перед основой: в основе вырезается зазор вокруг него."""
    return base.difference(obj.buffer(gap, quad_segs=16))


# PDF: лист и трилистник
trefoil = unary_union([
    line([(12, 10.0), (12, 13.4)], 2.9),
    line([(10.7, 16.3), (7.9, 18.0)], 2.8),
    line([(13.3, 16.3), (16.1, 18.0)], 2.8),
    circle(12, 15.4, 1.1),
    line([(12, 13.4), (12, 15.4)], 1.3),
])
write("pdf", "PDF: лист с загнутым углом и трилистник", [sheet(), dogear(), trefoil])

# Таблица: плита, вертикальная и две горизонтальные полосы
write("spreadsheet", "Таблица: плита с перекрестьем полос", [
    rrect(3, 3, 18, 18, 2.5),
    unary_union([line([(8.6, 4.6), (8.6, 19.4)], 2.6),
                 line([(4.6, 9.4), (19.4, 9.4)], 2.6),
                 line([(4.6, 15.2), (19.4, 15.2)], 2.6)]),
])

# Презентация: доска на ножке с кольцом, диаграмма и две строки
pie = circle(8.2, 10.4, 2.9).difference(box(8.2, 0, 24, 10.4))
slice_ = translate(circle(8.2, 10.4, 2.9).intersection(box(8.2, 0, 24, 10.4)), 0.45, -0.45)
board = unary_union([rrect(2.5, 3, 19, 14, 2.5),
                     line([(12, 16.5), (12, 19.4)], 1.3),
                     circle(12, 20.6, 1.5).difference(circle(12, 20.6, 0.7))])
write("presentation", "Презентация: доска на ножке, диаграмма и строки", [
    board,
    unary_union([pie, slice_, line([(13.6, 8.6), (17.6, 8.6)], 2.0),
                 line([(13.6, 12.2), (17.6, 12.2)], 2.0)]),
])

# Пакет: коробка, светлая крышка с прорезью под ленту, ручка
crate = rrect(3.5, 3, 17, 18, 3)
lid = crate.intersection(box(0, 0, 24, 8.6)).difference(box(11, 0, 13, 9))
write("package", "Установочный пакет: коробка с крышкой и ручкой", [
    crate, unary_union([lid, line([(11.1, 11.4), (12.9, 11.4)], 1.8)]),
])

# Скрипт: плита и знак ⌘
cmd = unary_union([box(12.2, 7.2, 16.8, 11.8).difference(box(13.4, 8.4, 15.6, 10.6))]
                  + [circle(x, y, 1.45) for x in (11.9, 17.1) for y in (6.9, 12.1)])
write("script", "Скрипт: плита со знаком ⌘", [rrect(3, 3, 18, 18, 3), cmd])

# Конфиг: лист и гаечный ключ перед ним
head = circle(19.2, 17.3, 2.5).difference(line([(19.9, 16.6), (23, 13.5)], 1.9))
wrench = unary_union([head, line([(18.2, 18.3), (15.2, 21.3)], 2.2)])
write("config", "Конфиг: лист и гаечный ключ", [cut(sheet(), wrench), cut(dogear(), wrench), wrench])

# Сертификат: плита со строками и печать с лентами
seal = unary_union([circle(17.6, 16.6, 3.2),
                    Polygon([(15.6, 18.5), (19.6, 18.5), (19.6, 22.6), (17.6, 21.3), (15.6, 22.6)])])
write("certificate", "Сертификат: плита со строками и печать", [
    cut(rrect(2.5, 2.5, 16, 16, 3), seal),
    cut(unary_union([line([(6.6, 7), (14.4, 7)], 2.0), line([(6.6, 10.6), (14.4, 10.6)], 2.0),
                     line([(6.6, 14.2), (12.4, 14.2)], 2.0)]), seal),
    seal,
])

# Ключ: головка с отверстием, стержень и две дорожки с контактами
key = unary_union([
    circle(8.5, 7.2, 5.2),
    line([(8.5, 9), (8.5, 19.6)], 5.0),
    line([(10.5, 12.4), (15.6, 12.4), (17.9, 10.1)], 1.3), circle(19.4, 8.6, 1.7),
    line([(10.5, 16.6), (15.6, 16.6), (17.9, 18.9)], 1.3), circle(19.4, 20.4, 1.7),
]).difference(circle(8.5, 6.6, 2.0))  # отверстие — после объединения, иначе его закроет стержень
write("key", "Ключ: головка с отверстием и дорожки к контактам", [key])

# HTML: окно браузера — светлая шапка, картинка и строки
win = rrect(3, 3, 18, 18, 3)
write("html", "HTML: окно со шапкой, картинкой и строками", [
    win, unary_union([win.intersection(box(0, 0, 24, 7)), rrect(6, 10.5, 5.6, 5.6, 1.2),
                      line([(14.6, 11.8), (17.8, 11.8)], 1.8), line([(14.6, 15.0), (17.8, 15.0)], 1.8)]),
])


def bezier(p0, p1, p2, p3, n=24):
    return [tuple((1 - t) ** 3 * a + 3 * (1 - t) ** 2 * t * b + 3 * (1 - t) * t ** 2 * c + t ** 3 * d
                  for a, b, c, d in zip(p0, p1, p2, p3)) for t in (i / n for i in range(n + 1))]


# Видео: плёнка с перфорацией и кнопкой воспроизведения
play = Polygon([(9.0, 8.9), (9.0, 15.1), (14.6, 12)]).buffer(0.7, quad_segs=16)
write("video", "Видео: кадр плёнки с перфорацией и кнопкой воспроизведения", [
    rrect(2.5, 5, 19, 14, 3),
    unary_union([play] + [circle(x, y, 1.05) for x in (5.6, 18.4) for y in (8.6, 12, 15.4)]),
])

# Шрифт: рукописная F — дуга сверху, наклонный стержень с хвостом, перекладина
f_top = bezier((5.3, 9.6), (5.0, 5.2), (7.4, 4.3), (11.6, 4.3)) + [(19.6, 4.3)]
f_stem = bezier((12.9, 4.3), (11.7, 10), (10.9, 14.6), (9.7, 18.0)) + bezier((9.7, 18.0), (9.1, 19.9), (7.5, 20.4), (5.0, 20.2))[1:]
write("font", "Шрифт: рукописная F", [unary_union([
    LineString(f_top).buffer(1.4, quad_segs=16), LineString(f_stem).buffer(1.4, quad_segs=16),
    line([(9.0, 12.4), (15.8, 12.4)], 2.8)])])

# Книга: раскрытые страницы на обложке с язычком под корешком
# Страницы прогибаются к корешку: сверху и снизу — плавная ложбинка
pages = unary_union([rrect(5.0, 3, 7.4, 15.6, 2.4), rrect(11.6, 3, 7.4, 15.6, 2.4)])
pages = pages.difference(circle(12, 1.6, 1.9)).difference(circle(12, 20.0, 1.9))
cover = unary_union([rrect(2.5, 6, 19, 14, 2.5), rrect(9.4, 18.6, 5.2, 2.9, 1.3)])
write("book", "Книга: раскрытые страницы на обложке", [cut(cover, pages, 0.4), pages])

# Код: лист и </>
write("code", "Код: лист с </>", [sheet(), dogear(), unary_union([
    LineString([(10.1, 12.4), (7.5, 15.1), (10.1, 17.8)]).buffer(0.85, quad_segs=16),
    LineString([(13.9, 12.4), (16.5, 15.1), (13.9, 17.8)]).buffer(0.85, quad_segs=16),
    line([(12.9, 11.6), (11.1, 18.6)], 1.5)])])

# Исполняемый файл: шестерёнка
from shapely.affinity import rotate
tooth = rrect(10.6, 2.6, 2.8, 4, 0.9)
gear = unary_union([circle(12, 12, 6.6)] + [rotate(tooth, a, origin=(12, 12)) for a in range(0, 360, 45)])
write("executable", "Исполняемый файл: шестерёнка", [gear.difference(circle(12, 12, 2.6))])

# Программа Windows: окно с плитками 2×2
app = rrect(3, 3, 18, 18, 3)
write("exe", "Программа Windows: окно с плитками 2×2", [app, unary_union(
    [rrect(x, y, 5.4, 5.4, 1.3) for x in (6.3, 12.3) for y in (6.3, 12.3)])])


def write_flat(name, comment, g):
    """Плоский белый силуэт — эмблема или логотип, стекло накладывает GlassEmblem."""
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="48" height="48" fill="#fff">\n'
           f'<!-- {comment} -->\n<path fill-rule="evenodd" d="{path(g)}"/>\n</svg>\n')
    open(f"{OUT}/{name}.svg", "w").write(svg)


# Java: чашка с паром на блюдце (официального логотипа в Simple Icons нет)
cup = unary_union([
    Polygon([(4.6, 11.4), (16.4, 11.4), (15.4, 18.0), (5.6, 18.0)]).buffer(0.9, quad_segs=16),
    circle(17.4, 13.9, 2.6).difference(circle(17.4, 13.9, 1.2)),
])
saucer = line([(3.0, 21.0), (18.0, 21.0)], 1.6)
steam = unary_union([LineString(bezier(*p)).buffer(0.75, quad_segs=16) for p in (
    ((7.8, 9.0), (6.4, 7.4), (9.2, 5.6), (7.8, 2.6)),
    ((11.0, 9.0), (9.6, 7.0), (12.6, 5.0), (11.0, 1.6)),
    ((14.2, 9.0), (12.8, 7.4), (15.6, 5.6), (14.2, 2.6)),
)])
import os
os.makedirs(f"{OUT}/lang", exist_ok=True)
write_flat("lang/java", "Java: чашка с паром на блюдце", unary_union([cup, saucer, steam]))


# Корзина: бак как у trash-can.svg (icons8). Полная — без крышки, над краем
# торчат лист и скомканная бумага; они за стеклом бака, поэтому рисуются первыми
def can_body():
    raw = Polygon([(4.1, 6.8), (19.9, 6.8), (18.0, 22), (6.0, 22)])
    return raw.buffer(-2.2, quad_segs=16).buffer(2.2, quad_segs=16)


papers = unary_union([
    # Веером и повыше: на 22 px в боковой панели иначе читается как крышка
    rotate(rrect(5.0, 0.8, 5.4, 8.4, 0.9), -24, origin=(8.0, 8)),
    rotate(rrect(13.2, 1.6, 5.2, 7.6, 0.9), 22, origin=(15.6, 8)),
    circle(12, 5.6, 2.3),
])
write("trash-full", "Корзина с мусором: над краем бака торчат бумаги", [papers, can_body()])


# Значки в углу иконки: плоские силуэты, кружок-подложку рисует Badge.qml
os.makedirs(f"{OUT}/badges", exist_ok=True)
arrow_stem = LineString(bezier((5.5, 19.0), (5.5, 12.0), (9.0, 8.6), (14.6, 8.6))).buffer(1.6, quad_segs=16)
arrow_head = Polygon([(13.2, 3.4), (20.4, 8.6), (13.2, 13.8)]).buffer(-0.6).buffer(0.9, quad_segs=16)
write_flat("badges/link", "Ссылка: изогнутая стрелка", unary_union([arrow_stem, arrow_head]))

shackle = circle(12, 9.6, 5.0).difference(circle(12, 9.6, 2.9)).intersection(box(0, 0, 24, 11))
shackle = unary_union([shackle, box(7.0, 9.6, 9.1, 12), box(14.9, 9.6, 17.0, 12)])
lock_body = rrect(4.8, 11, 14.4, 10.4, 2.4).difference(
    unary_union([circle(12, 15.2, 1.6), rrect(11.3, 15.2, 1.4, 3.6, 0.7)]))
write_flat("badges/lock", "Нет доступа или только чтение: замок", unary_union([shackle, lock_body]))


# Эмблемы особых папок: плоские силуэты, стекло накладывает GlassEmblem
os.makedirs(f"{OUT}/emblems", exist_ok=True)
git = unary_union([
    circle(7.5, 5.0, 2.6), circle(7.5, 19.0, 2.6), circle(16.5, 7.5, 2.6),
    line([(7.5, 5.0), (7.5, 19.0)], 2.2),
    LineString(bezier((16.5, 7.5), (16.5, 13.0), (7.5, 11.5), (7.5, 16.5))).buffer(1.1, quad_segs=16),
])
write_flat("emblems/folder-git", "Репозиторий git: ветка", git)
write_flat("emblems/folder-projects", "Проекты: </>", unary_union([
    LineString([(8.2, 6.0), (2.8, 12.0), (8.2, 18.0)]).buffer(1.4, quad_segs=16),
    LineString([(15.8, 6.0), (21.2, 12.0), (15.8, 18.0)]).buffer(1.4, quad_segs=16),
    line([(13.8, 4.4), (10.2, 19.6)], 2.6)]))
cog_tooth = rrect(10.2, 1.2, 3.6, 5.0, 1.0)
cog = unary_union([circle(12, 12, 8.0)] + [rotate(cog_tooth, a, origin=(12, 12)) for a in range(0, 360, 45)])
write_flat("emblems/folder-config", "Настройки: шестерёнка", cog.difference(circle(12, 12, 3.4)))
write_flat("badges/play", "Видео поверх миниатюры: треугольник воспроизведения",
           Polygon([(8.0, 5.0), (8.0, 19.0), (19.5, 12.0)]).buffer(-0.8).buffer(1.6, quad_segs=16))


# Значок самого Shelf: три книги на полке. Детали — второй слой того же стекла:
# полосы на корешках и ярлык, опоры полки за доской
def book(x, y, w, h, bands, label=None):
    """Корешок со скруглённым верхом; bands — высоты полос, label — (y, h) ярлыка.
    Ярлык — отдельным слоем в конце списка: его рисуют стеклом поплотнее."""
    body = rrect(x, y, w, h, w * 0.42)
    details = unary_union([line([(x - 1, b), (x + w + 1, b)], 0.7).intersection(body) for b in bands])
    if not label:
        return [cut(body, details, 0.35), details]
    ly, lh = label
    tag = rrect(x + w * 0.16, ly, w * 0.68, lh, lh / 2)
    return [cut(cut(body, details, 0.35), tag, 0.4), details, (tag, "bright")]


plank = rrect(2.4, 17.0, 19.2, 2.3, 1.15)
feet = cut(unary_union([rrect(4.4, 18.0, 2.6, 3.6, 1.3), rrect(17.0, 18.0, 2.6, 3.6, 1.3)]), plank)
tall = book(4.2, 3.0, 4.8, 13.4, [5.4], label=(11.2, 2.4))
thin = book(10.0, 5.6, 3.2, 10.8, [7.8, 14.2])
# Третья прислонена: наклон вправо вокруг нижнего угла, низом стоит на доске
lean_body, lean_detail = book(14.0, 6.4, 3.8, 10.6, [8.8, 14.6])
lean = [rotate(g, 14, origin=(14.0, 17.0)) for g in (lean_body, lean_detail)]
lean = [translate(g, 0.3, 16.4 - lean[0].bounds[3]) for g in lean]
shelf_glyph = [feet, plank] + tall + thin + lean
write("shelf", "Shelf: три книги на полке, одна прислонена",
      [l[0] if isinstance(l, tuple) else l for l in shelf_glyph])


def write_app(path_out, layers):
    """Иконка приложения: те же стеклянные слои на мягкой синей плитке —
    без подложки белое стекло пропадает на светлом фоне лаунчера."""
    body = "".join(layer(*l) if isinstance(l, tuple) else layer(l) for l in layers)
    svg = (f'<?xml version="1.0" encoding="UTF-8"?>\n<!-- Иконка приложения Shelf: стеклянная полка на синей плитке\n'
           f'     Сгенерировано tools/gen_icons.py -->\n'
           f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="128" height="128">\n'
           f'{DEFS}\n'
           '\t<defs>\n'
           '\t\t<linearGradient id="tile" x1="0" y1="0" x2="0" y2="1">\n'
           '\t\t\t<stop offset="0" stop-color="#3B52B6"/>\n'
           '\t\t\t<stop offset="1" stop-color="#1B2468"/>\n'
           '\t\t</linearGradient>\n'
           '\t\t<linearGradient id="rim" x1="0" y1="0" x2="0" y2="1">\n'
           '\t\t\t<stop offset="0" stop-color="#FFFFFF" stop-opacity="0.28"/>\n'
           '\t\t\t<stop offset="1" stop-color="#FFFFFF" stop-opacity="0.06"/>\n'
           '\t\t</linearGradient>\n'
           '\t\t<linearGradient id="bright" gradientUnits="userSpaceOnUse" x1="2.5" y1="2.5" x2="20" y2="21.5">\n'
           '\t\t\t<stop offset="0" stop-color="#FFFFFF" stop-opacity="0.92"/>\n'
           '\t\t\t<stop offset="1" stop-color="#FFFFFF" stop-opacity="0.7"/>\n'
           '\t\t</linearGradient>\n'
           '\t</defs>\n'
           '\t<rect x="0.5" y="0.5" width="23" height="23" rx="5.4" fill="url(#tile)"/>\n'
           '\t<rect x="0.75" y="0.75" width="22.5" height="22.5" rx="5.15" fill="none" stroke="url(#rim)" stroke-width="0.5"/>\n'
           f'\t<g transform="translate(2.4 2.2) scale(0.8)">\n{body}\t</g>\n</svg>\n')
    open(path_out, "w").write(svg)


write_app(f"{OUT}/../data/org.stasyao.shelf.svg", shelf_glyph)
