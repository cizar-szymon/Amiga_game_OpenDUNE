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


# Structures that stand on a 2x2 (or bigger) piece of rock: where one stood, a
# Construction Yard can stand (a base's place when the base has no yard).
SITE_STRUCTURES = ('Const Yard', 'Windtrap', 'Barracks', 'Light Fctry', 'Outpost', 'Spice Silo', 'Hi-Tech',
                   'IX', 'WOR', 'Refinery', 'Heavy Fctry', 'Repair', 'Palace', 'Starport')


def sites(sections):
    """The bases of a scenario: one place (packed) for each house's base."""
    out = {}
    for k, v in sections.get('STRUCTURES', []):
        if not k.upper().startswith('ID'):
            continue
        f = [x.strip() for x in v.split(',')]
        if len(f) < 4 or f[1] not in SITE_STRUCTURES:
            continue
        rank = SITE_STRUCTURES.index(f[1])
        if f[0] not in out or rank < out[f[0]][0]:
            out[f[0]] = (rank, int(f[3]))
    return [p for r, p in out.values()]


def dist(a, b):
    return max(abs(a % 64 - b % 64), abs(a // 64 - b // 64))


def best(places, n):
    """n places as far from each other as possible (the smallest distance largest)."""
    from itertools import combinations
    pick, far = None, -1
    for c in combinations(places, n):
        d = min(dist(a, b) for a, b in combinations(c, 2))
        if d > far:
            pick, far = c, d
    return pick, far


# The start of each mission level (1-9) for each house: the first scenario of the level.
LEVEL_SCENARIO = (1, 2, 5, 8, 11, 14, 17, 20, 22)
HOUSES = (('Harkonnen', 'H'), ('Atreides', 'A'), ('Ordos', 'O'))	# HOUSE_HARKONNEN = 0, ATREIDES, ORDOS
UNIT_ENUM = {'Infantry': 'UNIT_INFANTRY', 'Troopers': 'UNIT_TROOPERS', 'Soldier': 'UNIT_SOLDIER',
             'Trooper': 'UNIT_TROOPER', 'Launcher': 'UNIT_LAUNCHER', 'Deviator': 'UNIT_DEVIATOR', 'Tank': 'UNIT_TANK',
             'Siege Tank': 'UNIT_SIEGE_TANK', 'Devastator': 'UNIT_DEVASTATOR', 'Sonic Tank': 'UNIT_SONIC_TANK',
             'Trike': 'UNIT_TRIKE', 'Raider Trike': 'UNIT_RAIDER_TRIKE', 'Quad': 'UNIT_QUAD'}
LEVEL_UNITS_MAX = 12


def main():
    files = pak_files(open(sys.argv[1], 'rb').read())
    by_seed = {}
    for name in sorted(files):
        if not name.upper().startswith('SCEN') or not name.upper().endswith('.INI'):
            continue
        s = ini(files[name].decode('latin-1'))
        if value(s.get('BASIC', []), 'MapScale', '0') != '0':
            continue
        seed = value(s.get('MAP', []), 'Seed')
        if seed is None:
            continue
        m = by_seed.setdefault(seed, {'name': name[:8], 'field': value(s['MAP'], 'Field', ''),
                                      'bloom': value(s['MAP'], 'Bloom', ''), 'places': []})
        # the bases of every mission on this landscape (they differ between missions)
        for p in sites(s):
            if all(dist(p, q) >= 10 for q in m['places']):
                m['places'].append(p)

    maps = []
    for seed, m in by_seed.items():
        if len(m['places']) >= 3:
            three, far = best(m['places'], 3)
            if far >= 16:
                maps.append((3, m, seed, three))
                continue
        two, far = best(m['places'], 2)
        if two is not None and far >= 16:
            maps.append((2, m, seed, two + (two[0],)))
    maps.sort(key=lambda t: (-t[0], t[1]['name']))

    print('/* LAN maps and levels from SCENARIO.PAK (tools/lan_maps.py).')
    print(' * A map: the landscape of the missions on one 64x64 map and two or three')
    print(' * places of bases (of all those missions) far apart; each player starts at one')
    print(' * with a Construction Yard, its units around it (lan_amiga.c). */')
    print('typedef struct LanMap { const char *name; uint16 seed; const char *field, *bloom; uint8 players; uint16 cy[3]; } LanMap;')
    print('static const LanMap s_lanMaps[] = {')
    for players, m, seed, cy in maps:
        print('	{ "%s", %s, "%s", "%s", %d, { %s } },' % (m['name'], seed, m['field'], m['bloom'], players,
                                                     ', '.join(str(p) for p in cy)))
    print('};')
    print()
    print('/* A level: the start of mission 1..9 of each house (its first scenario):')
    print(' * credits and units; the game knows what mission N knows (g_campaignID). */')
    print('#define LAN_LEVEL_UNITS %d' % LEVEL_UNITS_MAX)
    print('typedef struct LanLevel { uint16 credits; uint8 count; uint8 units[LAN_LEVEL_UNITS]; } LanLevel;')
    print('static const LanLevel s_lanLevels[%d][3] = {' % len(LEVEL_SCENARIO))
    for level, number in enumerate(LEVEL_SCENARIO, 1):
        row = []
        for house, letter in HOUSES:
            s = ini(files['SCEN%s%03d.INI' % (letter, number)].decode('latin-1'))
            credits = int(value(s.get(house.upper(), []), 'Credits', '1000'))
            units = []
            for k, v in s.get('UNITS', []):
                f = [x.strip() for x in v.split(',')]
                if len(f) >= 2 and f[0] == house and f[1] in UNIT_ENUM:
                    units.append(UNIT_ENUM[f[1]])
            units = units[:LEVEL_UNITS_MAX]
            row.append('{ %d, %d, { %s } }' % (credits, len(units), ', '.join(units)))
        print('	/* %d */ { %s },' % (level, ', '.join(row)))
    print('};')


if __name__ == '__main__':
    main()
