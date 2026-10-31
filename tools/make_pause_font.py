"""The large PAUSE / PAUZA word of the map screen (gui/layout_amiga.c,
Layout_PauseDraw): the letters A E P S U Z of Noto Sans Bold (SIL Open Font
License 1.1) drawn at about 26 pixels capital height, 4 levels of cover
(0 none .. 3 full) for smooth edges, into port/src/gui/pause_font.h.

  python tools/make_pause_font.py [FONT.ttf]
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'port', 'src', 'gui', 'pause_font.h')
FONT = sys.argv[1] if len(sys.argv) > 1 else r'C:\Windows\Fonts\NotoSansJP-Bold.ttf'
LETTERS = 'AEPSUZ'
SIZE = 36

font = ImageFont.truetype(FONT, SIZE)
top = min(font.getbbox(c)[1] for c in LETTERS)
bottom = max(font.getbbox(c)[3] for c in LETTERS)
height = bottom - top
glyphs = []
for c in LETTERS:
    x0, _, x1, _ = font.getbbox(c)
    w = x1 - x0
    im = Image.new('L', (w, height), 0)
    ImageDraw.Draw(im).text((-x0, -top), c, font=font, fill=255)
    rows = []
    for y in range(height):
        rows.append(''.join(str(min(3, (im.getpixel((x, y)) + 42) // 85)) for x in range(w)))
    glyphs.append((c, w, rows))

with open(OUT, 'w', newline='\n') as f:
    f.write('/** @file src/gui/pause_font.h The letters of the large pause word. */\n\n')
    f.write('/* Made by tools/make_pause_font.py from Noto Sans Bold (SIL Open Font\n')
    f.write(' * License 1.1): each letter a string per row, 0 empty .. 3 full. */\n\n')
    f.write('#define PAUSE_FONT_H %d\n\n' % height)
    f.write('typedef struct { char c; uint8 width; const char *rows[%d]; } PauseGlyph;\n\n' % height)
    f.write('static const PauseGlyph s_pauseGlyphs[] = {\n')
    for c, w, rows in glyphs:
        f.write("\t{ '%s', %d, {\n" % (c, w))
        for r in rows:
            f.write('\t\t"%s",\n' % r)
        f.write('\t} },\n')
    f.write('};\n')
print('%s: height %d, widths %s' % (os.path.normpath(OUT), height, [g[1] for g in glyphs]))
for c, w, rows in glyphs[:2]:
    print('\n'.join(r.replace('0', '.') for r in rows))
