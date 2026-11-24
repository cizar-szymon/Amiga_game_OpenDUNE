#!/usr/bin/env python3
"""The hunks of an AmigaOS executable (type, memory, size) - to find data in it.
  tools/music/hunk_list.py FILE [--dump N OUT]   (--dump: the bytes of hunk N)
"""
import struct
import sys

NAMES = {0x3E9: 'CODE', 0x3EA: 'DATA', 0x3EB: 'BSS'}


def hunks(d):
    p = 0
    assert struct.unpack('>I', d[0:4])[0] == 0x3F3
    p = 4
    while struct.unpack('>I', d[p:p + 4])[0] != 0:	# resident library names
        p += 4 + 4 * struct.unpack('>I', d[p:p + 4])[0]
    p += 4
    count, first, last = struct.unpack('>III', d[p:p + 12]); p += 12
    sizes = []
    for _ in range(last - first + 1):
        s, = struct.unpack('>I', d[p:p + 4]); p += 4
        sizes.append(((s >> 30) & 3, (s & 0x3FFFFFFF) * 4))
    out = []
    n = 0
    while p < len(d) and n < count:
        t, = struct.unpack('>I', d[p:p + 4]); p += 4
        t &= 0x3FFFFFFF
        if t in (0x3E9, 0x3EA):
            l, = struct.unpack('>I', d[p:p + 4]); p += 4
            out.append((n, NAMES[t], sizes[n][0], l * 4, p))
            p += l * 4
        elif t == 0x3EB:
            l, = struct.unpack('>I', d[p:p + 4]); p += 4
            out.append((n, 'BSS', sizes[n][0], l * 4, None))
        elif t == 0x3EC:	# reloc32
            while True:
                c, = struct.unpack('>I', d[p:p + 4]); p += 4
                if c == 0:
                    break
                p += 4 + 4 * c
        elif t == 0x3F7 or t == 0x3FC:	# drel32 / reloc32short: 16-bit entries
            while True:
                c, = struct.unpack('>H', d[p:p + 2]); p += 2
                if c == 0:
                    break
                p += 2 + 2 * c
            if p & 2:
                p += 2
        elif t == 0x3F0:	# symbols
            while True:
                c, = struct.unpack('>I', d[p:p + 4]); p += 4
                if c == 0:
                    break
                p += 4 * (c & 0xFFFFFF) + 4
        elif t == 0x3F1:	# debug
            l, = struct.unpack('>I', d[p:p + 4]); p += 4 + l * 4
        elif t == 0x3F2:	# end
            n += 1
        else:
            raise ValueError('hunk type %x at %d' % (t, p - 4))
    return out


def main():
    d = open(sys.argv[1], 'rb').read()
    hs = hunks(d)
    for n, t, mem, size, off in hs:
        print('%3d %-4s %-4s %8d bytes  file offset %s' % (n, t, ['any', 'chip', 'fast', '?'][mem], size, off))
    if len(sys.argv) > 2 and sys.argv[2] == '--dump':
        n = int(sys.argv[3])
        h = [x for x in hs if x[0] == n][0]
        open(sys.argv[4], 'wb').write(d[h[4]:h[4] + h[3]])


if __name__ == '__main__':
    main()
