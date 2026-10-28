"""Leftover check of the map drawing (smooth units, scrolling): the game runs
for a while, is paused (P), a shot is taken, the whole map is drawn again
(test command redraw) and shot again; while paused nothing moves, so every
differing pixel in the viewport is something the incremental drawing left
behind. Needs the game on the map screen with debug_commands=1.

  python tools/ghost_check.py [--cycles N] [--play TICKS]
"""
import argparse, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def od(*lines, shot=None):
    cmd = [sys.executable, os.path.join(HERE, "od.py"), "--scale", "1"]
    if shot:
        cmd += ["--shot", shot]
    subprocess.run(cmd + list(lines), capture_output=True, text=True)


def main():
    from PIL import Image, ImageChops
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycles", type=int, default=4)
    ap.add_argument("--play", type=int, default=900, help="ticks (1/60 s) of play between checks")
    ap.add_argument("--ignore", help='"X0 Y0 X1 Y1" screen area not counted (an animated building)')
    ap.add_argument("--spawn", help='before each cycle: "TYPE HOUSE X Y DX DY" (test command spawn), e.g. "1 2 20 100 540 400" an ornithopter')
    a = ap.parse_args()
    tmp = tempfile.mkdtemp()
    total = 0
    for c in range(a.cycles):
        before, after = os.path.join(tmp, "a%d.png" % c), os.path.join(tmp, "b%d.png" % c)
        pre = ["spawn " + a.spawn] if a.spawn else []
        od(*(pre + ["wait %d" % a.play, "key p", "wait 30"]), shot=before)
        od("redraw", "wait 30", shot=after)
        od("key p", "wait 5")
        ia, ib = Image.open(before).convert("RGB"), Image.open(after).convert("RGB")
        view = (0, 40, 560, 472)		# the viewport of the 640x480 map screen
        if a.ignore:
            x0, y0, x1, y1 = [int(v) for v in a.ignore.split()]
            ib.paste(ia.crop((x0, y0, x1, y1)), (x0, y0))
        d = ImageChops.difference(ia.crop(view), ib.crop(view))
        n = d.convert('L').point(lambda v: 255 if v else 0).histogram()[255]
        total += n
        print("cycle %d: %d leftover pixels, bbox %s" % (c, n, d.getbbox()))
        if n:
            ia.save(os.path.join(tmp, "leftover%d.png" % c))
    print("total %d leftover pixels (shots in %s)" % (total, tmp))


if __name__ == "__main__":
    main()
