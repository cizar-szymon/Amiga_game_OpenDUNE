#!/usr/bin/env python3
"""MaxTrax (.MX, Westwood's Amiga music: Dune II, Kyrandia) - what is in a file.

The format (as ScummVM's audio/mods/maxtrax.cpp reads it), big endian:
  'MXTX', tempo u16, flags u16 (bit 0: 128 u16 microtonal table follows)
  numScores u16; each: numEvents u32, events of 6 bytes
      (command u8, parameter u8, startTime u16, stopTime u16)
  numSamples u16; each: number u16, tune u16, volume u16, octaves u16,
      attackLen u32, sustainLen u32, attackCount u16, releaseCount u16,
      envelopes (attackCount + releaseCount) x (duration u16, volume u16),
      the samples: (attackLen + sustainLen) * (2^octaves - 1) bytes
  tools/music/mx_info.py FILE.MX
"""
import struct
import sys


def read(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'MXTX', 'not MaxTrax'
    tempo, flags = struct.unpack('>HH', d[4:8])
    p = 8
    if flags & 1:
        p += 256
    n, = struct.unpack('>H', d[p:p + 2]); p += 2
    scores = []
    for _ in range(n):
        ne, = struct.unpack('>I', d[p:p + 4]); p += 4
        ev = [struct.unpack('>BBHH', d[p + 6 * i:p + 6 * i + 6]) for i in range(ne)]
        p += 6 * ne
        scores.append(ev)
    ns, = struct.unpack('>H', d[p:p + 2]); p += 2
    samples = []
    for _ in range(ns):
        num, tune, vol, octs, alen, slen, ac, rc = struct.unpack('>HHHHIIHH', d[p:p + 20]); p += 20
        p += 4 * (ac + rc)
        size = (alen + slen) * ((1 << octs) - 1)
        samples.append((num, tune, vol, octs, alen, slen, ac, rc, size))
        p += size
    return tempo, flags, scores, samples, p, len(d)


def main():
    tempo, flags, scores, samples, end, size = read(sys.argv[1])
    print('tempo %d flags %d scores %d samples %d (parsed %d of %d bytes)' % (tempo, flags, len(scores), len(samples), end, size))
    for i, ev in enumerate(scores):
        notes = [e for e in ev if e[0] < 0x80]
        cmds = sorted(set(e[0] for e in ev if e[0] >= 0x80))
        total = sum(e[2] for e in ev)	# start times are deltas
        print('score %2d: %5d events, %5d notes, delta sum %6d, commands %s' % (
            i, len(ev), len(notes), total, ' '.join('%02X' % c for c in cmds)))
    for s in samples:
        print('sample %2d tune %d vol %d octaves %d attack %d sustain %d env %d+%d bytes %d' % s)


if __name__ == '__main__':
    main()
