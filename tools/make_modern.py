#!/usr/bin/env python3
"""A modern mentat screen (opendune.ini modern=1): a picture in place of
MENTATx.CPS, the briefing's film playing in a monitor on it.

  python tools/make_modern.py PICTURE HOUSE X0 Y0 X1 Y1 [--pak OUT.PAK] [--preview OUT.png]

PICTURE: any size (PNG, JPG ...); HOUSE: A, H, O or M (the mentat's house);
X0 Y0 X1 Y1: the monitor's screen in the picture (pixels, the last ones
included). The picture is scaled so that this screen falls on the film's place
(widget 8: 128,48, 184x112) and cut to 320x200; rows missing above and below
are the picture's edge, darkened at the top for the text line and downwards. The colours:
the game's palette (IBM.PAL; BENE.PAL for M), only the colours the original mentat screen uses
(none of them cycles), with dithering. Written as MODERNx.CPS (uncompressed) into
lang/GFX.PAK (default build/gfx/GFX.PAK; the other houses' pictures in it kept), which the game reads from PROGDIR:lang.

The pictures (tools/gfx) and GFX.PAK stay on this PC (.gitignore).
"""

import argparse
import os
import struct
import sys

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'lang'))
from dune_lang import DataDir, read_pak, write_pak  # noqa: E402

FILM_X, FILM_Y, FILM_W, FILM_H = 128, 48, 184, 112
TEXT_ROWS = 14		# the mentat's text line at the top


def f80(src, size):
    """Format80 (LCW) decoding, as port/src/codec/format80.c"""
    dst = bytearray()
    i = 0
    while len(dst) < size:
        c = src[i]; i += 1
        if c == 0x80:
            break
        if c & 0x80 == 0:
            n = (c >> 4) + 3
            off = ((c & 0xF) << 8) | src[i]; i += 1
            for _ in range(n):
                dst.append(dst[len(dst) - off])
        elif c & 0x40 == 0:
            n = c & 0x3F
            dst += src[i:i + n]; i += n
        elif c == 0xFE:
            n = struct.unpack_from('<H', src, i)[0]; v = src[i + 2]; i += 3
            dst += bytes([v]) * n
        elif c == 0xFF:
            n, off = struct.unpack_from('<HH', src, i); i += 4
            for k in range(n):
                dst.append(dst[off + k])
        else:
            n = (c & 0x3F) + 3
            off = struct.unpack_from('<H', src, i)[0]; i += 2
            for k in range(n):
                dst.append(dst[off + k])
    return bytes(dst[:size])


def cps_pixels(b):
    """The 320x200 pixels of a CPS file"""
    comp = b[2]
    size = struct.unpack_from('<I', b, 4)[0]
    pal = struct.unpack_from('<H', b, 8)[0]
    data = b[10 + pal:]
    return data[:size] if comp == 0 else f80(data, size)


def cps_file(pixels):
    """An uncompressed CPS without a palette (Sprites_Decode type 0)"""
    body = bytes([0, 0]) + struct.pack('<IH', len(pixels), 0) + pixels
    return struct.pack('<H', len(body)) + body


def build(picture, house, rect):
    data = DataDir(os.path.join(ROOT, 'data'))
    pal = data.get('BENE.PAL' if house == 'M' else 'IBM.PAL')	# M (the house choice): GUI_Mentat_Display loads BENE.PAL
    orig = cps_pixels(data.get('MENTAT%s.CPS' % house))
    used = sorted(set(orig) - {0})

    x0, y0, x1, y1 = rect
    scale = FILM_W / float(x1 - x0 + 1)
    im = Image.open(picture).convert('RGB')
    sw, sh = round(im.width * scale), round(im.height * scale)
    im = im.resize((sw, sh), Image.LANCZOS)
    ox = FILM_X - round(x0 * scale)
    oy = FILM_Y - round(y0 * scale)

    out = Image.new('RGB', (320, 200))
    out.paste(im, (ox, oy))
    # rows above / below the picture: its edge row
    for y in range(0, max(0, oy)):
        out.paste(out.crop((0, oy, 320, oy + 1)), (0, y))
    for y in range(oy + sh, 200):
        out.paste(out.crop((0, oy + sh - 1, 320, oy + sh)), (0, y))
    # ... darker towards the bottom
    px = out.load()
    for y in range(max(0, oy + sh), 200):
        f = 0.7 ** (y - oy - sh + 1)
        for x in range(320):
            r, g, b = px[x, y]
            px[x, y] = (int(r * f), int(g * f), int(b * f))
    # the text line: darker
    px = out.load()
    for y in range(TEXT_ROWS):
        f = 0.45 + 0.55 * y / TEXT_ROWS
        for x in range(320):
            r, g, b = px[x, y]
            px[x, y] = (int(r * f), int(g * f), int(b * f))

    # the palette of the colours allowed (the rest far away, never chosen)
    pimg = Image.new('P', (1, 1))
    flat = []
    for i in range(256):
        if i in used or i == 0:
            flat += [min(255, pal[i * 3 + k] * 4) for k in range(3)]
        else:
            flat += [255, 0, 255]
    pimg.putpalette(flat)
    q = out.quantize(palette=pimg, dither=Image.Dither.FLOYDSTEINBERG)
    pixels = bytes(q.getdata())
    bad = set(pixels) - set(used) - {0}
    if bad:
        raise SystemExit('colours outside the allowed ones: %s' % sorted(bad))
    preview = q.convert('RGB')
    return pixels, preview, (ox, oy, sw, sh), len(used)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('picture')
    ap.add_argument('house', choices='AHOM')
    ap.add_argument('rect', type=int, nargs=4)
    ap.add_argument('--pak', default=os.path.join(ROOT, 'build', 'gfx', 'GFX.PAK'))
    ap.add_argument('--preview')
    a = ap.parse_args()

    pixels, preview, place, ncol = build(a.picture, a.house, a.rect)
    os.makedirs(os.path.dirname(os.path.abspath(a.pak)), exist_ok=True)
    files = read_pak(a.pak) if os.path.exists(a.pak) else {}	# the other houses' pictures kept
    files['MODERN%s.CPS' % a.house] = cps_file(pixels)
    write_pak(a.pak, sorted(files.items()))
    if a.preview:
        preview.resize((640, 400), Image.NEAREST).save(a.preview)
    print('written %s: MODERN%s.CPS, picture at %d,%d %dx%d, %d colours of the palette' %
          ((a.pak, a.house) + place + (ncol,)))


if __name__ == '__main__':
    main()
