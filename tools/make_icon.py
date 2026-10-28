#!/usr/bin/env python3
"""Build the Workbench icon of the OpenDUNE Amiga port from a frame of the game
(copied from the Vastar port).

  python tools/make_icon.py OUT.info --frame build/shots/frame_001600.bmp --crop X Y W H
                            [--preview OUT.png] [--tooltypes "SCALE=1" "(STATS)" ...]

The frame comes from the PC version (vastar -headless -shots DIR N plus a -script that
starts a game). The icon has a classic 4 colour image (any Workbench) and an OS 3.5+
ColorIcon (FORM ICON with FACE and IMAG chunks, RLE compressed) with the game's colours,
which AmigaOS 3.2 shows. Based on the Wizard of Wor port's icon tool.
"""

import argparse
import struct

from PIL import Image


def rle_pack(values, depth):
    """ColorIcon RLE: ILBM byterun1 on a bit stream of `depth` bit values"""
    out_bits = []

    def emit(v, n):
        for i in range(n - 1, -1, -1):
            out_bits.append((v >> i) & 1)

    i = 0
    while i < len(values):
        run = 1
        while i + run < len(values) and values[i + run] == values[i] and run < 128:
            run += 1
        if run >= 3:
            emit(257 - run, 8)
            emit(values[i], depth)
            i += run
            continue
        lit = 0
        while i + lit < len(values) and lit < 128:
            r = 1
            while i + lit + r < len(values) and values[i + lit + r] == values[i + lit] and r < 3:
                r += 1
            if r >= 3:
                break
            lit += 1
        emit(lit - 1, 8)
        for v in values[i:i + lit]:
            emit(v, depth)
        i += lit
    while len(out_bits) % 8:
        out_bits.append(0)
    return bytes(int("".join(map(str, out_bits[k:k + 8])), 2) for k in range(0, len(out_bits), 8))


def chunk(name, data):
    pad = b"\0" if len(data) & 1 else b""
    return name + struct.pack(">I", len(data)) + data + pad


def color_icon(idx, w, h, colors):
    depth = max(1, (len(colors) - 1).bit_length())
    pixels = rle_pack(idx, depth)
    palette = b"".join(struct.pack(">BBB", *c) for c in colors)
    face = struct.pack(">BBBBH", w - 1, h - 1, 0, 0x11, len(palette) - 1)
    imag = struct.pack(">BBBBBBHH", 0, len(colors) - 1, 0x02, 1, 0, depth, len(pixels) - 1,
                       len(palette) - 1) + pixels + palette
    body = b"ICON" + chunk(b"FACE", face) + chunk(b"IMAG", imag)
    return b"FORM" + struct.pack(">I", len(body)) + body


# the default Workbench 3.x colours: grey, black, white, blue
WB4 = [(0xAA, 0xAA, 0xAA), (0x00, 0x00, 0x00), (0xFF, 0xFF, 0xFF), (0x66, 0x88, 0xBB)]


def nearest_wb(c):
    return min(range(4), key=lambda i: sum((c[k] - WB4[i][k]) ** 2 for k in range(3)))


def planar(rgb, w, h):
    """the classic 2 bitplane image in the 4 Workbench colours"""
    words = (w + 15) // 16
    planes = []
    for p in range(2):
        data = bytearray()
        for y in range(h):
            bits = 0
            for x in range(words * 16):
                v = nearest_wb(rgb[y * w + x]) if x < w else 0
                bits = (bits << 1) | ((v >> p) & 1)
            data += bits.to_bytes(words * 2, "big")
        planes.append(bytes(data))
    return b"".join(planes)


def tool_types(types):
    out = struct.pack(">I", (len(types) + 1) * 4)
    for t in types:
        b = t.encode("latin-1") + b"\0"
        out += struct.pack(">I", len(b)) + b
    return out


def disk_object(rgb, w, h, types):
    gadget = struct.pack(">IhhhhHHHIIIIIHI", 0, 0, 0, w, h + 1, 0x0004, 0x0003, 0x0001,
                         1, 0, 0, 0, 0, 0, 1)
    head = struct.pack(">HH", 0xE310, 1) + gadget + struct.pack(
        ">BBIIiiIII", 3, 0, 0, 1 if types else 0, -0x80000000, -0x80000000, 0, 0, 16384)
    image = struct.pack(">hhhhhIBBI", 0, 0, w, h, 2, 1, 3, 0, 0) + planar(rgb, w, h)
    colors = sorted(set(rgb))
    if len(colors) > 256:
        raise SystemExit("more than 256 colours")
    remap = {c: i for i, c in enumerate(colors)}
    return head + image + (tool_types(types) if types else b"") + \
        color_icon([remap[c] for c in rgb], w, h, colors)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--frame", required=True, help="BMP/PNG frame of the game")
    ap.add_argument("--crop", nargs=4, type=int, required=True, metavar=("X", "Y", "W", "H"))
    ap.add_argument("--preview")
    ap.add_argument("--tooltypes", nargs="*", default=[])
    args = ap.parse_args()
    x, y, w, h = args.crop
    img = Image.open(args.frame).convert("RGB").crop((x, y, x + w, y + h))
    rgb = list(img.getdata())
    open(args.out, "wb").write(disk_object(rgb, w, h, args.tooltypes))
    if args.preview:
        img.resize((w * 6, h * 6), Image.NEAREST).save(args.preview)
    print(f"{args.out}: {w}x{h}, {len(set(rgb))} colours, tool types {args.tooltypes}")


if __name__ == "__main__":
    main()
