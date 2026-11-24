#!/usr/bin/env python3
"""MaxTrax (.MX, the music of the Amiga Dune II) recorded to a WAV or an ODMU RAW.

Written for this port from the format (tools/music/mx_info.py) and how the
MaxTrax driver behaves (as ScummVM's audio/mods/maxtrax.cpp describes it):
a 50 Hz interrupt, 4 Paula voices (0, 3 left; 1, 2 right), 16 channels,
patches = samples with up to several octaves, attack/release envelopes in ms.

  tools/music/mx_render.py FILE.MX SCORE OUT.wav|OUT.RAW [--seconds N] [--loop]

OUT.RAW: the port's music format ('ODMU', loop start, length, 8-bit signed
mono at 22030 Hz, audio/music_amiga.c); --loop: the score repeats from its
start (loop start 0), else it plays once.
"""
import argparse
import struct
import wave

import numpy as np

RATE = 22030		# Paula PAL period 161 (the port's music rate)
PAULA = 3546895		# PAL Paula clock
VBLANK = 50
FRAME = RATE / VBLANK
K_VALUE, PREF_PERIOD, PERIOD_LIMIT = 0x9fd77, 0x8fd77, 0x6f73d


def load(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'MXTX'
    tempo, flags = struct.unpack('>HH', d[4:8])
    p = 8
    if flags & 0x8000:
        p += 256
    n, = struct.unpack('>H', d[p:p + 2]); p += 2
    scores = []
    for _ in range(n):
        ne, = struct.unpack('>I', d[p:p + 4]); p += 4
        ev = []
        for i in range(ne):
            cmd, par, start, stop = struct.unpack('>BBHH', d[p:p + 6]); p += 6
            ev.append((cmd, par, start, stop))
        scores.append(ev)
    ns, = struct.unpack('>H', d[p:p + 2]); p += 2
    patches = {}
    for _ in range(ns):
        num, tune, vol, octs, alen, slen, ac, rc = struct.unpack('>HhHHIIHH', d[p:p + 20]); p += 20
        env = [struct.unpack('>HH', d[p + 4 * i:p + 4 * i + 4]) for i in range(ac + rc)]
        p += 4 * (ac + rc)
        total = alen + slen
        size = total * ((1 << octs) - 1)
        data = np.frombuffer(d[p:p + size], dtype=np.int8).astype(np.float32)
        p += size
        patches[num & 63] = dict(tune=tune, volume=vol, octaves=octs, attack=alen, total=total,
                                 attackEnv=env[:ac], releaseEnv=env[ac:], data=data)
    return tempo, flags, scores, patches


def calc_tempo(t):
    return ((t & 0xFFF0) << 8) // (5 * VBLANK)


class Voice:
    def __init__(self):
        self.status = 0	# 0 free, 1 halt, 2 decay, 3 release, 4 sustain, 5 attack, 6 start
        self.channel = None
        self.lastVolume = 0
        self.tieBreak = 0


def render(tempo0, flags, events, patches, seconds, loop, jumps=0):
    """jumps > 0: stop when the score's repeat jumped back that many times
    (the sample positions of the jumps: render.marks)."""
    handle_volume = (flags & 2) != 0
    master = 0x40
    chans = []
    for i in range(16):
        chans.append(dict(patch=i, volume=128, bendRange=24, pitchReal=0, right=(i & 1) != 0, rpn=0xFFFF))
    voices = [Voice() for _ in range(4)]
    out = np.zeros((int(seconds * RATE) + RATE, ), dtype=np.float32)
    tick_unit = calc_tempo(tempo0 << 4)
    ticks = 0
    idx = 0
    next_time = events[0][2] if events else 0
    ended = False
    reps = []	# [event index of the begin, repeats left]
    marks = []	# sample position of every jump back of a repeat
    render.marks = marks
    pos = 0.0
    frame_no = 0
    total_frames = int(seconds * VBLANK)

    def kill(v):
        v.status = 0
        v.channel = None
        v.lastVolume = 0

    def note_on(ch, note, vol):
        if vol == 0:
            return None
        c = chans[ch]
        patch = patches.get(c['patch'])
        if patch is None:
            return None
        pick = 1 if c['right'] else 0
        a, b = voices[pick], voices[pick ^ 3]
        va = (a.status << 8 | a.lastVolume) + a.tieBreak
        vb = b.status << 8 | b.lastVolume
        if va > vb:
            a.tieBreak, b.tieBreak = 0, 1
            pick ^= 3
        v = voices[pick]
        if v.channel is not None:
            kill(v)
        plain = K_VALUE + 0x3C000 - ((note << 14) + (patch['tune'] << 11) // 3) // 3
        octv = 0 if plain <= PREF_PERIOD else min((plain + 0xFFFF - PREF_PERIOD) >> 16, patch['octaves'] - 1)
        v.pre = plain - (octv << 16)
        v.octave = octv
        v.patch = patch
        v.channel = ch
        v.noteVolume = vol + 1 if handle_volume else 128
        v.baseVolume = 0
        v.status = 6
        v.envelope = None
        total = patch['total']
        start = (total << octv) - total
        v.attackStart = start
        v.attackLen = patch['attack'] << octv
        v.sustainLen = (total - patch['attack']) << octv
        v.phase = 0.0
        v.inSustain = v.attackLen == 0
        v.stop = 0
        v.lastPeriod = 0
        return v

    def start_env(v, env, status):
        v.env = env
        v.envIdx = 0
        v.ticksLeft = env[0][0] << 8
        v.status = status
        v.incr = vol_delta(env[0][1] - v.baseVolume, env[0][0])

    def vol_delta(delta, time):
        div = time * VBLANK
        return delta if div <= 1000 else int(1000 * delta / div)

    frame_unit = (1000 << 8) // VBLANK
    while frame_no < total_frames:
        # the events of this interrupt
        ticks += tick_unit
        millis = ticks >> 8
        if not ended and idx < len(events):
            delta = next_time - millis
            while delta <= 0:
                cmd, par, start, stop = events[idx]
                ch = par & 15
                c = chans[ch]
                if cmd < 0x80:
                    v = note_on(ch, cmd, (par & 0xF0) >> 1)
                    if v is not None:
                        v.stop = max(0, (delta + stop) << 8)
                elif cmd == 0x80:
                    tick_unit = calc_tempo(par << 4)
                elif cmd == 0xB0:
                    ctl, val = stop >> 8, stop & 0xFF
                    if ctl == 7:
                        c['volume'] = 0 if val == 0 else val + 1
                    elif ctl == 10:
                        c['right'] = val > 0x40 or (val == 0x40 and (ch & 1) != 0)
                    elif ctl == 0x64:
                        c['rpn'] = (c['rpn'] & 0xFF00) | val
                    elif ctl == 0x65:
                        c['rpn'] = (c['rpn'] & 0xFF) | (val << 8)
                    elif ctl == 6 and c['rpn'] == 0:
                        c['bendRange'] = min(24, val)
                    elif ctl in (0x7B, 0x7E, 0x7F):
                        for v in voices:
                            if v.channel == ch and v.status > 3:
                                v.stop = 0
                elif cmd == 0xA0:
                    sub, arg = stop >> 8, stop & 0xFF
                    if sub == 2:	# begin of a repeat: arg times more
                        reps.append([idx, arg])
                    elif sub == 3 and reps:	# end of a repeat
                        if reps[-1][1] > 0:
                            reps[-1][1] -= 1
                            idx = reps[-1][0]
                            marks.append(int(pos))
                        else:
                            reps.pop()
                elif cmd == 0xC0:
                    c['patch'] = stop & 63
                elif cmd == 0xE0:
                    bend = ((stop & 0x7F00) >> 1) | (stop & 0x7F)
                    c['pitchReal'] = ((c['bendRange'] * bend) >> 5) - (c['bendRange'] << 8)
                elif cmd == 0xFF:
                    if loop:
                        idx = 0
                        ticks = 0
                        next_time = events[0][2]
                        delta = next_time - millis
                        break
                    ended = True
                    break
                idx += 1
                if idx >= len(events):
                    ended = True
                    break
                delta += events[idx][2]
            next_time = delta + millis
        # the voices
        for v in voices:
            if v.channel is None:
                continue
            if v.status > 3:
                v.stop -= tick_unit
                if v.stop <= 0:
                    v.status = 3
            if v.status == 6:
                if v.patch['attackEnv']:
                    start_env(v, v.patch['attackEnv'], 5)
                else:
                    v.status = 4
                    v.baseVolume = v.patch['volume']
            elif v.status == 3:
                if v.patch['releaseEnv']:
                    start_env(v, v.patch['releaseEnv'], 2)
                else:
                    v.status = 1
            if v.status in (5, 2) and v.env is not None:
                if v.ticksLeft > frame_unit:
                    v.baseVolume = min(max(0, v.baseVolume + v.incr), 0x8000)
                    v.ticksLeft -= frame_unit
                else:
                    v.baseVolume = v.env[v.envIdx][1]
                    v.envIdx += 1
                    if v.envIdx < len(v.env):
                        e = v.env[v.envIdx]
                        v.ticksLeft = e[0] << 8
                        v.incr = vol_delta(e[1] - v.baseVolume, e[0])
                    elif v.status == 2:
                        v.status = 1
                    else:
                        v.status = 4
                        v.env = None
            if v.status == 1:
                kill(v)
                continue
            vol = (v.noteVolume * master) >> 7 if v.noteVolume < 128 else master
            if v.baseVolume < 0x8000:
                vol = (vol * v.baseVolume) >> 15
            cv = chans[v.channel]['volume']
            if cv < 128:
                vol = (vol * cv) >> 7
            v.lastVolume = min(vol, 64)
            tone = v.pre + (chans[v.channel]['pitchReal'] << 6) // 3
            v.lastPeriod = int(round(2 ** (tone / 65536.0))) if tone >= PERIOD_LIMIT else 0
        # the sound of this frame
        f0 = int(pos)
        pos += FRAME
        n = int(pos) - f0
        for v in voices:
            if v.channel is None or v.lastPeriod == 0 or v.lastVolume == 0:
                continue
            step = PAULA / v.lastPeriod / RATE
            ph = v.phase + step * np.arange(n, dtype=np.float64)
            data = v.patch['data']
            a0, al, sl = v.attackStart, v.attackLen, v.sustainLen
            if sl > 0:
                idxs = np.where(ph < al, ph, al + np.mod(ph - al, sl)).astype(np.int64)
                ok = np.ones(n, dtype=bool)
            else:
                idxs = ph.astype(np.int64)
                ok = idxs < al
                idxs = np.minimum(idxs, max(al - 1, 0))
                if not ok.any():
                    v.stop = 0	# the sample is over: release (dmaOff)
            s = data[a0 + idxs] * ok
            out[f0:f0 + n] += s * (v.lastVolume / 64.0)
            v.phase += step * n
            if sl > 0 and v.phase >= al + sl:
                v.phase = al + np.mod(v.phase - al, sl)
        frame_no += 1
        if jumps and len(marks) >= jumps:
            pos = marks[jumps - 1]
            break
        if ended and all(v.channel is None for v in voices):
            break
    return out[:int(pos)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('mx')
    ap.add_argument('score', type=int)
    ap.add_argument('out')
    ap.add_argument('--seconds', type=float, default=400)
    ap.add_argument('--loop', action='store_true')
    ap.add_argument('--gain', type=float, default=1.0, help='an extra gain')
    ap.add_argument('--repeat-loop', action='store_true',
                    help='the repeat of the score as the loop: the start and two passes, the loop from the second one')
    a = ap.parse_args()
    tempo, flags, scores, patches = load(a.mx)
    pcm = render(tempo, flags, scores[a.score], patches, a.seconds, a.loop, 2 if a.repeat_loop else 0)
    loop_at = render.marks[0] if a.repeat_loop and render.marks else (0 if a.loop else 0xFFFFFFFF)
    # 4 voices of up to 127 each: one gain, the same for every score
    gain = 127.0 / (128.0 * 1.1) * a.gain
    pcm = np.clip(np.round(pcm * gain), -128, 127).astype(np.int8)
    if a.out.lower().endswith('.wav'):
        w = wave.open(a.out, 'wb')
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(RATE)
        w.writeframes((pcm.astype(np.int16) + 128).astype(np.uint8).tobytes())
        w.close()
    else:
        loop = loop_at
        with open(a.out, 'wb') as f:
            f.write(b'ODMU' + struct.pack('>II', loop, len(pcm)) + pcm.tobytes())
    peak = int(np.abs(pcm.astype(np.int16)).max()) if len(pcm) else 0
    print('%s score %d: %.1f s, peak %d' % (a.out, a.score, len(pcm) / RATE, peak))


if __name__ == '__main__':
    main()
