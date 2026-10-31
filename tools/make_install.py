#!/usr/bin/env python3
"""The installation guide of the release in English, Polish and German, from
one source (docs/release/amiga/Install_src.txt, UTF-8):

  Install.txt   for the Amiga: ISO-8859-1 (German letters as they are), the
                Polish letters without their marks (the Amiga has no Polish
                character set by default)
  Install.pdf   the same with all the letters (ReportLab, Windows' Arial and
                Consolas)

  python tools/make_install.py [OUT_DIR]   (default docs/release/amiga)

The source: a title is underlined with ===, a section with ---, a line of
### a new language (a new page in the PDF); lines indented by two spaces
after an empty line are typed text (Consolas), "- " and "1. " lists.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'docs', 'release', 'amiga', 'Install_src.txt')
POLISH = str.maketrans('ąćęłńóśźżĄĆĘŁŃÓŚŹŻ', 'acelnoszzACELNOSZZ')


def text(src, out):
    t = src.translate(POLISH)
    open(out, 'w', encoding='latin-1', newline='\n').write(t)


def blocks(lines):
    """(kind, text) per block: title, section, page, code, item, para"""
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        nxt = lines[i + 1] if i + 1 < len(lines) else ''
        if not line.strip():
            i += 1
            continue
        if line.startswith('###'):
            out.append(('page', ''))
            i += 1
            continue
        if nxt.startswith('===') and line.strip():
            out.append(('title', line.strip()))
            i += 2
            continue
        if nxt.startswith('---') and line.strip():
            out.append(('section', line.strip()))
            i += 2
            continue
        if line.startswith('  '):
            code = []
            while i < len(lines) and lines[i].startswith('  '):
                code.append(lines[i][2:])
                i += 1
            out.append(('code', '\n'.join(code)))
            continue
        if line.startswith('- ') or (line[:1].isdigit() and line[1:3] == '. '):
            item = [line]
            i += 1
            while i < len(lines) and lines[i].startswith('  ') and lines[i].strip():
                item.append(lines[i].strip())
                i += 1
            out.append(('item', ' '.join(item)))
            continue
        para = [line]
        i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].startswith('  ') \
                and not lines[i].startswith('- ') and not lines[i].startswith('###') \
                and not (i + 1 < len(lines) and lines[i + 1][:3] in ('===', '---')):
            para.append(lines[i])
            i += 1
        out.append(('para', ' '.join(para)))
    return out


def pdf(src, out):
    from reportlab.lib import colors
    from reportlab.lib.pagesizes import A4
    from reportlab.lib.styles import ParagraphStyle
    from reportlab.lib.units import mm
    from reportlab.pdfbase import pdfmetrics
    from reportlab.pdfbase.ttfonts import TTFont
    from reportlab.platypus import PageBreak, Paragraph, Preformatted, SimpleDocTemplate, Spacer
    from xml.sax.saxutils import escape

    fonts = os.path.join(os.environ.get('WINDIR', r'C:\Windows'), 'Fonts')
    pdfmetrics.registerFont(TTFont('Arial', os.path.join(fonts, 'arial.ttf')))
    pdfmetrics.registerFont(TTFont('Arial-Bold', os.path.join(fonts, 'arialbd.ttf')))
    pdfmetrics.registerFont(TTFont('Consolas', os.path.join(fonts, 'consola.ttf')))
    accent = colors.HexColor('#8a4b12')
    body = ParagraphStyle('body', fontName='Arial', fontSize=10.5, leading=14.5, spaceAfter=6)
    item = ParagraphStyle('item', parent=body, leftIndent=14, bulletIndent=2, spaceAfter=3)
    title = ParagraphStyle('title', fontName='Arial-Bold', fontSize=18, leading=23, textColor=accent, spaceAfter=10)
    section = ParagraphStyle('section', fontName='Arial-Bold', fontSize=13, leading=17, textColor=accent,
                             spaceBefore=10, spaceAfter=5)
    code = ParagraphStyle('code', fontName='Consolas', fontSize=9.5, leading=12.5, leftIndent=10,
                          backColor=colors.HexColor('#f1ece4'), borderPadding=5, spaceBefore=3, spaceAfter=9)
    story = []
    pages = 0
    for kind, t in blocks(src.split('\n')):
        if kind == 'page':
            # the list of the languages on the first page, with the English part
            story.append(PageBreak() if pages else Spacer(1, 8 * mm))
            pages += 1
        elif kind == 'title':
            story.append(Paragraph(escape(t), title))
        elif kind == 'section':
            story.append(Paragraph(escape(t), section))
        elif kind == 'code':
            story.append(Preformatted(t, code))
        elif kind == 'item':
            mark, rest = t.split(' ', 1)
            story.append(Paragraph(escape(rest), item, bulletText='\u2022' if mark == '-' else mark))
        else:
            story.append(Paragraph(escape(t), body))
    doc = SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=18 * mm,
                            bottomMargin=18 * mm, title='OpenDUNE - Install', author='OpenDUNE Amiga port')
    doc.build(story)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(SRC)
    src = open(SRC, encoding='utf-8').read()
    text(src, os.path.join(outdir, 'Install.txt'))
    pdf(src, os.path.join(outdir, 'Install.pdf'))
    print('%s: Install.txt, Install.pdf' % outdir)


if __name__ == '__main__':
    main()
