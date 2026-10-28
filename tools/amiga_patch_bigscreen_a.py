"""Big in-game screen, stage A (no visible change): the screen size becomes a
variable (g_screenWidth / g_screenHeight, 320x200 until the map switches to
the large layout), buffers and sizes hold SCREEN_WIDTH_MAX x SCREEN_HEIGHT_MAX,
and the 16-bit overflows that a 640x480 buffer would hit are removed.
One-time edits of port/src; each replacement must match exactly once."""
import os, sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port", "src")

EDITS = [
    ("gfx.h",
     """enum {
	SCREEN_WIDTH  = 320, /*!< Width of the screen in pixels. */
	SCREEN_HEIGHT = 200  /*!< Height of the screen in pixels. */
};""",
     """#if defined(__amigaos__)
/* The size of the screen the game draws, in pixels: 320x200 in the menus and
 * cutscenes, larger on the map (GFX_SetScreenSize()). Buffers are made for
 * the maximum. */
#define SCREEN_WIDTH_MAX  640
#define SCREEN_HEIGHT_MAX 480
extern uint16 g_screenWidth;
extern uint16 g_screenHeight;
#define SCREEN_WIDTH  g_screenWidth
#define SCREEN_HEIGHT g_screenHeight
extern void GFX_SetScreenSize(uint16 width, uint16 height);
#else
enum {
	SCREEN_WIDTH  = 320, /*!< Width of the screen in pixels. */
	SCREEN_HEIGHT = 200  /*!< Height of the screen in pixels. */
};
#define SCREEN_WIDTH_MAX  SCREEN_WIDTH
#define SCREEN_HEIGHT_MAX SCREEN_HEIGHT
#endif"""),
    ("gfx.h",
     "extern uint16 GFX_Screen_GetSize_ByIndex(Screen screenID);",
     "extern uint32 GFX_Screen_GetSize_ByIndex(Screen screenID);"),
    ("gfx.h",
     "extern uint16 GFX_GetSize(int16 width, int16 height);",
     "extern uint32 GFX_GetSize(int16 width, int16 height);"),
    ("gfx.h",
     "#ifdef GFX_STORE_DIRTY_AREA_BLOCKS\nextern uint32 g_dirty_blocks[200];\n#endif",
     """#ifdef GFX_STORE_DIRTY_AREA_BLOCKS
#if defined(__amigaos__)
/* per row: GFX_DIRTY_WORDS words of 32 blocks of 16 pixels */
#define GFX_DIRTY_WORDS ((SCREEN_WIDTH_MAX + 511) / 512)
extern uint32 g_dirty_blocks[SCREEN_HEIGHT_MAX][GFX_DIRTY_WORDS];
#else
extern uint32 g_dirty_blocks[200];
#endif
#endif"""),

    ("gfx.c",
     "static const uint16 s_screenBufferSize[GFX_SCREEN_BUFFER_COUNT] = { 0xFA00, 0xFBF4, 0xFA00, 0xFD0D/*, 0xA044*/ };",
     """#if defined(__amigaos__)
/* the same extra bytes beyond a full screen as the original sizes */
static const uint32 s_screenBufferSize[GFX_SCREEN_BUFFER_COUNT] = {
	SCREEN_WIDTH_MAX * SCREEN_HEIGHT_MAX,
	SCREEN_WIDTH_MAX * SCREEN_HEIGHT_MAX + 0xFBF4 - 0xFA00,
	SCREEN_WIDTH_MAX * SCREEN_HEIGHT_MAX,
	SCREEN_WIDTH_MAX * SCREEN_HEIGHT_MAX + 0xFD0D - 0xFA00 };
uint16 g_screenWidth = 320;
uint16 g_screenHeight = 200;
#else
static const uint16 s_screenBufferSize[GFX_SCREEN_BUFFER_COUNT] = { 0xFA00, 0xFBF4, 0xFA00, 0xFD0D/*, 0xA044*/ };
#endif"""),
    ("gfx.c",
     "#ifdef GFX_STORE_DIRTY_AREA_BLOCKS\nuint32 g_dirty_blocks[200];\n#endif",
     "#ifdef GFX_STORE_DIRTY_AREA_BLOCKS\n#if defined(__amigaos__)\nuint32 g_dirty_blocks[SCREEN_HEIGHT_MAX][GFX_DIRTY_WORDS];\n#else\nuint32 g_dirty_blocks[200];\n#endif\n#endif"),
    ("gfx.c",
     "uint16 GFX_Screen_GetSize_ByIndex(Screen screenID)",
     "uint32 GFX_Screen_GetSize_ByIndex(Screen screenID)"),
    ("gfx.c",
     """	mask = (1 << ((right + 15) >> 4)) - 1;
	mask -= (1 << (left >> 4)) - 1;
	for (y = top; y < bottom; y++) g_dirty_blocks[y] |= mask;""",
     """#if defined(__amigaos__)
	{
		int w, lo = left >> 4, hi = (right + 15) >> 4;
		if (bottom > SCREEN_HEIGHT_MAX) bottom = SCREEN_HEIGHT_MAX;
		if (hi > 32 * GFX_DIRTY_WORDS) hi = 32 * GFX_DIRTY_WORDS;
		for (w = 0; w < GFX_DIRTY_WORDS; w++) {
			int a = lo - 32 * w, b = hi - 32 * w;
			if (a < 0) a = 0;
			if (b > 32) b = 32;
			if (a >= b) continue;
			mask = (b == 32) ? 0xFFFFFFFFUL : ((1UL << b) - 1);
			mask &= ~((1UL << a) - 1);
			for (y = top; y < bottom; y++) g_dirty_blocks[y][w] |= mask;
		}
	}
#else
	mask = (1 << ((right + 15) >> 4)) - 1;
	mask -= (1 << (left >> 4)) - 1;
	for (y = top; y < bottom; y++) g_dirty_blocks[y] |= mask;
#endif"""),
    ("gfx.c",
     "uint16 GFX_GetSize(int16 width, int16 height)",
     "uint32 GFX_GetSize(int16 width, int16 height)"),
    ("gfx.c",
     "	return width * height;\n}",
     "	return (uint32)width * height;\n}"),
    # the stride of the tiles follows the current screen width
    ("gfx.c",
     "	wptr = GFX_Screen_GetActive();\n	wptr += y * SCREEN_WIDTH + x;",
     "	wptr = GFX_Screen_GetActive();\n	wptr += y * SCREEN_WIDTH + x;\n	spacing = SCREEN_WIDTH - 2 * s_tileWidth;"),

    ("video/video.h",
     "extern void * Video_GetFrameBuffer(uint16 size);",
     "#if defined(__amigaos__)\nextern void * Video_GetFrameBuffer(uint32 size);\n#else\nextern void * Video_GetFrameBuffer(uint16 size);\n#endif"),
    ("video/video_amiga.c",
     "void *Video_GetFrameBuffer(uint16 size)",
     "void *Video_GetFrameBuffer(uint32 size)"),

    # GUI_DrawChar kept the byte offset in the uint16 x
    ("gui/gui.c",
     """	FontChar *fc;

	uint16 remainingWidth;""",
     """	FontChar *fc;

	uint32 remainingWidth;"""),
    ("gui/gui.c",
     """	GFX_Screen_SetDirty(SCREEN_ACTIVE, x, y, x + fc->width, y + g_fontCurrent->height);
	x += y * (uint16)SCREEN_WIDTH;""",
     """	GFX_Screen_SetDirty(SCREEN_ACTIVE, x, y, x + fc->width, y + g_fontCurrent->height);
	screen += y * (uint32)SCREEN_WIDTH;"""),
    ("gui/gui.c",
     "		x += fc->unusedLines * (uint16)SCREEN_WIDTH;",
     "		x += fc->unusedLines * SCREEN_WIDTH;"),
    ("gui/gui.c",
     "static ClippingArea g_clipping = { 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 };",
     "static ClippingArea g_clipping = { 0, 0, 320 - 1, 200 - 1 };"),
    ("gui/gui.c",
     "	uint16 offsetsY[100];\n	uint16 offsetsX[40];",
     "	uint16 offsetsY[SCREEN_HEIGHT_MAX / 2];\n	uint16 offsetsX[SCREEN_WIDTH_MAX / 8];"),
    ("gui/gui.c",
     "	uint16 columns[SCREEN_WIDTH];\n	uint16 rows[SCREEN_HEIGHT];",
     "	uint16 columns[SCREEN_WIDTH_MAX];\n	uint16 rows[SCREEN_HEIGHT_MAX];"),
    ("video/amiga_cmd.c", "XXX", "XXX"),
]


def main():
    ok = True
    for f, old, new in EDITS:
        if old == "XXX":
            continue
        p = os.path.join(SRC, f)
        s = open(p, encoding="latin-1").read().replace("\r\n", "\n")
        n = s.count(old)
        if n != 1:
            print("%s: %d matches for %r" % (f, n, old[:70]))
            ok = False
            continue
        s = s.replace(old, new)
        open(p, "w", encoding="latin-1", newline="\n").write(s)
    # GFX_DrawTile: the local stride
    p = os.path.join(SRC, "gfx.c")
    s = open(p, encoding="latin-1").read()
    if s.count("			wptr += s_tileSpacing;") == 2:
        s = s.replace("			wptr += s_tileSpacing;", "			wptr += spacing;")
        i = s.index("void GFX_DrawTile(")
        j = s.index("{", i) + 1
        s = s[:j] + "\n\tuint16 spacing;" + s[j:]
        open(p, "w", encoding="latin-1", newline="\n").write(s)
    else:
        print("gfx.c: s_tileSpacing uses not found")
        ok = False
    print("ok" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
