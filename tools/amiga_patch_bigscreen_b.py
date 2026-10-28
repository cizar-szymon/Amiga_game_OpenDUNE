"""Big in-game screen, stage B: the map screen at 640x480 (gui/layout_amiga.c).
The viewport size in tiles and the sidebar's shift become variables
(VIEWPORT_TILES_W/H, SIDEBAR_DX, 15/10/0 in the original layout).
One-time edits of port/src; each replacement must match exactly once."""
import os, re, sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port", "src")

INC = '#if defined(__amigaos__)\n#include "gui/layout_amiga.h"\n#endif\n'

EDITS = [
    # ---- gfx.h / gfx.c: the variables and GFX_SetScreenSize()
    ("gfx.h",
     "extern void GFX_SetScreenSize(uint16 width, uint16 height);\n#else",
     """extern void GFX_SetScreenSize(uint16 width, uint16 height);
/* the map screen's layout (gui/layout_amiga.c) */
extern uint16 g_viewportTilesW;
extern uint16 g_viewportTilesH;
extern int16 g_sidebarDX;
#define VIEWPORT_TILES_W g_viewportTilesW	/* viewport width in tiles (15) */
#define VIEWPORT_TILES_H g_viewportTilesH	/* viewport height in tiles (10) */
#define VIEWPORT_TILES_H_MAX 30
#define SIDEBAR_DX g_sidebarDX			/* shift of the sidebar and radar (0) */
#else"""),
    ("gfx.h",
     "#define SCREEN_WIDTH_MAX  SCREEN_WIDTH\n#define SCREEN_HEIGHT_MAX SCREEN_HEIGHT\n#endif",
     "#define SCREEN_WIDTH_MAX  SCREEN_WIDTH\n#define SCREEN_HEIGHT_MAX SCREEN_HEIGHT\n#define VIEWPORT_TILES_W 15\n#define VIEWPORT_TILES_H 10\n#define VIEWPORT_TILES_H_MAX 10\n#define SIDEBAR_DX 0\n#endif"),
    ("gfx.c",
     "uint16 g_screenWidth = 320;\nuint16 g_screenHeight = 200;\n#else",
     """uint16 g_screenWidth = 320;
uint16 g_screenHeight = 200;

/** Changes the size of the screen the game draws; SCREEN_0 is cleared. */
void GFX_SetScreenSize(uint16 width, uint16 height)
{
	g_screenWidth = width;
	g_screenHeight = height;
	memset(s_screenBuffer[0], 0, s_screenBufferSize[0]);
	GFX_Screen_SetDirty(SCREEN_0, 0, 0, width, height);
}
#else"""),

    # ---- map.c
    ("map.c",
     """	x = max(x, mapInfo->minX);
	y = max(y, mapInfo->minY);

	x = min(x, mapInfo->minX + mapInfo->sizeX - 15);
	y = min(y, mapInfo->minY + mapInfo->sizeY - 10);""",
     """	/* the upper limit first: the viewport can be larger than a small map */
	x = min(x, mapInfo->minX + mapInfo->sizeX - VIEWPORT_TILES_W);
	y = min(y, mapInfo->minY + mapInfo->sizeY - VIEWPORT_TILES_H);

	x = max(x, mapInfo->minX);
	y = max(y, mapInfo->minY);"""),
    ("map.c",
     """	/* Border tiles of the viewport relative to the top-left. */
	static const uint16 viewportBorder[] = {
		0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
		0x0040, 0x004E,
		0x0080, 0x008E,
		0x00C0, 0x00CE,
		0x0100, 0x010E,
		0x0140, 0x014E,
		0x0180, 0x018E,
		0x01C0, 0x01CE,
		0x0200, 0x020E,
		0x0240, 0x0241, 0x0242, 0x0243, 0x0244, 0x0245, 0x0246, 0x0247, 0x0248, 0x0249, 0x024A, 0x024B, 0x024C, 0x024D, 0x024E,
		0xFFFF
	};

	static uint16 minimapPreviousPosition = 0;""",
     """	static uint16 minimapPreviousPosition = 0;"""),
    ("map.c",
     """void Map_UpdateMinimapPosition(uint16 packed, bool forceUpdate)
{""",
     """/* Border tiles of the viewport relative to the top-left (0x0000..0x000E,
 * 0x0040, 0x004E, ... 0x0240..0x024E for 15x10), 0xFFFF at the end. */
static uint16 viewportBorder[2 * 64 + 2 * 64 + 1];
static uint16 s_borderW, s_borderH;

static void Map_UpdateViewportBorder(void)
{
	uint16 *m = viewportBorder;
	uint16 x, y;

	s_borderW = VIEWPORT_TILES_W;
	s_borderH = VIEWPORT_TILES_H;
	for (x = 0; x < s_borderW; x++) *m++ = x;
	for (y = 1; y < s_borderH - 1; y++) {
		*m++ = y << 6;
		*m++ = (y << 6) + s_borderW - 1;
	}
	for (x = 0; x < s_borderW; x++) *m++ = ((s_borderH - 1) << 6) + x;
	*m = 0xFFFF;
}

void Map_UpdateMinimapPosition(uint16 packed, bool forceUpdate)
{"""),
    ("map.c",
     """	if (packed != 0xFFFF && packed == minimapPreviousPosition && !forceUpdate) return;
	if (g_selectionType == SELECTIONTYPE_MENTAT) return;
""",
     """	if (packed != 0xFFFF && packed == minimapPreviousPosition && !forceUpdate) return;
	if (g_selectionType == SELECTIONTYPE_MENTAT) return;
	if (s_borderW != VIEWPORT_TILES_W || s_borderH != VIEWPORT_TILES_H) Map_UpdateViewportBorder();
"""),
    ("map.c",
     """		left   = (Tile_GetPackedX(packed) - mapInfo->minX) * (mapScale + 1) + 256;
		right  = left + mapScale * 15 + 14;
		top    = (Tile_GetPackedY(packed) - mapInfo->minY) * (mapScale + 1) + 136;
		bottom = top + mapScale * 10 + 9;""",
     """		left   = (Tile_GetPackedX(packed) - mapInfo->minX) * (mapScale + 1) + 256 + SIDEBAR_DX;
		right  = left + mapScale * VIEWPORT_TILES_W + VIEWPORT_TILES_W - 1;
		top    = (Tile_GetPackedY(packed) - mapInfo->minY) * (mapScale + 1) + 136;
		bottom = top + mapScale * VIEWPORT_TILES_H + VIEWPORT_TILES_H - 1;
		/* a large viewport on a small map: the rectangle inside the radar */
		if (right > 256 + SIDEBAR_DX + 63) right = 256 + SIDEBAR_DX + 63;
		if (bottom > 136 + 63) bottom = 136 + 63;"""),
    ("map.c",
     """	if (cleared && oldScreenID == SCREEN_0) {
		GUI_Mouse_Hide_Safe();
		GUI_Screen_Copy(32, 136, 32, 136, 8, 64, SCREEN_1, SCREEN_0);""",
     """	if (cleared && oldScreenID == SCREEN_0) {
		GUI_Mouse_Hide_Safe();
		GUI_Screen_Copy(32 + SIDEBAR_DX / 8, 136, 32 + SIDEBAR_DX / 8, 136, 8, 64, SCREEN_1, SCREEN_0);"""),
    ("map.c",
     "	return x >= -16 && x <= 256 && y >= -16 && y <= 176;",
     "	return x >= -16 && x <= VIEWPORT_TILES_W * 16 + 16 && y >= -16 && y <= VIEWPORT_TILES_H * 16 + 16;"),
    ("map.c",
     "	return x >= x2 && x < x2 + 15 && y >= y2 && y < y2 + 10;",
     "	return x >= x2 && x < x2 + VIEWPORT_TILES_W && y >= y2 && y < y2 + VIEWPORT_TILES_H;"),
    ("map.c",
     """	x = Tile_GetPackedX(packed) - 7;
	y = Tile_GetPackedY(packed) - 5;

	mapInfo = &g_mapInfos[g_scenario.mapScale];

	x = max(mapInfo->minX, min(mapInfo->minX + mapInfo->sizeX - 15, x));
	y = max(mapInfo->minY, min(mapInfo->minY + mapInfo->sizeY - 10, y));""",
     """	x = Tile_GetPackedX(packed) - VIEWPORT_TILES_W / 2;
	y = Tile_GetPackedY(packed) - VIEWPORT_TILES_H / 2;

	mapInfo = &g_mapInfos[g_scenario.mapScale];

	x = max(mapInfo->minX, min(mapInfo->minX + mapInfo->sizeX - VIEWPORT_TILES_W, x));
	y = max(mapInfo->minY, min(mapInfo->minY + mapInfo->sizeY - VIEWPORT_TILES_H, y));"""),

    # ---- gui/viewport.c
    ("gui/viewport.c",
     "		x = min((max(x, 256) - 256) / (mapScale + 1), mapInfo->sizeX - 1) + mapInfo->minX;",
     "		x = min((max(x, 256 + SIDEBAR_DX) - (256 + SIDEBAR_DX)) / (mapScale + 1), mapInfo->sizeX - 1) + mapInfo->minX;"),
    ("gui/viewport.c",
     "	int16 minX[10];\n	int16 maxX[10];",
     "	int16 minX[VIEWPORT_TILES_H_MAX];\n	int16 maxX[VIEWPORT_TILES_H_MAX];\n	uint16 msgRow = (VIEWPORT_TILES_H * 6) / 10;	/* row of the viewport message (6) */"),
    ("gui/viewport.c",
     """		for (y = 0; y < 10; y++) {
			uint16 top = (y << 4) + 0x28;	/* 40 */
			for (x = 0; x < (drawToMainScreen ? 15 : 16); x++) {""",
     """		for (y = 0; y < VIEWPORT_TILES_H; y++) {
			uint16 top = (y << 4) + 0x28;	/* 40 */
			for (x = 0; x < (drawToMainScreen ? VIEWPORT_TILES_W : VIEWPORT_TILES_W + 1); x++) {"""),
    ("gui/viewport.c",
     "				if (x < 15 && !forceRedraw && BitArray_Test(g_dirtyViewport, curPos)) {",
     "				if (x < VIEWPORT_TILES_W && !forceRedraw && BitArray_Test(g_dirtyViewport, curPos)) {"),
    ("gui/viewport.c",
     "				if (x < 15) {\n					updateDisplay = true;",
     "				if (x < VIEWPORT_TILES_W) {\n					updateDisplay = true;"),
    ("gui/viewport.c",
     "		GUI_SetClippingArea(0, 40, 239, SCREEN_HEIGHT - 1);",
     "		GUI_SetClippingArea(0, 40, VIEWPORT_TILES_W * 16 - 1, SCREEN_HEIGHT - 1);"),
    ("gui/viewport.c",
     "			if (minY < maxY) GUI_Screen_Copy(32, 136 + minY, 32, 136 + minY, 8,",
     "			if (minY < maxY) GUI_Screen_Copy(32 + SIDEBAR_DX / 8, 136 + minY, 32 + SIDEBAR_DX / 8, 136 + minY, 8,"),
    ("gui/viewport.c",
     """(minX[6] <= 14 || maxX[6] >= 0 || hasScrolled || forceRedraw)) {
		GUI_DrawText_Wrapper(g_viewportMessageText, 112, 139, 15, 0, 0x132);
		minX[6] = -1;
		maxX[6] = 14;""",
     """(minX[msgRow] <= VIEWPORT_TILES_W - 1 || maxX[msgRow] >= 0 || hasScrolled || forceRedraw)) {
		GUI_DrawText_Wrapper(g_viewportMessageText, VIEWPORT_TILES_W * 8 - 8, 40 + msgRow * 16 + 3, 15, 0, 0x132);
		minX[msgRow] = -1;
		maxX[msgRow] = VIEWPORT_TILES_W - 1;"""),
    ("gui/viewport.c",
     """			for (i = 0; i < 10; i++) {
				uint16 width;
				uint16 height;

				if (hasScrolled) {
					minX[i] = 0;
					maxX[i] = 14;
				}""",
     """			for (i = 0; i < VIEWPORT_TILES_H; i++) {
				uint16 width;
				uint16 height;

				if (hasScrolled) {
					minX[i] = 0;
					maxX[i] = VIEWPORT_TILES_W - 1;
				}"""),
    ("gui/viewport.c",
     "		GFX_PutPixel(x + 256, y + 136, colour & 0xFF);",
     "		GFX_PutPixel(x + 256 + SIDEBAR_DX, y + 136, colour & 0xFF);"),
    ("gui/viewport.c",
     """	GUI_Mouse_Hide_InWidget(3);
	GUI_Screen_Copy(32, 136, 32, 136, 8, 64, SCREEN_1, SCREEN_0);""",
     """	GUI_Mouse_Hide_InWidget(3);
	GUI_Screen_Copy(32 + SIDEBAR_DX / 8, 136, 32 + SIDEBAR_DX / 8, 136, 8, 64, SCREEN_1, SCREEN_0);"""),

    # ---- gui/gui.c
    ("gui/gui.c",
     "		int16 xOverlap = 15 - abs(xOffset);\n		int16 yOverlap = 10 - abs(yOffset);",
     "		int16 xOverlap = VIEWPORT_TILES_W - abs(xOffset);\n		int16 yOverlap = VIEWPORT_TILES_H - abs(yOffset);"),
    ("gui/gui.c",
     "		} else if (!g_viewport_forceRedraw && (xOverlap != 15 || yOverlap != 10)) {",
     "		} else if (!g_viewport_forceRedraw && (xOverlap != VIEWPORT_TILES_W || yOverlap != VIEWPORT_TILES_H)) {"),
    ("gui/gui.c",
     """		for (y = 0; y < 10; y++) {
			uint16 mapYBase = (y + viewportY) << 6;

			for (x = 0; x < 15; x++) {""",
     """		for (y = 0; y < VIEWPORT_TILES_H; y++) {
			uint16 mapYBase = (y + viewportY) << 6;

			for (x = 0; x < VIEWPORT_TILES_W; x++) {"""),
    ("gui/gui.c",
     """		for (xpos = 0; xpos < 14; xpos++) {
			uint16 v = g_minimapPosition + xpos + 6*64;""",
     """		for (xpos = 0; xpos < VIEWPORT_TILES_W - 1; xpos++) {
			uint16 v = g_minimapPosition + xpos + ((VIEWPORT_TILES_H * 6) / 10) * 64;"""),
    ("gui/gui.c",
     """		for (xpos = 0; xpos < 14; xpos++) {
			Map_Update(g_viewportPosition + xpos + 6 * 64, 0, true);""",
     """		for (xpos = 0; xpos < VIEWPORT_TILES_W - 1; xpos++) {
			Map_Update(g_viewportPosition + xpos + ((VIEWPORT_TILES_H * 6) / 10) * 64, 0, true);"""),
    ("gui/gui.c",
     "	l_info[7] = max;\n	l_info[6] = current;",
     "	l_info[7] = max;\n	l_info[6] = current;\n	l_info[0] = 293 + SIDEBAR_DX;"),
    ("gui/gui.c",
     """	oldScreenID = GFX_Screen_SetActive((screenID == SCREEN_0) ? SCREEN_1 : screenID);

	g_viewport_forceRedraw = true;

	Sprites_LoadImage("SCREEN.CPS", SCREEN_1, NULL);
	GUI_DrawSprite(SCREEN_1, g_sprites[11], 192, 0, 0, 0); /* "Credits" */""",
     """#if defined(__amigaos__)
	Layout_Set(true);
#endif
	oldScreenID = GFX_Screen_SetActive((screenID == SCREEN_0) ? SCREEN_1 : screenID);

	g_viewport_forceRedraw = true;

	Sprites_LoadImage("SCREEN.CPS", SCREEN_1, NULL);
#if defined(__amigaos__)
	Layout_ComposeBackground(SCREEN_1);
#endif
	GUI_DrawSprite(SCREEN_1, g_sprites[11], 192 + SIDEBAR_DX, 0, 0, 0); /* "Credits" */"""),
    ("gui/gui.c",
     """void GUI_ChangeSelectionType(uint16 selectionType)
{
	Screen oldScreenID;
""",
     """void GUI_ChangeSelectionType(uint16 selectionType)
{
	Screen oldScreenID;

#if defined(__amigaos__)
	/* the mentat, the factory window and the level end are 320x200 */
	if (selectionType == SELECTIONTYPE_MENTAT) Layout_Set(false);
#endif
"""),

    # ---- house.c: radar on/off animation
    ("house.c",
     "		WSA_DisplayFrame(wsa, activate ? frameCount - frame : frame, 256, 136, SCREEN_0);",
     "		WSA_DisplayFrame(wsa, activate ? frameCount - frame : frame, 256 + SIDEBAR_DX, 136, SCREEN_0);"),

    # ---- gui/mentat.c: the Mentat button leaves the map screen
    ("gui/mentat.c",
     """	VARIABLE_NOT_USED(w);

	g_cursorSpriteID = 0;

	Sprites_SetMouseSprite(0, 0, g_sprites[0]);""",
     """	VARIABLE_NOT_USED(w);

#if defined(__amigaos__)
	Layout_Set(false);
#endif
	g_cursorSpriteID = 0;

	Sprites_SetMouseSprite(0, 0, g_sprites[0]);"""),

    # ---- audio/sound.c: the distance from the middle of a large viewport
    ("audio/sound.c",
     "		volume = Tile_GetDistancePacked(g_minimapPosition, Tile_PackTile(position));",
     """#if defined(__amigaos__)
		volume = Tile_GetDistancePacked(g_minimapPosition + ((VIEWPORT_TILES_W - 15) / 2) + (((VIEWPORT_TILES_H - 10) / 2) << 6), Tile_PackTile(position));
#else
		volume = Tile_GetDistancePacked(g_minimapPosition, Tile_PackTile(position));
#endif"""),
]

# files that call Layout_*: include the header after their last local include
INCLUDES = ["gui/gui.c", "gui/mentat.c"]


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

    # gui/widget_draw.c: the action panel's absolute positions, lines 680..900
    p = os.path.join(SRC, "gui/widget_draw.c")
    lines = open(p, encoding="latin-1").read().replace("\r\n", "\n").split("\n")
    count = 0
    for i in range(670, min(900, len(lines))):
        l = lines[i]
        if "GUI_DrawLine(261, 95, 312, 95, 16)" in l:
            lines[i] = l.replace("GUI_DrawLine(261, 95, 312, 95, 16)", "GUI_DrawLine(261 + SIDEBAR_DX, 95, 312 + SIDEBAR_DX, 95, 16)")
            count += 1
            continue
        if "GUI_Draw" not in l:
            continue
        m = re.sub(r"(\]|\)|\"), (258|259|260|288|292|296), ", lambda mm: "%s, %s + SIDEBAR_DX, " % (mm.group(1), mm.group(2)), l, count=1)
        if m != l:
            lines[i] = m
            count += 1
    open(p, "w", encoding="latin-1", newline="\n").write("\n".join(lines))
    print("widget_draw.c: %d lines" % count)
    if count != 21:
        ok = False

    for f in INCLUDES:
        p = os.path.join(SRC, f)
        s = open(p, encoding="latin-1").read()
        idx = [m.end() for m in re.finditer(r'#include "[^"]+"\n', s)]
        s = s[:idx[-1]] + INC.replace('"gui/layout_amiga.h"', '"layout_amiga.h"') + s[idx[-1]:]
        open(p, "w", encoding="latin-1", newline="\n").write(s)

    mk = os.path.join(SRC, "..", "Makefile.amiga")
    m = open(mk).read()
    if "gui/layout_amiga.c" not in m:
        m = m.replace("gui/gui.c gui/mentat.c", "gui/gui.c gui/layout_amiga.c gui/mentat.c")
        open(mk, "w", newline="\n").write(m)
    print("ok" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
