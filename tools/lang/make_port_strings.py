"""The port's own texts in German and Polish: tools/lang/port_strings.txt ->
port/src/lang_strings.h (lang_amiga.c).

  python tools/lang/make_port_strings.py [--check]

Checks that each translation has the same printf conversions as the English
text and every Lang_T("...") of port/src has an entry. German is written in
the code page of the game's fonts (CP437: the umlauts of NEW6PG.FNT and
NEW8P.FNT), Polish in ISO-8859-2 (port/src/gui/font_polish.c).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, 'port', 'src')
TXT = os.path.join(HERE, 'port_strings.txt')
OUT = os.path.join(SRC, 'lang_strings.h')

# texts given to Lang_T() through a variable (the code passes them on)
INDIRECT = [
    'LOAD', 'SAVE', 'LOAD/SAVE',
    'Raising the clock overclocks', 'the CPU - the system may hang.',
    'The clock changes at once.', 'MC68040: 20 to 50 MHz.', 'Only for a MC68060 rev. 6.',
    'Not an address.', 'The server is full.', 'No answer from the server.', 'Bad data.',
]

ENCODING = {'de': 'cp437', 'pl': 'iso8859_2'}
CONV = re.compile(r'%[-+ #0-9.]*l?[diouxXcsp%]')


def parse(path=TXT):
    """The entries: list of dicts with en, de, pl, max, px (and the line)."""
    entries = []
    cur = None
    for n, line in enumerate(open(path, encoding='utf-8').read().split('\n'), 1):
        if line.startswith('#'):
            continue
        if line.strip() == '':
            cur = None
            continue
        key, sep, value = line.partition(': ')
        if not sep or key not in ('en', 'de', 'pl', 'max', 'px'):
            raise SystemExit('%s:%d: not "en: ..."' % (path, n))
        if key == 'en':
            cur = {'en': value, 'line': n}
            entries.append(cur)
        elif cur is None:
            raise SystemExit('%s:%d: %s: without en:' % (path, n, key))
        elif key == 'max':
            cur['max'] = int(value)
        elif key == 'px':
            w, font = value.split()
            cur['px'] = (int(w), int(font))
        else:
            cur[key] = value
    return entries


def encode(text, lang):
    return text.encode(ENCODING[lang]) if lang in ENCODING else text.encode('ascii')


def c_string(b):
    out = '"'
    for c in b:
        if c == 0x22 or c == 0x5C:
            out += '\\' + chr(c)
        elif 0x20 <= c < 0x7F:
            out += chr(c)
        else:
            out += '\\%03o' % c	# octal: never runs into the next letter
    return out + '"'


def code_keys():
    """The texts of Lang_T("...") in port/src."""
    keys = {}
    lit = re.compile(r'Lang_T\("((?:[^"\\]|\\.)*)"\)')
    for d, _, files in os.walk(SRC):
        for f in files:
            if f.endswith('.c'):
                p = os.path.join(d, f)
                for n, line in enumerate(open(p, encoding='latin-1'), 1):
                    for m in lit.finditer(line):
                        keys.setdefault(m.group(1), '%s:%d' % (os.path.relpath(p, ROOT), n))
    for k in INDIRECT:
        keys.setdefault(k, '(indirect)')
    return keys


def check(entries):
    errors = []
    seen = set()
    for e in entries:
        if e['en'] in seen:
            errors.append('line %d: "%s" twice' % (e['line'], e['en']))
        seen.add(e['en'])
        for lang in ('de', 'pl'):
            if lang not in e:
                continue
            if CONV.findall(e['en']) != CONV.findall(e[lang]):
                errors.append('line %d: %s "%s": not the conversions of "%s"' % (e['line'], lang, e[lang], e['en']))
            try:
                b = encode(e[lang], lang)
            except UnicodeEncodeError as x:
                errors.append('line %d: %s "%s": %s' % (e['line'], lang, e[lang], x))
                continue
            if 'max' in e and len(b) > e['max']:
                errors.append('line %d: %s "%s": %d bytes, at most %d' % (e['line'], lang, e[lang], len(b), e['max']))
    keys = code_keys()
    for k, where in sorted(keys.items()):
        if k not in seen:
            errors.append('%s: Lang_T("%s") has no entry' % (where, k))
    for e in entries:
        if e['en'] not in keys:
            errors.append('line %d: "%s" is not used in the code' % (e['line'], e['en']))
    return errors


def write(entries):
    lines = [
        '/** @file src/lang_strings.h The port\'s own texts in German and Polish. */',
        '',
        '/* Made by tools/lang/make_port_strings.py from tools/lang/port_strings.txt:',
        ' * change them there. English, German (CP437), Polish (ISO-8859-2); NULL: English. */',
        '',
        'static const char * const s_langStrings[][3] = {',
    ]
    for e in entries:
        cols = [c_string(e['en'].encode('ascii'))]
        for lang in ('de', 'pl'):
            cols.append(c_string(encode(e[lang], lang)) if lang in e else 'NULL')
        lines.append('\t{ %s },' % ', '.join(cols))
    lines.append('};')
    lines.append('')
    data = '\n'.join(lines)
    old = open(OUT, encoding='latin-1').read() if os.path.exists(OUT) else None
    if old != data:
        with open(OUT, 'w', encoding='latin-1', newline='\n') as f:
            f.write(data)
        print('written', os.path.relpath(OUT, ROOT))
    else:
        print('unchanged', os.path.relpath(OUT, ROOT))


def main():
    entries = parse()
    errors = check(entries)
    for x in errors:
        print(x)
    if errors:
        sys.exit(1)
    print('%d texts, German %d, Polish %d' % (len(entries), sum('de' in e for e in entries), sum('pl' in e for e in entries)))
    if '--check' not in sys.argv:
        write(entries)


if __name__ == '__main__':
    main()
