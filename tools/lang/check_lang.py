"""Checks of the languages against the player's Dune II data (PC side, no
Amiga needed):

  python tools/lang/check_lang.py DATADIR [GERDIR]
  e.g. python tools/lang/check_lang.py data build/data_ger

German: the .GER files parsed the way port/src/string.c does (the offsets
little or big endian), the number of strings against the English ones; which
pictures the game takes in the German's place (String_GenerateGfxFilename).
Polish: POLISH.PAK made from tools/lang/polish (make_polish.py) loaded like
the game loads it; every English string must have a Polish one with the same
printf conversions and lines; texts of buttons and the sidebar not wider
than the English or German ones; the mentat's help and the map's texts
complete. The port's own texts: make_port_strings.py and their widths.
Writes build/lang/polish_review.txt (English | Polish, for reading; game
text, never committed). Exit code 1 on errors; widths are warnings.
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dune_lang import (DataDir, Font, load_string_ids, read_pak, mentat_read,  # noqa: E402
                       STRING_FILES)
import make_polish  # noqa: E402
import make_port_strings  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
CONV_RE = re.compile(r'%[-+ #0-9.]*l?[diouxXcsp]')


class _Conv:
    """The printf conversions of a text (%% is none)."""
    @staticmethod
    def findall(s):
        return CONV_RE.findall(s.replace('%%', ''))


CONV = _Conv()
# the strings of DUNE.ENG in places of a fixed size: the unit's commands,
# buttons, the sidebar's panels, Game controls, the names under the pictures
# (the short ones), the starport, the Hall of Fame
CONSTRAINED = (set(range(1, 15)) | set(range(30, 43)) | {49} | set(range(84, 90)) | set(range(105, 118)) |
               {120, 142, 143, 144, 146, 153, 160, 161} | set(range(168, 181)) | set(range(188, 194)) |
               set(range(194, 270, 2)) | {323, 324, 325, 326, 327, 329, 330, 335})
# room known better than "as the English or German one": the unit's four
# command buttons (as wide as the widest command, German "Hinterhalt"), the
# name over the sidebar's picture (64 pixels), the buttons of Game controls
# (80 pixels)
ROOM = {}
for _i in range(1, 13):
    ROOM[_i] = 'commands'
for _i in list(range(168, 175)) + list(range(194, 270, 2)):
    ROOM[_i] = 62
for _i in (105, 106, 113, 114, 115, 116, 117):
    ROOM[_i] = 76
errors = []
warnings = []


def err(s):
    errors.append(s)


def warn(s):
    warnings.append(s)


def c_is_big_endian(b):
    """String_IsBigEndian of port/src/string.c."""
    size = len(b)
    le = struct.unpack_from('<H', b, 0)[0]
    be = struct.unpack_from('>H', b, 0)[0]
    if le >= 2 and le % 2 == 0 and le <= size and struct.unpack_from('<H', b, le - 2)[0] < size:
        return False
    return be >= 2 and be % 2 == 0 and be <= size and struct.unpack_from('>H', b, be - 2)[0] < size


def gfx_is_pc(b, image):
    """String_GenerateGfxFilename's test of a picture file."""
    if b is None or len(b) < 8:
        return False
    if image:
        return struct.unpack_from('<I', b, 4)[0] == 64000
    n = struct.unpack_from('<H', b, 0)[0]
    return n != 0 and n < 0x100


class Widths:
    def __init__(self, dd):
        self.f6 = Font(dd.get('NEW6P.FNT'))
        self.f6g = Font(dd.get('NEW6PG.FNT')) if dd.get('NEW6PG.FNT') else self.f6
        self.f8 = Font(dd.get('NEW8P.FNT'))

    def width(self, b, font):
        """Font_GetStringWidth with g_fontCharOffset -1 (style 0x?2 / 0x?1),
        the widest line. Polish letters: as wide as their drawings."""
        best = 0
        for line in b.split(b'\r'):
            w = 0
            for c in line:
                ch = font.chars[c] if c < font.count else None
                cw = ch['width'] if ch else POLISH_WIDTH.get((font.height, c), 0)
                if c in POLISH_WIDTH_CODES and (font.height, c) in POLISH_WIDTH:
                    cw = POLISH_WIDTH[(font.height, c)]
                w += max(cw - 1, 0)
            best = max(best, w)
        return best


POLISH_WIDTH = {}
POLISH_WIDTH_CODES = set()


def load_polish_widths():
    from render_fonts import parse_glyphs
    g = parse_glyphs()
    for code, base, rows in g[6]:
        POLISH_WIDTH[(8, code)] = len(rows[0])
        POLISH_WIDTH_CODES.add(code)
    for small, capital in g['small']:
        POLISH_WIDTH[(8, small)] = POLISH_WIDTH[(8, capital)]
        POLISH_WIDTH_CODES.add(small)
    for code, base, rows in g[8]:
        POLISH_WIDTH[(11, code)] = len(rows[0])
        POLISH_WIDTH_CODES.add(code)


def check_german(dd, report):
    report.append('== German (.GER)')
    try:
        eng, ce = load_string_ids(dd.get, 'ENG')
        ger, cg = load_string_ids(dd.get, 'GER')
    except FileNotFoundError as x:
        report.append('  no German files: %s' % x)
        return
    for name, _, _, _ in STRING_FILES:
        b = dd.get(name + '.GER')
        report.append('  %-8s %s, %3d strings (English %3d)' % (
            name + '.GER', 'big endian (Amiga)' if c_is_big_endian(b) else 'little endian (PC)', cg[name], ce[name]))
        if c_is_big_endian(dd.get(name + '.ENG')):
            err('%s.ENG taken as big endian' % name)
    n_eng = sum(1 for s in eng if s)
    n_ger = sum(1 for s in ger if s)
    report.append('  string IDs: English %d (%d with text), German %d (%d with text)' % (len(eng), n_eng, len(ger), n_ger))
    # (fewer: the copy protection has fewer questions in German, the IDs
    # before it, 0..524, are the same)
    if len(ger) < 525:
        err('German: %d string IDs, English %d' % (len(ger), len(eng)))
    for i, (e, g) in enumerate(zip(eng, ger)):
        if i >= 525:
            break	# the copy protection: other questions
        if CONV.findall(e.decode('cp437')) != CONV.findall(g.decode('cp437')):
            warn('German string %d: conversions %s, English %s' % (i, CONV.findall(g.decode('cp437')), CONV.findall(e.decode('cp437'))))
    for h in 'AHO':
        b = dd.get('MENTAT%s.GER' % h)
        if b is None:
            continue
        s, _, _ = mentat_read(b)
        report.append('  MENTAT%s.GER: IFF, %d subjects (English %d)' % (h, len(s), len(mentat_read(dd.get('MENTAT%s.ENG' % h))[0])))
    b = dd.get('CREDITS.GER')
    if b is not None:
        report.append('  CREDITS.GER: %s' % ('text as the PC one' if b[:1] == b'\x02' else 'not the PC format'))
    for name, image in (('BTTN', False), ('CHOAM', False), ('MENTAT', False), ('AND', True), ('HERALD', True), ('MISC', True), ('TITLE', True)):
        b = dd.get(name + '.GER')
        if b is None:
            continue
        report.append('  %-10s %s' % (name + '.GER', 'PC format: used' if gfx_is_pc(b, image) else 'Amiga format: %s.ENG used instead' % name))
    report.append('  NEW6PG.FNT (the German 6 pixel font): %s' % ('there' if dd.get('NEW6PG.FNT') else 'missing'))


def check_polish(dd, w, report, review):
    report.append('== Polish (POLISH.PAK from tools/lang/polish)')
    pak = dict(make_polish.build())
    files = dict(dd.files)
    files.update(pak)

    def get(name):
        name = name.upper()
        if name.endswith('.POL') and name not in files:
            name = name[:-4] + '.ENG'	# String_GenerateFilename
        return files.get(name)
    eng, _ = load_string_ids(dd.get, 'ENG')
    pol, _ = load_string_ids(get, 'POL')
    try:
        ger, _ = load_string_ids(dd.get, 'GER')
    except FileNotFoundError:
        ger = None
    total = sum(len(s) + 1 for s in pol)
    report.append('  string IDs: %d (English %d), %d bytes (room for %d)' % (len(pol), len(eng), total, 60 * 1024))
    if len(pol) != len(eng):
        err('Polish: %d string IDs, English %d' % (len(pol), len(eng)))
    if total > 60 * 1024:
        err('Polish: %d bytes of strings, MAX_CHARACTER_COUNT 60K' % total)
    missing = 0
    for i, (e, p) in enumerate(zip(eng, pol)):
        if i >= 525:
            break
        es, ps = e.decode('cp437'), p.decode('iso8859_2')
        review.append('%d\n  EN %s\n  PL %s' % (i, es.replace('\r', ' | '), ps.replace('\r', ' | ')))
        if es and not ps:
            missing += 1
            err('Polish: no string %d (%s)' % (i, es[:50]))
            continue
        if ps == es and re.search('[a-z]{3}', es) and i not in (10, 37, 43, 196, 224, 228, 242, 244, 246):
            warn('Polish string %d same as English: %s' % (i, es[:50]))
        if CONV.findall(es) != CONV.findall(ps):
            err('Polish string %d: conversions %s, English %s' % (i, CONV.findall(ps), CONV.findall(es)))
        if es.count('\r') != ps.count('\r'):
            warn('Polish string %d: %d lines, English %d' % (i, ps.count('\r') + 1, es.count('\r') + 1))
        # buttons, the sidebar, labels: as wide as the wider of the English
        # and the German one at most (the German texts fit where the game
        # draws them)
        if i in CONSTRAINED:
            gs = ger[i] if ger is not None and i < len(ger) else b''
            for font, fg, name in ((w.f6, w.f6g, '6'), (w.f8, w.f8, '8')):
                limit = max(w.width(e, font), w.width(gs, fg))
                if ROOM.get(i) == 'commands':
                    limit = max(max(w.width(eng[k], font), w.width(ger[k] if ger else b'', fg)) for k in range(1, 13))
                elif i in ROOM:
                    limit = max(limit, ROOM[i])
                wp = w.width(p, font)
                if wp > limit + 2:
                    warn('Polish string %d wider in the %sp font: %d pixels, English/German %d: "%s" / "%s" / "%s"' % (
                        i, name, wp, limit, ps.replace('\r', '|'), es.replace('\r', '|'), gs.decode('cp437').replace('\r', '|')))
    report.append('  missing translations: %d' % missing)
    for h in 'AHO':
        se, _, _ = mentat_read(dd.get('MENTAT%s.ENG' % h))
        sp, _, _ = mentat_read(pak['MENTAT%s.POL' % h])
        if len(se) != len(sp):
            err('MENTAT%s: %d subjects, English %d' % (h, len(sp), len(se)))
        for i, (a, b) in enumerate(zip(se, sp)):
            if (a['b5'], a['b6'], a['campaign']) != (b['b5'], b['b6'], b['campaign']):
                err('MENTAT%s subject %d: flags %s, English %s' % (h, i, (b['b5'], b['b6'], b['campaign']), (a['b5'], a['b6'], a['campaign'])))
            wa = re.split(rb'[*?]', a['text'], maxsplit=1)[0]
            wb = re.split(rb'[*?]', b['text'], maxsplit=1)[0]
            if wa != wb:
                err('MENTAT%s subject %d: animation %r, English %r' % (h, i, wb, wa))
            if a['text'].count(b'\x0c') != b['text'].count(b'\x0c'):
                err('MENTAT%s subject %d: no \\f between caption and text' % (h, i))
            review.append('MENTAT%s %d\n  EN %s: %s\n  PL %s: %s' % (h, i, a['title'].decode('cp437'), a['text'].decode('cp437'),
                                                                    b['title'].decode('iso8859_2'), b['text'].decode('iso8859_2')))
        report.append('  MENTAT%s.POL: %d subjects (English %d)' % (h, len(sp), len(se)))
    for h in 'AHO':
        ini = dd.get('REGION%s.INI' % h).decode('cp437')
        keys_e = set()
        group = None
        for line in ini.splitlines():
            line = line.strip()
            if line.startswith('[GROUP'):
                group = line
            elif line.upper().startswith('ENGTXT') and group:
                keys_e.add((group, line.split('=')[0].strip()[3:]))
        keys_p = set()
        group = None
        for line in pak['REGION%s.POL' % h].decode('iso8859_2').splitlines():
            if line.startswith('[GROUP'):
                group = line
            elif line.startswith('POLTXT'):
                keys_p.add((group, line.split('=')[0][3:]))
        if keys_e != keys_p:
            err('REGION%s.POL: keys %s missing, %s extra' % (h, sorted(keys_e - keys_p), sorted(keys_p - keys_e)))
        report.append('  REGION%s.POL: %d texts of the map (English %d)' % (h, len(keys_p), len(keys_e)))


def check_port(w, report):
    report.append('== The port\'s own texts (tools/lang/port_strings.txt)')
    entries = make_port_strings.parse()
    for x in make_port_strings.check(entries):
        err('port_strings: ' + x)
    for e in entries:
        if 'px' not in e:
            continue
        limit, size = e['px']
        for lang in ('en', 'de', 'pl'):
            if lang not in e:
                continue
            b = make_port_strings.encode(e[lang], lang) if lang != 'en' else e[lang].encode('ascii')
            font = w.f8 if size == 8 else (w.f6g if lang == 'de' else w.f6)
            px = w.width(CONV_RE.sub('00', b.decode('latin-1')).encode('latin-1'), font)
            if px > limit:
                warn('port text %s "%s": %d pixels, room for %d' % (lang, e[lang], px, limit))
    report.append('  %d texts' % len(entries))


def find_gcc():
    for p in (os.environ.get('HOST_GCC'), r'C:\msys64\ucrt64\bin\gcc.exe', r'C:\msys64\mingw64\bin\gcc.exe'):
        if p and os.path.exists(p):
            return p
    import shutil
    return shutil.which('gcc') or shutil.which('cc')


def check_host(dd, report):
    """The game's C code (string.c, font.c, font_polish.c) built for the PC
    (host_check.c) on the same files: its strings and Polish letters must
    be those of the Python model above."""
    import subprocess
    from render_fonts import parse_glyphs, emboss
    report.append('== The game\'s code on the PC (tools/lang/host_check.c)')
    gcc = find_gcc()
    if gcc is None:
        report.append('  no gcc (HOST_GCC=...): skipped')
        return
    out = os.path.join(ROOT, 'build', 'lang')
    exe = os.path.join(out, 'host_check.exe' if os.name == 'nt' else 'host_check')
    env = dict(os.environ)
    env['PATH'] = os.path.dirname(gcc) + os.pathsep + env.get('PATH', '')
    r = subprocess.run([gcc, '-std=gnu99', '-Wall', '-Wno-pointer-sign', '-Wno-unused-function', '-DNDEBUG',
                        '-iquote', os.path.join(ROOT, 'port', 'src'), '-I', os.path.join(ROOT, 'port', 'include'),
                        '-o', exe, os.path.join(HERE, 'host_check.c')], capture_output=True, text=True, env=env)
    if r.returncode != 0:
        err('host_check.c does not build: ' + r.stderr[:500])
        return
    pak = dict(make_polish.build())
    glyphs = parse_glyphs()
    for lang, suffix in ((0, 'ENG'), (2, 'GER'), (5, 'POL')):
        d = os.path.join(out, 'host_' + suffix)
        os.makedirs(d, exist_ok=True)
        for f in os.listdir(d):
            os.remove(os.path.join(d, f))
        files = {k: v for k, v in dd.files.items() if k.endswith('.ENG') or k.endswith('.FNT')}
        if suffix == 'GER':
            files.update({k: v for k, v in dd.files.items() if k.endswith('.GER')})
        if suffix == 'POL':
            files.update(pak)
        if not any(k.endswith('.' + suffix) for k in files):
            continue
        for k, v in files.items():
            with open(os.path.join(d, k), 'wb') as f:
                f.write(v)
        dump = os.path.join(d, '_dump.txt')
        r = subprocess.run([exe, d, str(lang), dump], capture_output=True, env=env)
        if r.returncode != 0:
            err('host_check %s: exit %d' % (suffix, r.returncode))
            continue
        b = open(dump, 'rb').read()
        for f in os.listdir(d):	# (copies of game data: not left lying around)
            os.remove(os.path.join(d, f))
        os.rmdir(d)
        # the strings
        lines = b.split(b'\n')
        count = int(lines[0].split()[1])

        def get(name, files=files):
            name = name.upper()
            if name not in files:
                name = name[:-4] + '.ENG'
            return files.get(name)
        model, _ = load_string_ids(get, suffix)
        c_strings = []
        p = len(lines[0]) + 1
        for i in range(count):
            e = b.index(b' ', b.index(b' ', p) + 1)
            n = int(b[b.index(b' ', p) + 1:e])
            c_strings.append(b[e + 1:e + 1 + n])
            p = e + 1 + n + 1
        same = c_strings == model
        if not same:
            diff = [i for i, (x, y) in enumerate(zip(c_strings, model)) if x != y][:5]
            err('host %s: the C code gives %d strings, the model %d, different: %s' % (suffix, count, len(model), diff))
        rest = b[p:].decode('latin-1').split('\n')
        report.append('  %s: string.c %d strings, %s the Python model; %s; %s' % (
            suffix, count, 'the same as' if same else 'NOT the same as', rest[0], rest[1]))
        # the Polish letters
        if suffix == 'POL':
            fonts = {}
            for line in rest[2:]:
                parts = line.split()
                if not parts:
                    continue
                if parts[0] == 'font':
                    cur = fonts.setdefault(parts[1], {})
                    if parts[3] != '256':
                        err('host: font %s has %s chars, not 256' % (parts[1], parts[3]))
                elif parts[0] == 'char':
                    cur[int(parts[1], 16)] = (int(parts[2]), int(parts[3]), int(parts[4]), parts[5] if len(parts) > 5 else '')
            bad = 0
            for key, name in ((6, '6p'), (8, '8p')):
                table = {code: emboss(rows) for code, _, rows in glyphs[key]}
                if key == 6:
                    for small, capital in glyphs['small']:
                        table[small] = table[capital]
                for code, rows in table.items():
                    nz = [y for y, r in enumerate(rows) if any(r)]
                    first, last = nz[0], nz[-1]
                    want = (len(rows[0]), first, last + 1 - first, ''.join(str(v) for r in rows[first:last + 1] for v in r))
                    if fonts.get(name, {}).get(code) != want:
                        bad += 1
                        err('host: %s letter %02X differs from the drawing' % (name, code))
            report.append('  Polish letters in NEW6P and NEW8P (Font_AddPolish): %d of 36 as drawn' % (36 - bad))


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    data = sys.argv[1]
    ger = sys.argv[2] if len(sys.argv) > 2 else None
    dd = DataDir(data, ger)
    load_polish_widths()
    w = Widths(dd)
    report, review = [], []
    check_german(dd, report)
    check_polish(dd, w, report, review)
    check_port(w, report)
    check_host(dd, report)
    out = os.path.join(ROOT, 'build', 'lang')
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'polish_review.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(review) + '\n')
    print('\n'.join(report))
    for x in warnings:
        print('warning: ' + x)
    for x in errors:
        print('ERROR: ' + x)
    print('%d errors, %d warnings; side by side: build/lang/polish_review.txt' % (len(errors), len(warnings)))
    sys.exit(1 if errors else 0)


if __name__ == '__main__':
    main()
