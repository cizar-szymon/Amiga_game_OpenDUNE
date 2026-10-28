"""Drive OpenDUNE on the Amiga through its test commands (debug_commands=1,
port/src/video/amiga_cmd.c) via AmiAgent.

  python tools/od.py "click 160 100" "wait 30" "key esc"   run lines, wait for the end
  python tools/od.py --shot out.png "click 160 100"          ... then a 320x200 shot
  python tools/od.py --state                                 mode, mission, mouse

Coordinates are the game's (320x200). Default config: amiagent.cfg (Amiga 1), as amiga_remote.py."""
import argparse, os, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
REMOTE = [sys.executable, os.path.join(HERE, "amiga_remote.py")]


def remote(cfg, *args, quiet=True):
    r = subprocess.run(REMOTE + ["--config", cfg] + list(args), capture_output=True, text=True,
                       encoding="latin-1")
    if not quiet:
        sys.stdout.write(r.stdout)
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="C:/amiga-dev/amiagent.cfg")
    ap.add_argument("--shot", help="PNG file for a 320x200 shot after the commands")
    ap.add_argument("--scale", type=int, default=2, help="enlargement of the PNG")
    ap.add_argument("--state", action="store_true", help="print RAM:od-state.txt at the end")
    ap.add_argument("--timeout", type=float, default=60)
    ap.add_argument("lines", nargs="*")
    a = ap.parse_args()

    lines = list(a.lines)
    if a.shot:
        lines.append("shot")
    if a.state:
        lines.append("state")
    if not lines:
        return
    tmp = tempfile.mkdtemp()
    cmd = os.path.join(tmp, "od-cmd")
    with open(cmd, "w", newline="\n", encoding="latin-1") as f:
        f.write("\n".join(lines) + "\n")
    remote(a.config, "run", "Delete RAM:od-done QUIET")
    remote(a.config, "put", cmd, "RAM:od-cmd")
    t0 = time.time()
    done = os.path.join(tmp, "od-done")
    while time.time() - t0 < a.timeout:
        time.sleep(0.5)
        r = remote(a.config, "get", "RAM:od-done", done)
        if r.returncode == 0 and os.path.exists(done):
            break
    else:
        sys.exit("timeout: is the game running with debug_commands=1?")
    if a.shot:
        ppm = os.path.join(tmp, "od-shot.ppm")
        remote(a.config, "get", "RAM:od-shot.ppm", ppm)
        try:
            from PIL import Image
            im = Image.open(ppm)
            if a.scale > 1:
                im = im.resize((im.width * a.scale, im.height * a.scale), Image.NEAREST)
            im.save(a.shot)
        except ImportError:
            os.replace(ppm, os.path.splitext(a.shot)[0] + ".ppm")
        print("shot:", a.shot)
    if a.state:
        st = os.path.join(tmp, "od-state.txt")
        remote(a.config, "get", "RAM:od-state.txt", st)
        print(open(st, encoding="latin-1").read(), end="")


if __name__ == "__main__":
    main()
