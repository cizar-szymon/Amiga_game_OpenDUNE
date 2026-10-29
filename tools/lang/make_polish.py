"""The Polish translation: tools/lang/polish/*.txt -> POLISH.PAK, the file the
game loads from PROGDIR:lang (port/src/file.c, string.c).

  python tools/lang/make_polish.py [OUT.PAK]      (default build/lang/POLISH.PAK)

Needs no game data: the files are written in the layout of the English ones
(DUNE, MESSAGE, INTRO uncompressed, TEXT? with the escapes of the compressed
files, the mentat help as IFF FORM MENT, the map texts as ini files, the
credits). The texts are ISO-8859-2 (the letters of port/src/gui/font_polish.c).
PROTECT (the copy protection's questions about the English manual) is not
translated: the game takes PROTECT.ENG.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from dune_lang import write_strings, mentat_write, write_pak  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(HERE, 'polish')

# the string files: name, compressed
STRING_FILES = [('DUNE', False), ('MESSAGE', False), ('INTRO', False),
                ('TEXTH', True), ('TEXTA', True), ('TEXTO', True)]

POLISH = 'ąćęłńóśźżĄĆĘŁŃÓŚŹŻ'


def encode(text, where):
    """ISO-8859-2 bytes; only ASCII and the Polish letters (the fonts have no others)."""
    for c in text:
        if not (0x20 <= ord(c) < 0x7F or c in POLISH or c in '\r\n\x0c'):
            raise SystemExit('%s: "%s" cannot be drawn by the game\'s fonts' % (where, c))
    return text.encode('iso8859_2')


def unescape(s):
    return s.replace('\\r', '\r').replace('\\f', '\x0c').replace('\\n', '\n')


def lines(name):
    path = os.path.join(SRC, name)
    for n, line in enumerate(open(path, encoding='utf-8').read().split('\n'), 1):
        yield n, line


def read_strings(name):
    """{index: text} of a string file's source."""
    out = {}
    for n, line in lines(name + '.txt'):
        if line.startswith('#') or line.strip() == '':
            continue
        m = re.match(r'^(\d+) = ?(.*)$', line)
        if not m:
            raise SystemExit('%s.txt:%d: not "N = text"' % (name, n))
        i = int(m.group(1))
        if i in out:
            raise SystemExit('%s.txt:%d: %d twice' % (name, n, i))
        out[i] = unescape(m.group(2))
    return out


def string_file(name, compressed):
    texts = read_strings(name)
    last = max(texts)
    strings = []
    for i in range(last + 1):
        t = texts.get(i, '')
        # the game skips empty strings (string.c String_Load): a gap would move
        # every string after it to another ID
        if t == '' and i != 0:
            raise SystemExit('%s.txt: no text for %d' % (name, i))
        strings.append(encode(t, '%s.txt %d' % (name, i)))
    strings.append(b'')	# the empty string at the end, as the English files
    return write_strings(strings, compressed)


def read_mentat(name):
    subjects = []
    cur = None
    for n, line in lines(name + '.txt'):
        if line.startswith('#') or line.strip() == '':
            continue
        m = re.match(r'^\[(\d+)\] (.)(.) (\d+)$', line)
        if m:
            if int(m.group(1)) != len(subjects):
                raise SystemExit('%s.txt:%d: subject %s out of order' % (name, n, m.group(1)))
            cur = {'b5': m.group(2).encode(), 'b6': m.group(3).encode(), 'campaign': int(m.group(4))}
            subjects.append(cur)
            continue
        key, sep, value = line.partition(' = ')
        if cur is None or not sep or key not in ('title', 'text'):
            raise SystemExit('%s.txt:%d: not [n] / title = / text =' % (name, n))
        cur[key] = encode(unescape(value), '%s.txt:%d' % (name, n))
    for i, s in enumerate(subjects):
        if 'title' not in s or 'text' not in s:
            raise SystemExit('%s.txt: subject %d without title or text' % (name, i))
        t = s['text']
        if b'*' not in t and b'?' not in t:
            raise SystemExit('%s.txt: subject %d: no animation (WSA*)' % (name, i))
    return mentat_write(subjects)


def region_file(name):
    """REGION?.POL: the POLTXT keys of the map's texts, an ini file."""
    out = []
    for n, line in lines(name + '.txt'):
        if line.startswith('#') or line.strip() == '':
            continue
        if re.match(r'^\[GROUP\d+\]$', line):
            out.append(line)
            continue
        m = re.match(r'^TXT(\d+) = (.*)$', line)
        if not m:
            raise SystemExit('%s.txt:%d: not [GROUPn] or TXTn = text' % (name, n))
        if len(m.group(2)) > 42:
            raise SystemExit('%s.txt:%d: longer than 42 letters' % (name, n))
        out.append('POLTXT%s=%s' % (m.group(1), m.group(2)))
    return encode('\r\n'.join(out) + '\r\n', name)


def credits_file():
    body = []
    for n, line in lines('CREDITS.txt'):
        if line.startswith('#'):
            continue
        body.append(line)
    while body and body[-1] == '':
        body.pop()
    text = '\r'.join(body)
    # {n}: the control bytes 0..5, kept as they are
    out = b''
    for part in re.split(r'(\{\d+\})', text):
        m = re.match(r'^\{(\d+)\}$', part)
        out += bytes([int(m.group(1))]) if m else encode(part, 'CREDITS.txt')
    return out


def build():
    files = []
    for name, compressed in STRING_FILES:
        files.append((name + '.POL', string_file(name, compressed)))
    for h in 'AHO':
        files.append(('MENTAT%s.POL' % h, read_mentat('MENTAT' + h)))
    for h in 'AHO':
        files.append(('REGION%s.POL' % h, region_file('REGION' + h)))
    files.append(('CREDITS.POL', credits_file()))
    return files


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'lang', 'POLISH.PAK')
    files = build()
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    write_pak(out, files)
    print('written %s: %s' % (out, ', '.join('%s %d' % (n, len(b)) for n, b in files)))


if __name__ == '__main__':
    main()
