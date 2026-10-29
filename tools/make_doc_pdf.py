#!/usr/bin/env python3
"""The documentation of the Amiga port as a PDF: a title page, the contents,
the history of the changes (git log, and what is not committed yet) and the
whole docs/PORTING.md (progress, decisions, measurements, the LAN game).

  python tools/make_doc_pdf.py [--out docs/OpenDUNE-Amiga-port.pdf]

ReportLab with the Windows fonts Arial and Consolas (Polish letters). The
Markdown of PORTING.md: headings, paragraphs, lists (two levels), tables,
`code`, **bold**, [links](...).
"""
import argparse
import datetime
import os
import re
import subprocess

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, Frame, PageBreak, PageTemplate, Paragraph, Spacer, Table,
                                TableStyle)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = "C:/Windows/Fonts"

pdfmetrics.registerFont(TTFont("Arial", os.path.join(FONTS, "arial.ttf")))
pdfmetrics.registerFont(TTFont("Arial-Bold", os.path.join(FONTS, "arialbd.ttf")))
pdfmetrics.registerFont(TTFont("Arial-Italic", os.path.join(FONTS, "ariali.ttf")))
pdfmetrics.registerFont(TTFont("Consolas", os.path.join(FONTS, "consola.ttf")))
pdfmetrics.registerFontFamily("Arial", normal="Arial", bold="Arial-Bold", italic="Arial-Italic", boldItalic="Arial-Bold")

ACCENT = colors.HexColor("#8a4b12")		# Dune's sand brown
CODE_BG = "#f1ece4"

BODY = ParagraphStyle("body", fontName="Arial", fontSize=9.5, leading=13, spaceAfter=4)
SMALL = ParagraphStyle("small", parent=BODY, fontSize=8, leading=10, spaceAfter=0)
BULLET = ParagraphStyle("bullet", parent=BODY, leftIndent=12, bulletIndent=3, spaceAfter=2)
BULLET2 = ParagraphStyle("bullet2", parent=BODY, leftIndent=24, bulletIndent=15, spaceAfter=2)
H1 = ParagraphStyle("h1", fontName="Arial-Bold", fontSize=17, leading=21, textColor=ACCENT, spaceBefore=6, spaceAfter=8)
H2 = ParagraphStyle("h2", fontName="Arial-Bold", fontSize=13.5, leading=17, textColor=ACCENT, spaceBefore=12, spaceAfter=6)
H3 = ParagraphStyle("h3", fontName="Arial-Bold", fontSize=11, leading=14, spaceBefore=10, spaceAfter=4)
TITLE = ParagraphStyle("title", fontName="Arial-Bold", fontSize=26, leading=32, alignment=TA_CENTER, textColor=ACCENT)
SUBTITLE = ParagraphStyle("subtitle", fontName="Arial", fontSize=13, leading=18, alignment=TA_CENTER)
CENTER = ParagraphStyle("center", parent=BODY, alignment=TA_CENTER)


def inline(text):
    """Markdown inline markup -> ReportLab paragraph markup."""
    text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    parts = re.split(r"(`[^`]+`)", text)
    out = []
    for p in parts:
        if len(p) > 1 and p.startswith("`") and p.endswith("`"):
            out.append('<font name="Consolas" backColor="%s">%s</font>' % (CODE_BG, p[1:-1]))
            continue
        p = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", p)
        p = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", p)
        p = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<i>\1</i>", p)
        out.append(p)
    return "".join(out)


class Doc(BaseDocTemplate):
    def __init__(self, path):
        super().__init__(path, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=18 * mm,
                         bottomMargin=18 * mm, title="OpenDUNE na Amigę – dokumentacja portu",
                         author="Port OpenDUNE (Amiga)")
        frame = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="f")
        self.addPageTemplates([PageTemplate(id="page", frames=[frame], onPage=self.footer)])

    def footer(self, canv, doc):
        if doc.page == 1:
            return
        canv.saveState()
        canv.setFont("Arial", 8)
        canv.setFillColor(colors.grey)
        canv.drawString(18 * mm, 10 * mm, "OpenDUNE na Amigę – dokumentacja portu")
        canv.drawRightString(A4[0] - 18 * mm, 10 * mm, "str. %d" % doc.page)
        canv.restoreState()

    def afterFlowable(self, flowable):
        # headings into the contents
        if isinstance(flowable, Paragraph) and flowable.style.name in ("h1", "h2", "h3"):
            level = {"h1": 0, "h2": 1, "h3": 2}[flowable.style.name]
            key = "h%d" % id(flowable)
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(flowable.getPlainText(), key, level=level)
            self.notify("TOCEntry", (level, flowable.getPlainText(), self.page, key))


def table(rows, width):
    """A Markdown table (first row the header) with wrapped cells."""
    n = max(len(r) for r in rows)
    rows = [r + [""] * (n - len(r)) for r in rows]
    weights = [max(min(len(r[c]), 60) for r in rows) + 4 for c in range(n)]
    total = float(sum(weights))
    widths = [width * w / total for w in weights]
    data = [[Paragraph(inline(c), ParagraphStyle("cell", parent=SMALL, fontName="Arial-Bold" if i == 0 else "Arial"))
             for c in r] for i, r in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e6d6bf")),
        ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#b9a68c")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 3), ("RIGHTPADDING", (0, 0), (-1, -1), 3),
        ("TOPPADDING", (0, 0), (-1, -1), 2), ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
    ]))
    return t


def markdown(path, width):
    """PORTING.md -> flowables."""
    story = []
    lines = open(path, encoding="utf-8").read().splitlines()
    para, items, rows = [], [], []

    def flush():
        nonlocal para, items, rows
        if para:
            story.append(Paragraph(inline(" ".join(para)), BODY))
        for level, num, text in items:
            style = BULLET2 if level else BULLET
            story.append(Paragraph(inline(text), style, bulletText=num or ("–" if level else "•")))
        if rows:
            body = [r for r in rows if not re.match(r"^\s*:?-{2,}", r[0] if r else "")]
            story.append(table(body, width))
            story.append(Spacer(1, 6))
        para, items, rows = [], [], []

    for raw in lines:
        line = raw.rstrip()
        if not line.strip():
            flush()
            continue
        m = re.match(r"^(#{1,3}) (.*)", line)
        if m:
            flush()
            story.append(Paragraph(inline(m.group(2)), {1: H1, 2: H2, 3: H3}[len(m.group(1))]))
            continue
        if line.lstrip().startswith("|"):
            if para or items:
                flush()
            rows.append([c.strip() for c in line.strip().strip("|").split("|")])
            continue
        m = re.match(r"^(\s*)(?:[-*]|(\d+)\.) (.*)", line)
        if m:
            if para or rows:
                flush()
            level = 1 if len(m.group(1)) >= 2 else 0
            items.append([level, m.group(2) + "." if m.group(2) else None, m.group(3)])
            continue
        if items and raw.startswith(" "):
            items[-1][2] += " " + line.strip()		# a list item goes on
            continue
        if rows:
            flush()
        para.append(line.strip())
    flush()
    return story


def git(*args):
    return subprocess.run(["git", "-C", ROOT] + list(args), capture_output=True, text=True, encoding="utf-8").stdout


def history(width):
    story = [Paragraph("Historia zmian", H1),
             Paragraph("Wszystkie commity repozytorium portu, od najnowszego (opisy commitów są po angielsku). "
                       "Szczegóły każdej zmiany: rozdziały dokumentacji z tą samą datą.", BODY)]
    rows = [["Data", "Commit", "Zmiana"]]
    for line in git("log", "--format=%ad|%h|%s", "--date=short").splitlines():
        d, h, s = line.split("|", 2)
        rows.append([d, h, s])
    story.append(table(rows, width))
    status = [l for l in git("status", "--short").splitlines() if l.strip()]
    if status:
        story.append(Paragraph("Zmiany jeszcze bez commita", H2))
        story.append(Paragraph("Pliki zmienione od ostatniego commita (opis w rozdziałach z ostatnią datą):", BODY))
        for l in status:
            story.append(Paragraph(inline("`%s`" % l.strip()), BULLET, bulletText="•"))
    return story


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "docs", "OpenDUNE-Amiga-port.pdf"))
    a = ap.parse_args()

    doc = Doc(a.out)
    width = doc.width
    commits = git("rev-list", "--count", "HEAD").strip()
    first = git("log", "--reverse", "--format=%ad", "--date=short").splitlines()[0]
    today = datetime.date.today().strftime("%d.%m.%Y")

    story = [Spacer(1, 60 * mm),
             Paragraph("OpenDUNE na Amigę", TITLE), Spacer(1, 6 * mm),
             Paragraph("Dokumentacja portu: postępy od początku, decyzje, pomiary i zmiany", SUBTITLE),
             Spacer(1, 20 * mm),
             Paragraph("Amiga z kartą Warp (68060, RTG) · AmigaOS 3.2 · gra LAN dla 3 graczy", CENTER),
             Paragraph("Prace od %s · %s commitów · stan na %s" % (first, commits, today), CENTER),
             PageBreak()]
    toc = TableOfContents()
    toc.levelStyles = [ParagraphStyle("toc0", fontName="Arial-Bold", fontSize=10, leading=14, leftIndent=0),
                       ParagraphStyle("toc1", fontName="Arial", fontSize=9, leading=12, leftIndent=12),
                       ParagraphStyle("toc2", fontName="Arial", fontSize=8, leading=10, leftIndent=24,
                                      textColor=colors.HexColor("#444444"))]
    story += [Paragraph("Spis treści", ParagraphStyle("tocTitle", parent=H1)), toc, PageBreak()]
    story += history(width) + [PageBreak()]
    story += markdown(os.path.join(ROOT, "docs", "PORTING.md"), width)
    doc.multiBuild(story)
    print(a.out)


if __name__ == "__main__":
    main()
