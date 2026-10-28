"""Group selection and right-click orders (gui/rts_amiga.c): the hooks in the
shared code. One-time edits of port/src; each replacement must match once."""
import os, re, sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port", "src")

EDITS = [
    ("input/mouse.c",
     """	if (mousePosY >= SCREEN_HEIGHT) {
		mousePosY = SCREEN_HEIGHT - 1;
	}
""",
     """	if (mousePosY >= SCREEN_HEIGHT) {
		mousePosY = SCREEN_HEIGHT - 1;
	}

#if defined(__amigaos__)
	/* drag rectangles and right-click orders take their button events */
	newButtonState = RTS_FilterButtons(mousePosX, mousePosY, newButtonState);
#endif
"""),
    ("opendune.c",
     "	for (;; sleepIdle()) {\n		if (g_gameMode == GM_MENU) {",
     "	for (;; sleepIdle()) {\n#if defined(__amigaos__)\n		RTS_Tick();\n#endif\n		if (g_gameMode == GM_MENU) {"),
    ("gui/viewport.c",
     "		if (u == g_unitSelected && Map_IsPositionInViewport(u->o.position, &x, &y)) {",
     "		if ((u == g_unitSelected || RTS_IS_IN_GROUP(u)) && Map_IsPositionInViewport(u->o.position, &x, &y)) {"),
    ("gui/viewport.c",
     "			if (u != g_unitSelected) continue;",
     "			if (u != g_unitSelected && !RTS_IS_IN_GROUP(u)) continue;"),
    ("gui/viewport.c",
     "void GUI_Widget_Viewport_Draw(",
     """#if defined(__amigaos__)
#define RTS_IS_IN_GROUP(u) RTS_IsInGroup(u)
#else
#define RTS_IS_IN_GROUP(u) false
#endif

void GUI_Widget_Viewport_Draw("""),
]

INCLUDES = {
    "input/mouse.c": '"../gui/rts_amiga.h"',
    "opendune.c": '"gui/rts_amiga.h"',
    "gui/viewport.c": '"rts_amiga.h"',
}


def main():
    ok = True
    for f, old, new in EDITS:
        p = os.path.join(SRC, f)
        s = open(p, encoding="latin-1").read().replace("\r\n", "\n")
        n = s.count(old)
        if n != 1:
            print("%s: %d matches for %r" % (f, n, old[:70]))
            ok = False
            continue
        s = s.replace(old, new)
        open(p, "w", encoding="latin-1", newline="\n").write(s)
    for f, inc in INCLUDES.items():
        p = os.path.join(SRC, f)
        s = open(p, encoding="latin-1").read()
        idx = [m.end() for m in re.finditer(r'#include "[^"]+"\n', s)]
        s = s[:idx[-1]] + "#if defined(__amigaos__)\n#include %s\n#endif\n" % inc + s[idx[-1]:]
        open(p, "w", encoding="latin-1", newline="\n").write(s)
    mk = os.path.join(SRC, "..", "Makefile.amiga")
    m = open(mk).read()
    if "gui/rts_amiga.c" not in m:
        m = m.replace("gui/layout_amiga.c", "gui/layout_amiga.c gui/rts_amiga.c")
        open(mk, "w", newline="\n").write(m)
    print("ok" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
