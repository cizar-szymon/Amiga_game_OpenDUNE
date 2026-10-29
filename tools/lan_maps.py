#!/usr/bin/env python3
"""Maps for the LAN game (port/src/lan_maps.h) from Dune II's SCENARIO.PAK.

Every scenario with a 64x64 map (MapScale 0) and at least three bases (houses
with a Construction Yard) gives a LAN map: its landscape (Seed, Field, Bloom)
and, for three of its bases, the Construction Yard, a Windtrap, a Refinery
and some of the base's units - the start of one player each.

  python tools/lan_maps.py data/SCENARIO.PAK > port/src/lan_maps.h
"""
import struct
import sys


def pak_files(data):
    """name -> bytes of a Dune II PAK (offset, name, ..., 0)."""
    entries = []
    pos = 0
    while True:
        off = struct.unpack_from('<I', data, pos)[0]
        if off == 0:
            break
        end = data.index(b'\0', pos + 4)
        entries.append((data[pos + 4:end].decode('latin-1'), off))
        pos = end + 1
    files = {}
    for i, (name, off) in enumerate(entries):
        nxt = entries[i + 1][1] if i + 1 < len(entries) else len(data)
        files[name] = data[off:nxt]
    return files


def ini(text):
    sections, cur = {}, None
    for line in text.splitlines():
        line = line.split(';', 1)[0].strip()
        if not line:
            continue
        if line.startswith('[') and line.endswith(']'):
            cur = line[1:-1].upper()
            sections.setdefault(cur, [])
        elif cur and '=' in line:
            k, v = line.split('=', 1)
            sections[cur].append((k.strip(), v.strip()))
    return sections


def value(sec, key, default=None):
    for k, v in sec:
        if k.upper() == key.upper():
            return v
    return default


UNIT_KEEP = ('Trike', 'Quad', 'Tank', 'Siege Tank', 'Launcher', 'Infantry', 'Soldier', 'Troopers', 'Trooper')


def bases(sections):
    """house -> {'cy': pos, 'wind': pos, 'refinery': pos, 'units': [(type, pos)]}"""
    out = {}
    for k, v in sections.get('STRUCTURES', []):
        if not k.upper().startswith('ID'):
            continue
        f = [x.strip() for x in v.split(',')]
        if len(f) < 4:
            continue
        house, typ, pos = f[0], f[1], int(f[3])
        b = out.setdefault(house, {'count': 0, 'units': []})
        b['count'] += 1
        if typ == 'Const Yard' and 'cy' not in b:
            b['cy'] = pos
        elif typ == 'Windtrap' and 'wind' not in b:
            b['wind'] = pos
        elif typ == 'Refinery' and 'refinery' not in b:
            b['refinery'] = pos
    for k, v in sections.get('UNITS', []):
        f = [x.strip() for x in v.split(',')]
        if len(f) < 4:
            continue
        house, typ, pos = f[0], f[1], int(f[3])
        if house in out and typ in UNIT_KEEP and len(out[house]['units']) < 6:
            out[house]['units'].append((typ, pos))
    return {h: b for h, b in out.items() if 'cy' in b}


def main():
    files = pak_files(open(sys.argv[1], 'rb').read())
    maps = []
    seen = set()
    for name in sorted(files):
        if not name.upper().startswith('SCEN') or not name.upper().endswith('.INI'):
            continue
        s = ini(files[name].decode('latin-1'))
        if value(s.get('BASIC', []), 'MapScale', '0') != '0':
            continue
        seed = value(s.get('MAP', []), 'Seed')
        if seed is None or seed in seen:
            continue
        b = bases(s)
        if len(b) < 3:
            continue
        seen.add(seed)
        # the three largest bases
        three = sorted(b.items(), key=lambda kv: -kv[1]['count'])[:3]
        maps.append((name, seed, value(s['MAP'], 'Field', ''), value(s['MAP'], 'Bloom', ''), three))

    # bases at least 20 tiles apart (one map of each landscape)
    good = []
    for name, seed, field, bloom, three in maps:
        pts = [(b['cy'] % 64, b['cy'] // 64) for h, b in three]
        d = min(abs(p[0] - q[0]) + abs(p[1] - q[1]) for i, p in enumerate(pts) for q in pts[i + 1:])
        if d >= 20:
            good.append((name, seed, field, bloom, three))

    print('/* LAN maps from SCENARIO.PAK (tools/lan_maps.py): the landscape of a')
    print(' * scenario and three of its bases far apart; each player starts at one with')
    print(' * a Construction Yard and units where the base had units (else next to it). */')
    print('typedef struct LanMap { const char *name; uint16 seed; const char *field, *bloom; uint16 cy[3]; uint16 units[3][4]; } LanMap;')
    print('static const LanMap s_lanMaps[] = {')
    for name, seed, field, bloom, three in good:
        cys, units = [], []
        for house, b in three:
            cys.append(str(b['cy']))
            pos = [p for t, p in b['units']][:4]
            # next to the yard (two rows below it, and right of it)
            for off in (130, 132, 2 + 64, 2):
                if len(pos) >= 4:
                    break
                pos.append(b['cy'] + off)
            units.append('{ %s }' % ', '.join(str(p) for p in pos))
        print('	{ "%s", %s, "%s", "%s", { %s }, { %s } },' % (name[:8], seed, field, bloom, ', '.join(cys), ', '.join(units)))
    print('};')


if __name__ == '__main__':
    main()
