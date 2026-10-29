"""Dune II language data: PAK files, string files, mentat help files, fonts.

Shared by the language tools (make_polish.py, check_lang.py, render_fonts.py).
Reads the game data the way port/src does (string.c, gui/mentat.c,
gui/font.c); never writes game data into the repository.
"""
import os
import struct

# ---- PAK ----

def read_pak(path):
    """{name: bytes} of a Dune II PAK file (LE offsets, names, 0)."""
    d = open(path, 'rb').read()
    ents = []
    p = 0
    while True:
        off = struct.unpack_from('<I', d, p)[0]
        if off == 0:
            break
        p += 4
        e = d.index(b'\0', p)
        ents.append((d[p:e].decode('latin-1'), off))
        p = e + 1
    out = {}
    for i, (n, o) in enumerate(ents):
        end = ents[i + 1][1] if i + 1 < len(ents) else len(d)
        out[n.upper()] = d[o:end]
    return out


def write_pak(path, files):
    """files: list of (name, bytes)."""
    header = 0
    for name, _ in files:
        header += 4 + len(name) + 1
    header += 4
    out = bytearray()
    pos = header
    for name, data in files:
        out += struct.pack('<I', pos) + name.encode('latin-1') + b'\0'
        pos += len(data)
    out += struct.pack('<I', 0)
    for _, data in files:
        out += data
    with open(path, 'wb') as f:
        f.write(out)


class DataDir:
    """The files of a Dune II data directory: loose files and PAK contents
    (as port/src/file.c: loose files first)."""

    def __init__(self, *dirs):
        self.files = {}
        for d in dirs:
            if d is None or not os.path.isdir(d):
                continue
            for n in sorted(os.listdir(d)):
                p = os.path.join(d, n)
                if not os.path.isfile(p):
                    continue
                if n.upper().endswith('.PAK'):
                    for k, v in read_pak(p).items():
                        self.files.setdefault(k, v)
            for n in sorted(os.listdir(d)):
                p = os.path.join(d, n)
                if os.path.isfile(p) and not n.upper().endswith('.PAK'):
                    self.files[n.upper()] = open(p, 'rb').read()

    def get(self, name):
        return self.files.get(name.upper())

# ---- string files (DUNE, MESSAGE, INTRO, TEXT?, PROTECT) ----

COUPLES = (" etainosrlhcdupm" "tasio wb" " rnsdalm" "h ieoras" "nrtlc sy"
           "nstcloer" " dtgesio" "nr ufmsw" " tep.ica" "e oiadur" " laeiyod"
           "eia otru" "etoakhlr" " eiu,.oa" "nsrctlai" "leoiratp" "eaoip bm")


def decompress(s):
    """String_DecompressAndTranslate: pairs of letters, 0x1B escapes."""
    out = bytearray()
    i = 0
    while i < len(s):
        c = s[i]
        if c & 0x80:
            c &= 0x7F
            out.append(ord(COUPLES[c >> 3]))
            out.append(ord(COUPLES[c + 16]))
        elif c == 0x1B:
            i += 1
            out.append((0x7F + s[i]) & 0xFF)
        else:
            out.append(c)
        i += 1
    return bytes(out)


def compress(s):
    """The reverse of decompress: plain bytes, the ones >= 0x80 escaped
    (no letter pairs: the files only get a little bigger)."""
    out = bytearray()
    for c in s:
        if c >= 0x80 or c == 0x1B:
            out += bytes([0x1B, c - 0x7F])
        else:
            out.append(c)
    return bytes(out)


def string_table_endian(b):
    """'<' (PC) or '>' (Amiga) for the offset table of a string file: the first
    offset is the size of the table, every offset inside the file and rising
    (as String_Load in port/src/string.c decides)."""
    for fmt in ('<', '>'):
        first = struct.unpack_from(fmt + 'H', b, 0)[0]
        if first < 2 or first % 2 or first > len(b):
            continue
        n = first // 2
        offs = [struct.unpack_from(fmt + 'H', b, i * 2)[0] for i in range(n)]
        if all(first <= o < len(b) for o in offs) and offs == sorted(offs):
            return fmt
    raise ValueError('not a string file')


def read_strings(b, compressed):
    """The strings of a file as bytes (decompressed, trailing spaces kept)."""
    fmt = string_table_endian(b)
    n = struct.unpack_from(fmt + 'H', b, 0)[0] // 2
    out = []
    for i in range(n):
        o = struct.unpack_from(fmt + 'H', b, i * 2)[0]
        s = b[o:b.index(b'\0', o)]
        out.append(decompress(s) if compressed else s)
    return out


def write_strings(strings, compressed):
    """A PC (LE) string file of byte strings."""
    body = [(compress(s) if compressed else s) + b'\0' for s in strings]
    table = 2 * len(body)
    out = bytearray()
    pos = table
    for s in body:
        out += struct.pack('<H', pos)
        pos += len(s)
    for s in body:
        out += s
    return bytes(out)

# String_Load in port/src/string.c: the files, compressed or not, the first ID
# and the last one (0: as many as the file has)
STRING_FILES = [
    ('DUNE', False, 1, 339),
    ('MESSAGE', False, 340, 367),
    ('INTRO', False, 368, 404),
    ('TEXTH', True, 405, 444),
    ('TEXTA', True, 445, 484),
    ('TEXTO', True, 485, 524),
    ('PROTECT', True, 525, 0),
]


def trim(s):
    return s.rstrip(b' \t\r\n\x0b\x0c')


def load_string_ids(get, suffix):
    """String_Init / String_Load in port/src/string.c, step by step: the
    string IDs (index -> bytes) the game builds from the files of one
    language. get(name) gives a file's bytes. Returns (strings, per file
    counts of strings in the files)."""
    STR_LOAD_GAME, STR_LOAD_A_GAME = 339, 92
    strings = []
    counts = {}
    for name, compressed, start, end in STRING_FILES:
        b = get(name + '.' + suffix)
        if b is None:
            raise FileNotFoundError(name + '.' + suffix)
        raw = read_strings(b, compressed)
        counts[name] = len(raw)
        if end == 0:
            end = start + len(raw) - 1
        for s in raw:
            if len(strings) > end:
                break
            s = trim(s)
            if len(s) > 0 or len(strings) == 0:
                strings.append(s)
        if len(strings) == 335:
            raise ValueError('DUNE V1.0 message file: not handled here')
        if len(strings) == STR_LOAD_GAME:
            strings.append(strings[STR_LOAD_A_GAME])
        while len(strings) <= end:
            strings.append(b'')
    return strings, counts


def mentat_read(b):
    """A MENTAT?.ENG help file: (subjects, form). A subject: dict with
    'flags' (the 6 bytes after the offset: byte 5 = no description, byte 6 '0' =
    a heading), 'title' bytes, 'campaign' byte, 'text' bytes (decompressed)."""
    if b[:4] != b'FORM':
        raise ValueError('not IFF')
    chunks = {}
    p = 12
    form_type = b[8:12]
    while p + 8 <= len(b):
        cid = b[p:p + 4]
        size = struct.unpack_from('>I', b, p + 4)[0]
        chunks[cid] = (p + 8, size)
        p += 8 + size + (size & 1)
    name_pos, name_size = chunks[b'NAME']
    info_pos, info_size = chunks[b'INFO']
    names = b[name_pos:name_pos + name_size]
    subjects = []
    i = 0
    while i < len(names):
        size = names[i]
        if size == 0:
            break
        e = names[i:i + size]
        offset = struct.unpack_from('>I', e, 1)[0]
        info_len = struct.unpack_from('>I', b, info_pos + 8)[0]
        # the text: compressed, ends with a 0
        end = b.index(b'\0', offset)
        subjects.append({
            'b5': e[5:6], 'b6': e[6:7],
            'title': e[7:size - 2] if e[size - 2] == 0 else e[7:size - 1],
            'campaign': e[size - 1],
            'text': decompress(b[offset:end]),
            'raw': e,
        })
        i += size
    return subjects, form_type, b[info_pos:info_pos + info_size]


def mentat_write(subjects, form_type=b'MENT'):
    """A MENTAT help file in the original layout: FORM MENT, NAME (the
    subjects), INFO (8 unknown bytes as the original + the longest text),
    the compressed texts after the chunks."""
    texts = [compress(s['text']) + b'\0' for s in subjects]
    entries = []
    for s in subjects:
        size = 1 + 4 + 2 + len(s['title']) + 1 + 1
        entries.append(size)
    name_size = sum(entries)
    info_size = 12
    head = 12 + 8 + name_size + (name_size & 1) + 8 + info_size
    offs = []
    pos = head
    for t in texts:
        offs.append(pos)
        pos += len(t)
    name = bytearray()
    for s, size, off in zip(subjects, entries, offs):
        name += bytes([size]) + struct.pack('>I', off) + s['b5'] + s['b6'] + s['title'] + b'\0' + bytes([s['campaign']])
    if name_size & 1:
        name += b'\0'
    info = b'\0' * 8 + struct.pack('>I', max(len(t) for t in texts))
    body = b'NAME' + struct.pack('>I', name_size) + bytes(name) + b'INFO' + struct.pack('>I', info_size) + info
    form = form_type + body
    out = b'FORM' + struct.pack('>I', len(form) + sum(len(t) for t in texts)) + form + b''.join(texts)
    return out

# ---- fonts ----

class Font:
    """gui/font.c Font_LoadFile: 4-bit pixels, width per char, unused lines
    above, used lines."""

    def __init__(self, b):
        if b[2] != 0 or b[3] != 5:
            raise ValueError('not a font')
        start, data_start, width_list, count_end, line_list = struct.unpack_from('<5H', b, 4)
        self.height = b[start + 4]
        self.max_width = b[start + 5]
        self.count = count_end - width_list
        self.chars = []
        for i in range(self.count):
            w = b[width_list + i]
            unused = b[line_list + i * 2]
            used = b[line_list + i * 2 + 1]
            off = struct.unpack_from('<H', b, data_start + i * 2)[0]
            rows = []
            if off != 0:
                for y in range(used):
                    row = []
                    for x in range(w):
                        v = b[off + y * ((w + 1) // 2) + x // 2]
                        if x % 2:
                            v >>= 4
                        row.append(v & 0xF)
                    rows.append(row)
            self.chars.append({'width': w, 'unused': unused, 'used': used if off else 0, 'rows': rows})

    def glyph_rows(self, c):
        """The char as height rows of pixel values (0 = background)."""
        ch = self.chars[c] if c < self.count else {'width': 0, 'unused': 0, 'used': 0, 'rows': []}
        out = [[0] * ch['width'] for _ in range(self.height)]
        for y, row in enumerate(ch['rows']):
            if ch['unused'] + y < self.height:
                out[ch['unused'] + y] = list(row)
        return out
