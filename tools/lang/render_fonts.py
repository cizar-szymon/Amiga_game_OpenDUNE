"""Pictures of the Polish letters in the game's fonts, as the game draws them
(port/src/gui/font_polish.c on the player's NEW6P.FNT and NEW8P.FNT).

  python tools/lang/render_fonts.py DATADIR OUTDIR ["more lines" ...]

Writes OUTDIR/polish_glyphs.png (each letter large, next to its plain letter,
the font's colours 1 face, 2 shadow, 3 light edge) and
OUTDIR/polish_text.png (sample lines as in the game, style 0x22: the face
and the shadow, enlarged 3 times). Needs Pillow.
"""
import os
import re
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dune_lang import DataDir, Font  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GLYPHS_C = os.path.join(ROOT, 'port', 'src', 'gui', 'font_polish.c')


def parse_glyphs():
    """{'6': [(code, base, rows)], '8': [...]} from font_polish.c."""
    src = open(GLYPHS_C, encoding='latin-1').read()
    out = {}
    for table, key in (('s_glyphs6', 6), ('s_glyphs8', 8)):
        body = src[src.index(table + '[] = {'):]
        body = body[:body.index('\n};')]
        glyphs = []
        for m in re.finditer(r"\{ 0x([0-9A-F]{2}), '(.)', \{[^\n]*\n((?:\s*\"[.#]*\",\n)+)", body):
            rows = re.findall(r'"([.#]*)"', m.group(3))
            glyphs.append((int(m.group(1), 16), m.group(2), rows))
        out[key] = glyphs
    pairs = re.findall(r'\{ 0x([0-9A-F]{2}), 0x([0-9A-F]{2}) \}', src[src.index('s_smallToCapital'):])
    out['small'] = [(int(a, 16), int(b, 16)) for a, b in pairs]
    return out


def emboss(rows):
    """Font_Emboss of font_polish.c."""
    h, w = len(rows), len(rows[0])

    def face(x, y):
        return 0 <= x < w and 0 <= y < h and rows[y][x] == '#'
    out = []
    for y in range(h):
        r = []
        for x in range(w):
            if face(x, y):
                v = 1
            elif face(x - 1, y) or face(x, y - 1) or face(x - 1, y - 1):
                v = 2
            elif face(x + 1, y) or face(x, y + 1) or face(x + 1, y + 1) or face(x - 1, y + 1) or face(x + 1, y - 1):
                v = 3
            else:
                v = 0
            r.append(v)
        out.append(r)
    return out


def add_polish(font, glyphs, height):
    """The font's char table with the Polish letters (Font_AddPolish)."""
    table = {c: font.glyph_rows(c) for c in range(font.count)}
    key = 6 if height == 8 else 8
    for code, base, rows in glyphs[key]:
        table[code] = emboss(rows)
    if height == 8:
        for small, capital in glyphs['small']:
            table[small] = table[capital]
    return table


SHEET = {0: (40, 40, 48), 1: (250, 230, 150), 2: (20, 20, 20), 3: (150, 140, 120)}


def glyph_sheet(fonts, glyphs, path, scale=6):
    cells = []
    for name, font in fonts:
        table = add_polish(font, glyphs, font.height)
        key = 6 if font.height == 8 else 8
        row = []
        for code, base, _ in glyphs[key]:
            row.append((table[ord(base)], table[code]))
        if font.height == 8:
            for small, capital in glyphs['small']:
                pass
        cells.append((name, font.height, row))
    width = max(sum((len(a[0]) + len(b[0]) + 3) for a, b in row) for _, _, row in cells) * scale + 20
    height = sum((h + 4) * scale for _, h, _ in cells) + 20 * len(cells) + 10
    img = Image.new('RGB', (width, height), (90, 90, 100))
    d = ImageDraw.Draw(img)
    y0 = 5
    for name, h, row in cells:
        d.text((10, y0), name + ': the plain letter, then the Polish one (1 face, 2 shadow, 3 light edge)', fill=(255, 255, 255))
        y0 += 15
        x0 = 10
        for plain, pol in row:
            for g in (plain, pol):
                for y, r in enumerate(g):
                    for x, v in enumerate(r):
                        d.rectangle([x0 + x * scale, y0 + y * scale, x0 + (x + 1) * scale - 1, y0 + (y + 1) * scale - 1], fill=SHEET[v])
                x0 += (len(g[0]) + 1) * scale
            x0 += scale
        y0 += (h + 4) * scale
    img.save(path)


def text_image(lines, fonts, glyphs, path, scale=3):
    """Lines as GUI_DrawText_Wrapper draws them with style 0x22: colour 1 the
    text, 2 a dark shadow, 3 not drawn, each char one pixel narrower."""
    bg, fg, shadow = (164, 112, 56), (252, 252, 200), (24, 16, 8)
    rows = []
    for name, font in fonts:
        table = add_polish(font, glyphs, font.height)
        for line in lines:
            rows.append((name, font.height, table, line.encode('iso8859_2')))
    width = 0
    for _, h, table, b in rows:
        width = max(width, sum(max(len(table.get(c, [[0]])[0]) - 1, 0) for c in b))
    img = Image.new('RGB', ((width + 70) * scale, sum((h + 3) * scale for _, h, _, _ in rows) + 10), bg)
    d = ImageDraw.Draw(img)
    y0 = 5
    for name, h, table, b in rows:
        d.text((4, y0 + 2), name, fill=(0, 0, 0))
        x0 = 64
        for c in b:
            g = table.get(c)
            if g is None or len(g) == 0 or len(g[0]) == 0:
                continue
            for y, r in enumerate(g):
                for x, v in enumerate(r):
                    if v in (1, 2):
                        col = fg if v == 1 else shadow
                        d.rectangle([(x0 + x) * scale, y0 + y * scale, (x0 + x + 1) * scale - 1, y0 + (y + 1) * scale - 1], fill=col)
            x0 += len(g[0]) - 1
        y0 += (h + 3) * scale
    img.save(path)


def main():
    data, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    dd = DataDir(data)
    glyphs = parse_glyphs()
    fonts = [(n, Font(dd.get(n + '.FNT'))) for n in ('NEW6P', 'NEW8P')]
    glyph_sheet(fonts, glyphs, os.path.join(out, 'polish_glyphs.png'))
    lines = [
        'Zażółć gęślą jaźń.  ZAŻÓŁĆ GĘŚLĄ JAŹŃ.',
        'ąćęłńóśźż  ĄĆĘŁŃÓŚŹŻ  Łódź, Świętokrzyskie',
        'Wróg zniszczył twoją rafinerię przyprawy.',
    ] + sys.argv[3:]
    text_image(lines, fonts, glyphs, os.path.join(out, 'polish_text.png'))
    print('written', os.path.join(out, 'polish_glyphs.png'), os.path.join(out, 'polish_text.png'))


if __name__ == '__main__':
    main()
