"""Large screen sidebar, second layout (user's design, 28.10.2026): the radar at
the top of the sidebar, the action panel below it reaching down the screen,
a selected squad shown as a list of unit pictures two per row.
RADAR_Y (136 in the original layout) and PANEL_DY (0) become variables.
One-time edits of port/src; each replacement must match exactly once."""
import os, re, sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port", "src")

EDITS = [
    ("gfx.h",
     "#define SIDEBAR_DX g_sidebarDX			/* shift of the sidebar and radar (0) */",
     """#define SIDEBAR_DX g_sidebarDX			/* shift of the sidebar and radar (0) */
extern int16 g_radarY;
extern int16 g_panelDY;
#define RADAR_Y g_radarY			/* top of the radar (136) */
#define PANEL_DY g_panelDY			/* shift down of the action panel (0) */"""),
    ("gfx.h",
     "#define SIDEBAR_DX 0\n#endif",
     "#define SIDEBAR_DX 0\n#define RADAR_Y 136\n#define PANEL_DY 0\n#endif"),

    # radar position
    ("map.c",
     "		top    = (Tile_GetPackedY(packed) - mapInfo->minY) * (mapScale + 1) + 136;",
     "		top    = (Tile_GetPackedY(packed) - mapInfo->minY) * (mapScale + 1) + RADAR_Y;"),
    ("map.c",
     "		if (bottom > 136 + 63) bottom = 136 + 63;",
     "		if (bottom > RADAR_Y + 63) bottom = RADAR_Y + 63;"),
    ("map.c",
     "		GUI_Screen_Copy(32 + SIDEBAR_DX / 8, 136, 32 + SIDEBAR_DX / 8, 136, 8, 64, SCREEN_1, SCREEN_0);",
     "		GUI_Screen_Copy(32 + SIDEBAR_DX / 8, RADAR_Y, 32 + SIDEBAR_DX / 8, RADAR_Y, 8, 64, SCREEN_1, SCREEN_0);"),
    ("gui/viewport.c",
     "		y = min((max(y, 136) - 136) / (mapScale + 1), mapInfo->sizeY - 1) + mapInfo->minY;",
     "		y = min((max(y, RADAR_Y) - RADAR_Y) / (mapScale + 1), mapInfo->sizeY - 1) + mapInfo->minY;"),
    ("gui/viewport.c",
     "			if (minY < maxY) GUI_Screen_Copy(32 + SIDEBAR_DX / 8, 136 + minY, 32 + SIDEBAR_DX / 8, 136 + minY, 8,",
     "			if (minY < maxY) GUI_Screen_Copy(32 + SIDEBAR_DX / 8, RADAR_Y + minY, 32 + SIDEBAR_DX / 8, RADAR_Y + minY, 8,"),
    ("gui/viewport.c",
     "		GFX_PutPixel(x + 256 + SIDEBAR_DX, y + 136, colour & 0xFF);",
     "		GFX_PutPixel(x + 256 + SIDEBAR_DX, y + RADAR_Y, colour & 0xFF);"),
    ("gui/viewport.c",
     "	GUI_Screen_Copy(32 + SIDEBAR_DX / 8, 136, 32 + SIDEBAR_DX / 8, 136, 8, 64, SCREEN_1, SCREEN_0);",
     "	GUI_Screen_Copy(32 + SIDEBAR_DX / 8, RADAR_Y, 32 + SIDEBAR_DX / 8, RADAR_Y, 8, 64, SCREEN_1, SCREEN_0);"),
    ("house.c",
     "		WSA_DisplayFrame(wsa, activate ? frameCount - frame : frame, 256 + SIDEBAR_DX, 136, SCREEN_0);",
     "		WSA_DisplayFrame(wsa, activate ? frameCount - frame : frame, 256 + SIDEBAR_DX, RADAR_Y, SCREEN_0);"),

    # the progress bar: moved with the panel, or placed by the squad list
    ("gui/gui.c",
     "	l_info[0] = 293 + SIDEBAR_DX;",
     """	l_info[0] = 293 + SIDEBAR_DX;
#if defined(__amigaos__)
	l_info[1] = (g_progressbarY >= 0) ? g_progressbarY : 52 + PANEL_DY;
#endif"""),
    ("gui/gui.c",
     "void GUI_DrawProgressbar(uint16 current, uint16 max)\n{",
     "#if defined(__amigaos__)\nint16 g_progressbarY = -1;	/* >= 0: the squad list's bar (rts_amiga.c) */\n#endif\n\nvoid GUI_DrawProgressbar(uint16 current, uint16 max)\n{"),
    # credits: staged in widget 4, shown in widget 5 (was widget 4's y - 40)
    ("gui/gui.c",
     "		GUI_Screen_Copy(g_curWidgetXBase, g_curWidgetYBase, g_curWidgetXBase, g_curWidgetYBase - 40, g_curWidgetWidth, g_curWidgetHeight, SCREEN_ACTIVE, oldScreenID);",
     "		GUI_Screen_Copy(g_curWidgetXBase, g_curWidgetYBase, g_curWidgetXBase, g_widgetProperties[5].yBase, g_curWidgetWidth, g_curWidgetHeight, SCREEN_ACTIVE, oldScreenID);"),

    # action panel: a squad is drawn by rts_amiga.c, the rest moved down
    ("gui/widget_draw.c",
     """		if (actionType == 2 && squad > 1) {
			RTS_DrawSquadPicture(258 + SIDEBAR_DX, 51);
		} else
#endif""",
     """		RTS_PlaceSquadButtons(-1);
		if (actionType == 2 && squad > 1) {
			RTS_PlaceSquadButtons(RTS_DrawSquadList(256 + SIDEBAR_DX, 51 + PANEL_DY));
			spriteID = 0xFFFF;
		} else
#endif"""),
    ("gui/widget_draw.c",
     """		if (actionType == 2 && squad > 1) {
				GUI_DrawProgressbar(RTS_GroupHealthPercent(), 100);	/* the average */
			} else
#endif
			GUI_DrawProgressbar(o->hitpoints, oi->hitpoints);""",
     """		if (actionType == 2 && squad > 1) {
				/* drawn with the squad list */
			} else
#endif
			{
			GUI_DrawProgressbar(o->hitpoints, oi->hitpoints);"""),
]


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

    # widget_draw.c: close the block opened above around the DMG sprite and text
    p = os.path.join(SRC, "gui/widget_draw.c")
    s = open(p, encoding="latin-1").read()
    old = """			GUI_DrawSprite(SCREEN_ACTIVE, g_sprites[27], 292 + SIDEBAR_DX, 60, 0, 0);
			GUI_DrawText_Wrapper(String_Get_ByIndex(STR_DMG), 296 + SIDEBAR_DX, 65, 29, 0, 0x11);"""
    new = """			GUI_DrawSprite(SCREEN_ACTIVE, g_sprites[27], 292 + SIDEBAR_DX, 60, 0, 0);
			GUI_DrawText_Wrapper(String_Get_ByIndex(STR_DMG), 296 + SIDEBAR_DX, 65, 29, 0, 0x11);
			}"""
    if s.count(old) != 1:
        print("widget_draw.c: DMG block not found")
        ok = False
    s = s.replace(old, new)
    # every y of the panel: + PANEL_DY
    lines = s.split("\n")
    count = 0
    for i, l in enumerate(lines):
        if "+ SIDEBAR_DX" not in l or "PANEL_DY" in l:
            continue
        m = l
        m = m.replace("GUI_DrawLine(261 + SIDEBAR_DX, 95, 312 + SIDEBAR_DX, 95, 16)",
                      "GUI_DrawLine(261 + SIDEBAR_DX, 95 + PANEL_DY, 312 + SIDEBAR_DX, 95 + PANEL_DY, 16)")
        m = re.sub(r"(\d+ \+ SIDEBAR_DX), (\d+), ", r"\1, \2 + PANEL_DY, ", m, count=1)
        m = m.replace("g_fontCurrent->height * 2 + 80,", "g_fontCurrent->height * 2 + 80 + PANEL_DY,")
        m = m.replace("g_fontCurrent->height * 3 + 80,", "g_fontCurrent->height * 3 + 80 + PANEL_DY,")
        if m != l:
            lines[i] = m
            count += 1
    open(p, "w", encoding="latin-1", newline="\n").write("\n".join(lines))
    print("widget_draw.c: %d lines moved with the panel" % count)

    # gui.h: g_progressbarY
    p = os.path.join(SRC, "gui/gui.h")
    s = open(p, encoding="latin-1").read()
    if "g_progressbarY" not in s:
        s = s.replace("extern void GUI_DrawProgressbar(", "#if defined(__amigaos__)\nextern int16 g_progressbarY;\n#endif\nextern void GUI_DrawProgressbar(", 1)
        open(p, "w", encoding="latin-1", newline="\n").write(s)
    print("ok" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
