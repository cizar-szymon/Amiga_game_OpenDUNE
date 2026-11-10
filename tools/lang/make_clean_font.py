#!/usr/bin/env python3
"""The clean font of the port (opendune.ini font=clean): port/src/gui/font_clean.h
from a 6x12 bitmap font in BDF, the letters the game's 8 pixel font is used for.

  python tools/lang/make_clean_font.py ter-u12n.bdf > port/src/gui/font_clean.h

The source in use is 6x10.bdf of the X11 "fixed" fonts (public domain, from
Markus Kuhn's ucs-fonts): a capital letter of 7 rows on the same line of
writing as the game's NEW8P.FNT, two rows under it, one above for accents - it
fits the game's buttons, lists and message line as it is.

A 6x12 source (ter-u12n.bdf of Terminus Font, SIL Open Font License) is fitted
into 10 rows as described below; tried first (10.11.2026) and given up: its
capital letters are 8 rows, one more than the game's windows are made for.

Codes: ASCII 32..126; the German letters in the code page of the game's fonts
(CP437); the Polish letters in ISO-8859-2 (as port/src/gui/font_polish.c),
used only when the game's language is Polish.

The cell of the source is 12 rows: two for accents, eight for a capital
letter, two under the line of writing. The game's 8 pixel font is used in
lists with a line every 10 rows (each line paints its background) and in
buttons of 13 rows, so a letter here is 10 rows: the two accent rows go. An
accent of a small letter or a sign up there (i j " ' ^ ` ~) moves into the
row below it, which is empty. A capital letter with an accent gets the accent
in its own top row and is one row lower itself (one of two equal rows of its
stem is left out). The rows under the line stay two: joined into one, g and
y read as q and u.
"""
import sys

ASCII = [(c, chr(c)) for c in range(32, 127)]
GERMAN = [(0x84, 'ä'), (0x8E, 'Ä'), (0x94, 'ö'), (0x99, 'Ö'), (0x81, 'ü'), (0x9A, 'Ü'), (0xE1, 'ß')]
POLISH = [(0xB1, 'ą'), (0xA1, 'Ą'), (0xE6, 'ć'), (0xC6, 'Ć'), (0xEA, 'ę'), (0xCA, 'Ę'), (0xB3, 'ł'), (0xA3, 'Ł'),
          (0xF1, 'ń'), (0xD1, 'Ń'), (0xF3, 'ó'), (0xD3, 'Ó'), (0xB6, 'ś'), (0xA6, 'Ś'), (0xBC, 'ź'), (0xAC, 'Ź'),
          (0xBF, 'ż'), (0xAF, 'Ż')]
W, H = 6, 12


def bdf(path):
    glyphs = {}
    box = (W, H, 0, -2)
    code = bbx = rows = None
    for line in open(path, encoding='latin-1'):
        w = line.split()
        if not w:
            continue
        if w[0] == 'FONTBOUNDINGBOX':
            box = tuple(int(v) for v in w[1:5])
        elif w[0] == 'ENCODING':
            code = int(w[1])
        elif w[0] == 'BBX':
            bbx = tuple(int(v) for v in w[1:5])
        elif w[0] == 'BITMAP':
            rows = []
        elif w[0] == 'ENDCHAR':
            glyphs[code] = (bbx, rows)
            rows = None
        elif rows is not None:
            rows.append(int(w[0], 16) << (32 - len(w[0]) * 4))
    return glyphs, box


def cell(glyphs, box, ch):
    """The 12 rows of the letter, 6 bits each (bit 5 the left pixel)."""
    (bw, bh, bx, by), rows = glyphs[ord(ch)]
    fw, fh, fx, fy = box
    top = fh + fy - (bh + by)
    out = [0] * 12
    for r, bits in enumerate(rows):
        y = top + r
        if not 0 <= y < 12:
            continue
        for c in range(bw):
            x = bx + c
            if bits & (1 << (31 - c)) and 0 <= x < W:
                out[y] |= 1 << (W - 1 - x)
    if fh == 10:
        return out[:10]     # a 6x10 font: the game's own proportions, as it is
    accent = out[0] | out[1]
    if accent:
        if out[2] and out[4]:
            # a capital letter under an accent: one row lower, the accent on top
            body = out[2:10]
            for k in range(1, 7):
                if body[k] == body[k + 1]:
                    del body[k]
                    break
            else:
                del body[1]
            out[2:10] = [accent] + body
        else:
            out[2] |= accent
    return out[2:12]


def main():
    glyphs, box = bdf(sys.argv[1])
    print('/* The clean font of the port (opendune.ini font=clean), made by')
    print(' * tools/lang/make_clean_font.py from %s. Do not edit. */' % (
        '6x10.bdf of the X11 "fixed" fonts (public domain; the ucs-fonts of Markus Kuhn)' if box[1] == 10 else
        'ter-u12n.bdf of Terminus Font 4.49.1, Copyright (C) 2020 Dimitar Toshkov Zhekov, SIL Open Font License 1.1 '
        '(a part of that font in another form: a modified version, not called by the reserved name)'))
    print('')
    print('#define CLEAN_WIDTH  %d' % W)
    print('#define CLEAN_HEIGHT 10')
    print('typedef struct CleanGlyph { uint8 code; uint8 polish; uint8 rows[CLEAN_HEIGHT]; } CleanGlyph;')
    print('static const CleanGlyph s_cleanGlyphs[] = {')
    for group, polish in ((ASCII, 0), (GERMAN, 0), (POLISH, 1)):
        for code, ch in group:
            rows = cell(glyphs, box, ch)
            name = ch if 32 < ord(ch) < 127 and ch not in '\\*/' else 'U+%04X' % ord(ch)
            print('\t{ 0x%02X, %d, { %s } },\t/* %s */' % (code, polish, ', '.join('0x%02X' % r for r in rows), name))
    print('};')


if __name__ == '__main__':
    main()
