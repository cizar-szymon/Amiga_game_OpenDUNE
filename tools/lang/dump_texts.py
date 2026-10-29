"""Dump the texts of one language of the player's Dune II data in the format
of the translation sources (tools/lang/polish/*.txt), as a template or for a
side by side review. Output goes to build/ - game text, never committed.

  python tools/lang/dump_texts.py DATADIR [GERDIR] SUFFIX OUTDIR
  e.g. python tools/lang/dump_texts.py data - ENG build/lang/eng
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dune_lang import DataDir, STRING_FILES, read_strings, mentat_read  # noqa: E402

ENC = 'cp437'


def text(b):
    return b.decode(ENC).replace('\r', '\\r').replace('\n', '\\n').replace('\x0c', '\\f')


def main():
    data, ger, suffix, out = sys.argv[1:5]
    dd = DataDir(data, None if ger == '-' else ger)
    os.makedirs(out, exist_ok=True)
    for name, compressed, _, _ in STRING_FILES:
        b = dd.get(name + '.' + suffix)
        if b is None:
            continue
        with open(os.path.join(out, name + '.txt'), 'w', encoding='utf-8') as f:
            for i, s in enumerate(read_strings(b, compressed)):
                f.write('%d = %s\n' % (i, text(s)))
    for h in 'AHO':
        b = dd.get('MENTAT%s.%s' % (h, suffix))
        if b is None:
            continue
        subjects, _, _ = mentat_read(b)
        with open(os.path.join(out, 'MENTAT%s.txt' % h), 'w', encoding='utf-8') as f:
            for i, s in enumerate(subjects):
                f.write('[%d] %s%s %d\n' % (i, s['b5'].decode(), s['b6'].decode(), s['campaign']))
                f.write('title = %s\n' % text(s['title']))
                f.write('text = %s\n\n' % text(s['text']))
    b = dd.get('CREDITS.' + suffix)
    if b is not None:
        with open(os.path.join(out, 'CREDITS.txt'), 'w', encoding='utf-8') as f:
            f.write(b.decode(ENC))
    for h in 'AHO':
        b = dd.get('REGION%s.INI' % h)
        if b is None:
            continue
        with open(os.path.join(out, 'REGION%s.txt' % h), 'w', encoding='utf-8') as f:
            group = None
            for line in b.decode(ENC).splitlines():
                line = line.strip()
                if line.startswith('[GROUP'):
                    group = line
                    f.write(line + '\n')
                elif line.upper().startswith(suffix + 'TXT') and group:
                    k, v = line.split('=', 1)
                    f.write('%s = %s\n' % (k.strip()[len(suffix):], v.strip()))


main()
