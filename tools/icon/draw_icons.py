"""The pictures of two Workbench icons of the release, drawn here (no
system or game graphics in them):
  drawer.png     the game's drawer: a plain blue drawer, as the AmigaOS 3.2
                 drawers look (the user's wish: "a normal one, like the others")
  makemusic.png  MakeMusic: a loudspeaker with sound waves and two notes
  doc_pl.png, doc_en.png, doc_i.png   the texts: a page with the language (i: Install.txt)
                 (the label in Noto Sans Bold, SIL Open Font License 1.1)
Magenta (255,0,255) is transparent (tools/make_icon.py --transparent).

  python tools/icon/draw_icons.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
S = 8                      # drawn 8 times larger, then scaled down (smooth edges)
CLEAR = (255, 0, 255)
BACK = (0xAA, 0xAA, 0xAA)  # the edges blended for the Workbench's grey


def finish(big, w, h, name, colours=48):
    small = big.resize((w, h), Image.LANCZOS)
    out = Image.new('RGB', (w, h), CLEAR)
    flat = Image.new('RGB', (w, h), BACK)
    flat.paste(small, (0, 0), small)
    flat = flat.quantize(colours, dither=Image.NONE).convert('RGB')
    alpha = small.split()[3]
    for y in range(h):
        for x in range(w):
            if alpha.getpixel((x, y)) >= 110:
                out.putpixel((x, y), flat.getpixel((x, y)))
    out.save(os.path.join(HERE, name))
    out.resize((w * 6, h * 6), Image.NEAREST).save(os.path.join(HERE, '..', '..', 'build', name.replace('.png', '-preview.png')))


def drawer():
    w, h = 46, 40
    im = Image.new('RGBA', (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    k = S
    # the top (seen from above), lighter
    d.polygon([(4 * k, 12 * k), (9 * k, 4 * k), (37 * k, 4 * k), (42 * k, 12 * k)], fill=(120, 160, 220, 255), outline=(30, 50, 90, 255), width=k)
    # the front, a vertical gradient
    for y in range(12 * k, 36 * k):
        t = (y - 12 * k) / (24 * k)
        c = (int(85 - 35 * t), int(125 - 45 * t), int(200 - 60 * t), 255)
        d.line([(4 * k, y), (42 * k, y)], fill=c)
    d.rectangle([4 * k, 12 * k, 42 * k, 36 * k], outline=(30, 50, 90, 255), width=k)
    # a light edge along the front's top
    d.line([(5 * k, 13 * k), (41 * k, 13 * k)], fill=(170, 200, 240, 255), width=k)
    # the handle: a white bar with a dark shadow
    d.rounded_rectangle([16 * k, 26 * k, 30 * k, 30 * k], radius=2 * k, fill=(40, 50, 70, 255))
    d.rounded_rectangle([16 * k, 25 * k, 30 * k, 29 * k], radius=2 * k, fill=(245, 245, 245, 255), outline=(60, 70, 90, 255), width=k // 2)
    d.rectangle([18 * k, 25 * k + k // 2, 28 * k, 27 * k], fill=(150, 160, 175, 255))
    finish(im, w, h, 'drawer.png')


def makemusic():
    w, h = 54, 44
    im = Image.new('RGBA', (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    k = S
    dark = (25, 25, 35, 255)
    # the loudspeaker: the magnet box and the cone
    d.rectangle([3 * k, 17 * k, 11 * k, 29 * k], fill=(90, 95, 110, 255), outline=dark, width=k)
    d.polygon([(11 * k, 17 * k), (21 * k, 8 * k), (21 * k, 38 * k), (11 * k, 29 * k)], fill=(150, 155, 170, 255), outline=dark)
    d.line([(11 * k, 17 * k), (21 * k, 8 * k), (21 * k, 38 * k), (11 * k, 29 * k)], fill=dark, width=k)
    # sound waves
    for i, r in enumerate((6, 10)):
        box = [(21 - r) * k, (23 - r) * k, (21 + r) * k, (23 + r) * k]
        d.arc(box, -45, 45, fill=(240, 240, 250, 255), width=int(1.6 * k))
    # two beamed eighth notes, gold
    gold, edge = (250, 200, 50, 255), (120, 70, 0, 255)
    for cx, cy in ((36, 31), (47, 27)):
        d.ellipse([(cx - 4) * k, (cy - 3) * k, (cx + 3) * k, (cy + 3) * k], fill=gold, outline=edge, width=k)
    d.line([(39 * k, 31 * k), (39 * k, 9 * k)], fill=edge, width=2 * k)
    d.line([(50 * k, 27 * k), (50 * k, 5 * k)], fill=edge, width=2 * k)
    d.polygon([(38 * k, 9 * k), (51 * k, 4 * k), (51 * k, 9 * k), (38 * k, 14 * k)], fill=gold, outline=edge)
    finish(im, w, h, 'makemusic.png')


def document(label, colour):
    w, h = 36, 44
    im = Image.new('RGBA', (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    k = S
    edge = (40, 40, 50, 255)
    # the page with a folded corner
    page = [(4 * k, 2 * k), (24 * k, 2 * k), (32 * k, 10 * k), (32 * k, 42 * k), (4 * k, 42 * k)]
    d.polygon(page, fill=(250, 250, 250, 255))
    d.line(page + [page[0]], fill=edge, width=k)
    d.polygon([(24 * k, 2 * k), (24 * k, 10 * k), (32 * k, 10 * k)], fill=(200, 200, 210, 255), outline=edge)
    # lines of text
    for y in (8, 12, 16, 20):
        d.line([(8 * k, y * k), ((20 if y == 8 else 28) * k, y * k)], fill=(150, 150, 160, 255), width=k)
    # the language
    d.rounded_rectangle([7 * k, 25 * k, 29 * k, 38 * k], radius=2 * k, fill=colour, outline=edge, width=k)
    font = ImageFont.truetype(FONT, 10 * k)
    x0, y0, x1, y1 = d.textbbox((0, 0), label, font=font)
    d.text((18 * k - (x0 + x1) / 2, 31.5 * k - (y0 + y1) / 2), label, font=font, fill=(255, 255, 255, 255))
    finish(im, w, h, 'doc_%s.png' % label.lower())


FONT = os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'Fonts', 'NotoSansJP-Bold.ttf')

if __name__ == '__main__':
    os.makedirs(os.path.join(HERE, '..', '..', 'build'), exist_ok=True)
    drawer()
    makemusic()
    document('PL', (200, 40, 50, 255))
    document('EN', (40, 80, 170, 255))
    document('i', (210, 120, 20, 255))	# Install.txt: all three languages
    print('tools/icon: drawer.png, makemusic.png, doc_pl.png, doc_en.png, doc_i.png')
