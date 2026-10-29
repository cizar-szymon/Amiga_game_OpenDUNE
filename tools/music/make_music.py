#!/usr/bin/env python3
"""Records the music of Dune II once, for the Amiga port (which only plays it).

The AdLib music (.ADL in SOUND.PAK of the PC data) is played by AdPlug with
the Nuked OPL3 emulator (render_adl.cpp, built in Docker), each of the 37
pieces of g_table_musics (file, track) to its own file:

  build/music/DUNE<n>_<track>.RAW   'ODMU', loop start, length (samples, 32-bit
                                    big endian; loop 0xFFFFFFFF: plays once), then
                                    8-bit signed mono, 22030 Hz (Paula PAL period
                                    161), one gain for all pieces

A piece the original loops is recorded once (render_adl.cpp finds the loop in
the OPL writes), the Amiga plays it again from the loop start. Pieces the AdLib
version does not have (silent) get no file.

Copy the directory to the game's directory as MUSIC (PROGDIR:MUSIC). The files
are made from the game's data: never in the repository (build/ is ignored).

  python tools/music/make_music.py [--data data] [--out build/music] [--wav]
  --wav  also 16-bit WAV previews of the same (build/music/wav)
"""
import argparse
import os
import struct
import subprocess
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
RATE_OPL = 49716		# the OPL's own rate
RATE_AMIGA = 22030		# Paula, PAL, period 161
MAX_SECONDS = 600

# g_table_musics (port/src/table/sound.c): musicID 1..37 -> (file, track)
MUSICS = [
    ("dune1", 2), ("dune1", 3), ("dune1", 4), ("dune1", 5), ("dune17", 4), ("dune8", 3), ("dune8", 2),
    ("dune1", 6), ("dune2", 6), ("dune3", 6), ("dune4", 6), ("dune5", 6), ("dune6", 6), ("dune9", 4),
    ("dune9", 5), ("dune18", 6), ("dune10", 7), ("dune11", 7), ("dune12", 7), ("dune13", 7), ("dune14", 7),
    ("dune15", 7), ("dune1", 8), ("dune7", 2), ("dune7", 3), ("dune7", 4), ("dune0", 2), ("dune7", 6),
    ("dune16", 7), ("dune19", 4), ("dune19", 2), ("dune19", 3), ("dune20", 2), ("dune16", 8), ("dune0", 3),
    ("dune0", 4), ("dune0", 5),
]


def pak_files(path):
    """The files of a Westwood PAK: {name: bytes}."""
    d = open(path, "rb").read()
    entries, i = [], 0
    while True:
        off = struct.unpack("<I", d[i:i + 4])[0]
        if off == 0:
            break
        j = d.index(b"\0", i + 4)
        entries.append((d[i + 4:j].decode("ascii"), off))
        i = j + 1
    files = {}
    for k, (name, off) in enumerate(entries):
        end = entries[k + 1][1] if k + 1 < len(entries) else len(d)
        files[name.upper()] = d[off:end]
    return files


def find_loop(path):
    """The loop of a piece that plays on, from render_adl's ticks file (per
    tick: the hash of the notes struck, the first sample). The driver's
    random effects change some notes, so: the shortest period L whose notes
    match within a point of the best one (over the second half), its start S
    the first tick from which the rest matches nearly as well. Returns the
    start and end samples (OPL rate), the match and L in seconds."""
    t = np.fromfile(path, dtype="<u8").reshape(-1, 2)
    h, pos = t[:, 0], t[:, 1].astype(np.int64)
    n = len(h)
    values, counts = np.unique(h, return_counts=True)
    idle = values[counts.argmax()]		# the ticks without notes
    late = np.nonzero(h[n // 2:] != idle)[0] + n // 2
    scores = np.array([np.mean(h[late] == h[late - L]) for L in range(5 * 72, n // 2)])
    best = scores.max()
    L = 5 * 72 + int(np.nonzero(scores >= best - 0.01)[0][0])
    act = np.nonzero(h[:n - L] != idle)[0]
    same = (h[act] == h[act + L]).astype(np.float64)
    rest = np.cumsum(same[::-1])[::-1] / np.arange(len(same), 0, -1)	# the match from each note on
    S = int(act[np.nonzero(rest >= best - 0.03)[0][0]])
    return int(pos[S]), int(pos[S + L]), float(best), L / 72.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default=os.path.join(ROOT, "data"))
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "music"))
    ap.add_argument("--wav", action="store_true")
    a = ap.parse_args()

    work = os.path.join(a.out, "work")
    os.makedirs(work, exist_ok=True)
    files = pak_files(os.path.join(a.data, "SOUND.PAK"))
    for name in sorted({f for f, _ in MUSICS}):
        open(os.path.join(work, name.upper() + ".ADL"), "wb").write(files[name.upper() + ".ADL"])

    # all tracks in one container run (the build of render_adl once)
    lines = ["set -e", "apt-get update -qq >/dev/null", "apt-get install -y -qq g++ pkg-config libadplug-dev libbinio-dev >/dev/null 2>&1",
             "g++ -O2 -o /tmp/render_adl /tools/render_adl.cpp $(pkg-config --cflags --libs adplug)"]
    for name, track in MUSICS:
        base = "%s_%d" % (name.upper(), track)
        lines.append('rm -f /work/%s.s16; echo "%s $(/tmp/render_adl /work/%s.ADL %d %d %d /work/%s.s16)"' %
                     (base, base, name.upper(), track, RATE_OPL, MAX_SECONDS, base))
    script = os.path.join(work, "render.sh")
    open(script, "w", newline="\n").write("\n".join(lines) + "\n")
    r = subprocess.run(["docker", "run", "--rm", "-v", HERE + ":/tools:ro", "-v", work + ":/work",
                        "debian:bookworm", "bash", "/work/render.sh"], capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if r.returncode != 0:
        sys.exit(r.stderr)
    ended = {}
    for line in r.stdout.splitlines():
        f = line.split()
        ended[f[0]] = f[3] == "1" if len(f) == 4 else None	# None: AdPlug failed (no such track)

    if a.wav:
        os.makedirs(os.path.join(a.out, "wav"), exist_ok=True)
    # resampled by ffmpeg (float, no clipping); a piece that plays on cut
    # after one loop; one gain for all (the pieces keep their loudness to
    # each other), the loudest peak at 8 bits
    tracks, loops = {}, {}
    for name, track in MUSICS:
        base = "%s_%d" % (name.upper(), track)
        s16 = os.path.join(work, base + ".s16")
        if ended.get(base) is None or not os.path.exists(s16):
            tracks[base] = np.zeros(0, dtype=np.float32)
            continue
        f32 = subprocess.run(["ffmpeg", "-v", "error", "-f", "s16le", "-ar", str(RATE_OPL), "-ac", "1", "-i", s16,
                              "-ar", str(RATE_AMIGA), "-f", "f32le", "-"], capture_output=True, check=True).stdout
        x = np.frombuffer(f32, dtype=np.float32)
        loops[base] = None
        if not ended[base]:
            start, end, score, seconds = find_loop(s16 + ".ticks")
            k = RATE_AMIGA / RATE_OPL
            loops[base] = round(start * k)
            x = x[:round(end * k)]
            print("%s: loop %.1f s from %.1f s (notes alike %.0f%%)" % (base, seconds, start / RATE_OPL, score * 100))
        tracks[base] = x
    peak = max(float(np.max(np.abs(x))) if len(x) else 0.0 for x in tracks.values()) or 1.0

    total, silent = 0, []
    for base, x in tracks.items():
        path = os.path.join(a.out, base + ".RAW")
        if len(x) == 0 or float(np.max(np.abs(x))) < peak / 1000:
            silent.append(base)		# nothing in the AdLib version: no file, the game plays nothing
            if os.path.exists(path):
                os.remove(path)
            continue
        s8 = np.clip(np.round(x / peak * 127.0), -128, 127).astype(np.int8)
        loop = 0xFFFFFFFF if loops[base] is None else min(len(s8) - 1, loops[base])
        # 'ODMU', the loop start (samples, 0xFFFFFFFF: none), the length; big endian
        open(path, "wb").write(b"ODMU" + struct.pack(">II", loop, len(s8)) + s8.tobytes())
        total += len(s8)
        if a.wav:
            with wave.open(os.path.join(a.out, "wav", base + ".wav"), "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(RATE_AMIGA)
                w.writeframes((np.clip(x / peak, -1, 1) * 32767).astype("<i2").tobytes())
    print("%d files, %.1f MB, %.1f minutes; silent: %s" %
          (len(tracks) - len(silent), total / 1e6, total / RATE_AMIGA / 60, " ".join(silent) or "-"))


if __name__ == "__main__":
    main()
