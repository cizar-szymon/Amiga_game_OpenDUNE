/** @file src/gui/layout_amiga.c The large in-game screen (AmigaOS).
 *
 * The map screen is drawn at LAYOUT_BIG_WIDTH x LAYOUT_BIG_HEIGHT (640x480)
 * with pixels 1:1: the viewport grows to 35x27 tiles, the sidebar moves to
 * the right edge with the radar at its top (RADAR_Y) and the action panel
 * below it reaching down the screen (PANEL_DY, height), where a selected
 * squad is listed (rts_amiga.c); the top bar spans the width. Menus, the mentat, the
 * factory window and cutscenes stay 320x200 (the video driver doubles them).
 *
 * Layout_Set(true) is called when the map screen is (re)drawn
 * (GUI_DrawInterfaceAndRadar), Layout_Set(false) when it is left for a 320x200
 * screen (GUI_ChangeSelectionType(SELECTIONTYPE_MENTAT), the Mentat button).
 * Positions in the tables are moved from their original values, which are kept.
 *
 * Zoom (as in the Settlers port: Z steps x1 -> x2 -> x3 -> x1, the mouse wheel
 * one step in or out): the viewport is drawn with fewer tiles (logical, in
 * SCREEN_1 at its usual place) and its copy to SCREEN_0 is stretched over the
 * whole viewport (Layout_ZoomCopy, from GUI_Screen_Copy); windows, the panel
 * and the pointer stay 1:1. The pointer over the viewport is turned into the
 * logical picture's pixels where the game turns it into tiles
 * (Layout_ViewPoint). Only the picture changes: in a LAN game too.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "../os/common.h"
#include "../os/math.h"
#include "../gfx.h"
#include "../file.h"
#include "../sprites.h"
#include "font.h"
#include "../table/strings.h"
#include "../map.h"
#include "../scenario.h"
#include "../tile.h"
#include "../input/mouse.h"
#include "gui.h"
#include "widget.h"
#include "layout_amiga.h"
#include "../house.h"
#include "../opendune.h"

extern bool Video_ScrollView(int16 dx, int16 dy, int16 x, int16 y, int16 w, int16 h);

uint16 g_viewportTilesW = 15;
uint16 g_viewportTilesH = 10;
int g_viewZoom = 1;			/* the zoom in force (1 off the large map screen) */
int g_zoomCopy = 0;			/* > 0: a copy of the viewport to SCREEN_0 (stretched when zoomed) */
static int s_zoomWanted = 1;		/* the player's choice */
static uint16 s_fullTilesW = 15, s_fullTilesH = 10;	/* the viewport's tiles at x1 */
static volatile int s_zoomRequest;	/* from the video driver: 1 Z, 2 wheel in, 3 wheel out */
int16 g_sidebarDX = 0;
int16 g_radarY = 136;
int16 g_panelDY = 0;

/* the large sidebar: radar at the top, the bar of the original between radar
 * and panel, the panel below */
#define BIG_RADAR_Y   42
#define BIG_BAR_SRC   124	/* the bar's rows in SCREEN.CPS: 124..135 */
#define BIG_BAR_ROWS  12
#define BIG_PANEL_Y   (BIG_RADAR_Y + 64 + BIG_BAR_ROWS)

static bool s_big = false;

extern void Video_Amiga_ScreenSize(int *w, int *h);
extern bool Video_Amiga_SetScreen(int w, int h);
static bool s_saved = false;
static WidgetProperties s_props[24];		/* g_widgetProperties as in the original */

/* The live game widgets moved with the sidebar (g_table_gameWidgetInfo). */
static const uint16 s_sidebarWidgets[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 44 };

static void Layout_Widget(uint16 index, int16 x, int16 y, uint16 w, uint16 h)
{
	Widget *wi = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, index);
	if (wi == NULL) return;
	wi->offsetX = x;
	wi->offsetY = y;
	wi->width = w;
	wi->height = h;
}

/* The game widgets: original places from g_table_gameWidgetInfo + the shift. */
static void Layout_GameWidgets(void)
{
	const WidgetInfo *info;
	uint16 vw = s_fullTilesW * 16, vh = s_fullTilesH * 16;	/* on the screen (zoomed or not) */
	unsigned i;

	for (info = g_table_gameWidgetInfo; info->index >= 0; info++) {
		Widget *w = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, info->index);
		if (w == NULL) continue;
		w->offsetX = info->offsetX;
		w->offsetY = info->offsetY;
		for (i = 0; i < lengthof(s_sidebarWidgets); i++) {
			if (s_sidebarWidgets[i] == info->index) w->offsetX = info->offsetX + g_sidebarDX;
		}
		if (info->index >= 3 && info->index <= 11) w->offsetY = info->offsetY + g_panelDY;
		if (info->index == 44) w->offsetY = g_radarY;
	}
	if (!s_big) {
		/* scroll edges, viewport and full screen as in the original */
		for (info = g_table_gameWidgetInfo; info->index >= 0; info++) {
			if (info->index >= 39 && info->index <= 45) {
				Layout_Widget(info->index, info->offsetX + ((info->index == 44) ? g_sidebarDX : 0), info->offsetY, info->width, info->height);
			}
		}
		return;
	}
	Layout_Widget(39, 0, 24, vw, 16);			/* scroll up */
	Layout_Widget(40, vw, 40, 10, vh);			/* scroll right */
	Layout_Widget(41, 0, 40, 2, vh);			/* scroll left */
	Layout_Widget(42, 0, SCREEN_HEIGHT - 2, vw, 2);	/* scroll down */
	Layout_Widget(43, 0, 40, vw, vh);			/* viewport */
	Layout_Widget(45, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void Layout_Properties(void)
{
	int dx8 = g_sidebarDX / 8;
	int i;

	memcpy(g_widgetProperties, s_props, sizeof(s_props));
	/* 10: the Options window of six entries (Resolution, no Load / Save) */
	g_widgetProperties[10] = g_widgetProperties[16];
	g_widgetProperties[10].yBase = 41;
	g_widgetProperties[10].height = 117;
	if (!s_big) return;

	g_widgetProperties[0].width = SCREEN_WIDTH / 8;
	g_widgetProperties[0].height = SCREEN_HEIGHT;
	/* message box: centred over the viewport */
	g_widgetProperties[1].xBase += (s_fullTilesW * 16 - 240) / 16;
	g_widgetProperties[1].yBase += (s_fullTilesH * 16 - 160) / 2;
	g_widgetProperties[2].width = g_viewportTilesW * 2;
	g_widgetProperties[2].height = g_viewportTilesH * 16;
	for (i = 3; i <= 6; i++) g_widgetProperties[i].xBase += dx8;	/* radar, credits, action panel */
	g_widgetProperties[3].yBase = g_radarY;
	g_widgetProperties[4].yBase += g_panelDY;	/* credits staged in the panel's back buffer */
	g_widgetProperties[6].yBase += g_panelDY;
	g_widgetProperties[6].height = SCREEN_HEIGHT - 8 - g_widgetProperties[6].yBase;
	g_widgetProperties[7].width = SCREEN_WIDTH / 8 - 2;			/* text line */
	for (i = 10; i <= 22; i++) {						/* windows: centred */
		if (i != 10 && (i < 14 || i > 18) && i != 22) continue;
		g_widgetProperties[i].xBase += (SCREEN_WIDTH - 320) / 16;
		g_widgetProperties[i].yBase += (SCREEN_HEIGHT - 200) / 2;
	}
}

/** Keeps the viewport on the map (it may be larger than a small map). */
void Layout_ClampViewport(void)
{
	const MapInfo *mapInfo = &g_mapInfos[g_scenario.mapScale];
	int16 x = Tile_GetPackedX(g_viewportPosition);
	int16 y = Tile_GetPackedY(g_viewportPosition);

	x = min(x, (int16)(mapInfo->minX + mapInfo->sizeX - g_viewportTilesW));
	y = min(y, (int16)(mapInfo->minY + mapInfo->sizeY - g_viewportTilesH));
	x = max(x, (int16)mapInfo->minX);
	y = max(y, (int16)mapInfo->minY);
	g_viewportPosition = Tile_PackXY(x, y);
	g_minimapPosition = g_viewportPosition;
}

void Layout_Set(bool big)
{
	if (!s_saved) {
		memcpy(s_props, g_widgetProperties, sizeof(s_props));
		s_saved = true;
	}
	if (big == s_big) {
		int w, h;
		if (!big) return;
		/* the map screen again after another resolution was chosen */
		Video_Amiga_ScreenSize(&w, &h);
		if (SCREEN_WIDTH == min(w, SCREEN_WIDTH_MAX) && SCREEN_HEIGHT == min(h, SCREEN_HEIGHT_MAX)) return;
		s_big = false;
	}

	GUI_Mouse_Hide_Safe();
	s_big = big;
	if (big) {
		int w, h;
		Video_Amiga_ScreenSize(&w, &h);		/* the map screen 1:1: as large as the screen */
		GFX_SetScreenSize((uint16)min(w, SCREEN_WIDTH_MAX), (uint16)min(h, SCREEN_HEIGHT_MAX));
		Mouse_SetRegion(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
		s_fullTilesW = (SCREEN_WIDTH - 80) / 16;
		s_fullTilesH = (SCREEN_HEIGHT - 40) / 16;
		g_viewZoom = min(s_zoomWanted, Layout_MaxZoom());
		if (g_viewZoom < 1) g_viewZoom = 1;
		g_viewportTilesW = (s_fullTilesW + g_viewZoom - 1) / g_viewZoom;
		g_viewportTilesH = (s_fullTilesH + g_viewZoom - 1) / g_viewZoom;
		g_sidebarDX = SCREEN_WIDTH - 320;
		g_radarY = BIG_RADAR_Y;
		g_panelDY = BIG_PANEL_Y - 42;
	} else {
		GFX_SetScreenSize(320, 200);
		Mouse_SetRegion(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
		s_fullTilesW = g_viewportTilesW = 15;
		s_fullTilesH = g_viewportTilesH = 10;
		g_viewZoom = 1;
		g_sidebarDX = 0;
		g_radarY = 136;
		g_panelDY = 0;
	}
	GUI_SetClippingArea(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
	Layout_Properties();
	Layout_GameWidgets();
	if (big) Layout_ClampViewport();
	GUI_Mouse_Show_Safe();
}

bool Layout_IsBig(void)
{
	return s_big;
}

/**
 * SCREEN.CPS (320x200, loaded at the start of the buffer as 320 bytes per
 * row) laid out for the large screen: the left of the top bar as it is, its
 * right end and the sidebar moved to the right edge, the plain part of the
 * top bar repeated in between, the strip between viewport and sidebar
 * continued downwards; in the sidebar the radar at the top, the bar below it,
 * the panel's frame below that and the bar again at the bottom.
 */
void Layout_ComposeBackground(Screen screenID)
{
	uint8 *buf = GFX_Screen_Get_ByIndex(screenID);
	uint8 *src;
	int x, y, dx = g_sidebarDX;

	if (!s_big) return;
	src = malloc(320 * 200);
	if (src == NULL) return;
	memcpy(src, buf, 320 * 200);
	memset(buf, 0, SCREEN_WIDTH * SCREEN_HEIGHT);

	for (y = 0; y < SCREEN_HEIGHT; y++) {
		uint8 *d = buf + y * SCREEN_WIDTH;
		if (y < 200) {
			const uint8 *s = src + y * 320;
			memcpy(d, s, 184);
			for (x = 184; x < 184 + dx; x++) d[x] = s[184 + (x & 7)];
			memcpy(d + 184 + dx, s + 184, 320 - 184);
		} else {
			/* the strip at 240..255 of the original, rows 136..199 repeated */
			const uint8 *s = src + (136 + (y - 200) % 64) * 320;
			memcpy(d + 240 + dx, s + 240, 16);
		}
		{
			int sr;
			if (y < BIG_RADAR_Y) sr = y;
			else if (y < BIG_RADAR_Y + 64) sr = 136 + y - BIG_RADAR_Y;
			else if (y < BIG_PANEL_Y) sr = BIG_BAR_SRC + y - BIG_RADAR_Y - 64;
			else if (y < SCREEN_HEIGHT - 8) sr = 42 + min(y - BIG_PANEL_Y, 81);
			else sr = BIG_BAR_SRC + y - (SCREEN_HEIGHT - 8);
			memcpy(d + 256 + dx, src + sr * 320 + 256, 64);
		}
	}
	free(src);
}

/* ---- the resolution (Options -> Resolution), kept in the save directory ---- */

#define RESOLUTION_FILE "RESOLUTION.CFG"

/* the resolutions of the Settlers port: 4:3 and 16:9 */
const uint16 g_layoutResolutions[LAYOUT_RESOLUTIONS][2] = {
	{ 400, 300 }, { 640, 400 }, { 640, 480 }, { 800, 600 }, { 640, 360 }, { 960, 540 }
};

/** The resolution chosen in Options (Video_Init, when the ini has no screenmode). */
bool Layout_SavedResolution(int *w, int *h)
{
	char text[16];
	uint8 fd;
	uint32 n;
	char *x;

	if (!File_Exists_Personal(RESOLUTION_FILE)) return false;
	fd = File_Open_Personal(RESOLUTION_FILE, FILE_MODE_READ);
	if (fd == 0xFF) return false;
	n = File_Read(fd, text, sizeof(text) - 1);
	File_Close(fd);
	text[n] = '\0';
	x = strchr(text, 'x');
	if (x == NULL) return false;
	*w = atoi(text);
	*h = atoi(x + 1);
	return true;
}

/**
 * Another resolution: the screen opened again, the choice kept; the map
 * screen is laid out again by the next GUI_DrawInterfaceAndRadar. False when
 * the screen mode is not there.
 */
bool Layout_SetResolution(int w, int h)
{
	char text[16];
	uint8 fd;
	int n;

	if (!Video_Amiga_SetScreen(w, h)) return false;
	n = sprintf(text, "%dx%d\n", w, h);
	fd = File_Open_Personal(RESOLUTION_FILE, FILE_MODE_WRITE);
	if (fd != 0xFF) {
		File_Write(fd, text, n);
		File_Close(fd);
	}
	return true;
}

/* ---- the LOAD and SAVE buttons of the top bar (large map screen only) ---- */

#define BUTTON_X0   190	/* after OPTIONS (104 + 78) */
#define BUTTON_W    78	/* as MENTAT and OPTIONS */
#define BUTTON_GAP  8

extern bool GUI_Widget_LoadSave_Click(Widget *w);
extern bool GUI_Widget_LoadGame_Click(Widget *w);
extern bool GUI_Widget_SaveGame_Click(Widget *w);

/* Widgets 12 and 13: below 39..45 (45 covers the screen and takes clicks first). */
typedef struct { uint16 index; const char *word; bool (*click)(Widget *); } TopButton;
static const TopButton s_buttons[2] = {
	{ 12, "LOAD", &GUI_Widget_LoadGame_Click },
	{ 13, "SAVE", &GUI_Widget_SaveGame_Click }
};
static bool s_twoButtons = true;	/* else one LOAD/SAVE (no room at all) */
static int s_buttonStyle = 2;		/* 2 large lettering, 1 small (8 pixel font), 0 one LOAD/SAVE */
static int16 s_btnX[2] = { BUTTON_X0, BUTTON_X0 + BUTTON_W + BUTTON_GAP };	/* as drawn */
static int16 s_btnW[2] = { BUTTON_W, BUTTON_W };
#define BUTTON_MARGIN 7	/* lettering to the button's ends */
static uint8 s_barSave[16][BUTTON_W];	/* the top bar under the sampled button */

/* Places on the top bar: MENTAT, OPTIONS, LOAD, the gap between LOAD and SAVE,
 * the lettering's margin. The original's places first; where the large
 * LOAD / SAVE do not fit there (400x300) MENTAT and OPTIONS move left and
 * closer together. */
typedef struct { int16 mentatX, optionsX, loadX, gap, margin; } BarLayout;
static const BarLayout s_barLayouts[] = {
	{ 16, 104, BUTTON_X0, BUTTON_GAP, BUTTON_MARGIN },
	{  2,  83, 164, 3, 5 },
	{  2,  83, 164, 3, 4 },
	{  1,  81, 161, 2, 4 }
};
static const BarLayout *s_bar = &s_barLayouts[0];

/* MENTAT and OPTIONS where the bar's layout puts them. */
static void Layout_PlaceBarWidgets(void)
{
	Widget *w = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, 1);
	if (w != NULL && w->index == 1) w->offsetX = s_bar->mentatX;
	w = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, 2);
	if (w != NULL && w->index == 2) w->offsetX = s_bar->optionsX;
}

static Widget *Layout_ButtonWidget(uint16 index)
{
	Widget *w = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, index);
	return (w != NULL && w->index == index) ? w : NULL;
}

/* The buttons' widgets: made once, clickable where the buttons are drawn. */
static void Layout_MakeButtonWidgets(void)
{
	Widget *mentat = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, 1);
	int i;

	if (mentat == NULL) return;
	for (i = 0; i < 2; i++) {
		Widget *w;
		if (Layout_ButtonWidget(s_buttons[i].index) != NULL) continue;
		w = GUI_Widget_Allocate(s_buttons[i].index, 0, BUTTON_X0, 1, 0xFFFF, STR_NULL);	/* drawn by Layout_DrawLoadSaveButton */
		w->width = BUTTON_W;
		w->height = 16;
		w->flags = mentat->flags;
		g_widgetLinkedListHead = GUI_Widget_Insert(g_widgetLinkedListHead, w);
	}
}

/** In the game loop: the buttons only on the large map screen, when MENTAT is shown. */
void Layout_UpdateLoadSaveButton(void)
{
	Widget *mentat;
	int i;

	if (g_widgetLinkedListHead == NULL) return;
	mentat = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, 1);
	if (mentat == NULL) return;
	if (s_big) {
		Layout_MakeButtonWidgets();
		Layout_PlaceBarWidgets();
	}
	for (i = 0; i < 2; i++) {
		Widget *w = Layout_ButtonWidget(s_buttons[i].index);
		if (w == NULL) continue;
		w->offsetX = s_btnX[i];
		w->width = s_btnW[i];
		w->height = 16;
		if (LAN_GAME) {
			/* no saving in a LAN game: not there at all (an invisible widget still drew its frame) */
			w->width = 0;
			w->height = 0;
		}
		if (s_twoButtons) {
			w->clickProc = s_buttons[i].click;
			w->flags.invisible = !s_big || mentat->flags.invisible || LAN_GAME;
		} else {
			/* one button: LOAD/SAVE with the small window */
			w->clickProc = &GUI_Widget_LoadSave_Click;
			w->flags.invisible = !s_big || mentat->flags.invisible || i > 0 || LAN_GAME;
		}
	}
}

/* Glyphs of the MENTAT (sprite 7) and OPTIONS (sprite 9) buttons' lettering:
 * columns from the sprite's left edge, the lettering in rows 2..11. */
typedef struct { uint8 sprite, from, to; } ButtonGlyph;
static const ButtonGlyph s_glyphO = { 9,  7, 15 };
static const ButtonGlyph s_glyphA = { 7, 49, 62 };
static const ButtonGlyph s_glyphS = { 9, 64, 71 };
static const ButtonGlyph s_glyphE = { 7, 18, 27 };
static const ButtonGlyph s_glyphI = { 9, 35, 41 };	/* the stem of I: L, D */
static const ButtonGlyph s_glyphOR = { 9, 11, 15 };	/* the right half of O: D */

#define GLYPH_TOP 3	/* the lettering's first row on the screen, as MENTAT / OPTIONS */
#define GLYPH_H   10

/* A word is put together at full size in s_letter, the letters close
 * together as in MENTAT / OPTIONS, and drawn a little bold (each pixel also
 * one to the right). */
#define LETTER_W 160
static uint8 s_letter[GLYPH_H][LETTER_W];
static uint8 s_glyphBg;

/* A glyph's lettering pixels (all but the button's background, in their own
 * colours) from the scratch screen (sprite 7 at y 0, sprite 9 at y 20) into
 * s_letter at x; returns its width. */
static int Layout_PutGlyph(const uint8 *scratch, const ButtonGlyph *g, int x)
{
	int sy = (g->sprite == 7) ? 2 : 22, c, r;
	for (c = g->from; c <= g->to; c++) {
		for (r = 0; r < GLYPH_H; r++) {
			uint8 v = scratch[(sy + r) * SCREEN_WIDTH + c];
			int lx = x + c - g->from;
			if (v != 0 && v != s_glyphBg && lx >= 0 && lx < LETTER_W) s_letter[r][lx] = v;
		}
	}
	return g->to - g->from + 1;
}

/* A stroke in s_letter, `thick` pixels wide. */
static void Layout_Stroke(int x1, int y1, int x2, int y2, int thick, uint8 fg)
{
	int dx = abs(x2 - x1), dy = abs(y2 - y1), sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
	int err = dx - dy, t;
	for (;;) {
		for (t = 0; t < thick; t++) {
			if (y1 >= 0 && y1 < GLYPH_H && x1 + t >= 0 && x1 + t < LETTER_W) s_letter[y1][x1 + t] = fg;
		}
		if (x1 == x2 && y1 == y2) break;
		if (2 * err > -dy) { err -= dy; x1 += sx; }
		if (2 * err < dx) { err += dx; y1 += sy; }
	}
}

/* A word of L, O, A, D, S, V, E in s_letter from the letters of MENTAT and
 * OPTIONS (L, D and V from their parts and strokes); returns its width. */
static int Layout_MakeWord(const uint8 *scratch, const char *word, uint8 fg)
{
	int x = 0, w, bottom = GLYPH_H - 1;

	memset(s_letter, 0, sizeof(s_letter));
	for (; *word != '\0'; word++) {
		switch (*word) {
			case 'L':	/* the stem of I, a foot to the right with a serif */
				w = Layout_PutGlyph(scratch, &s_glyphI, x);
				Layout_Stroke(x + 5, bottom, x + w + 2, bottom, 1, fg);
				Layout_Stroke(x + w + 2, bottom - 2, x + w + 2, bottom, 1, fg);
				x += w + 3;
				break;
			case 'O': x += Layout_PutGlyph(scratch, &s_glyphO, x) - 1; break;
			case 'A': x += Layout_PutGlyph(scratch, &s_glyphA, x) - 1; break;
			case 'S': x += Layout_PutGlyph(scratch, &s_glyphS, x) - 1; break;
			case 'E': x += Layout_PutGlyph(scratch, &s_glyphE, x) - 1; break;
			case 'D':	/* the stem of I and the right half of O */
				x += Layout_PutGlyph(scratch, &s_glyphI, x) - 2;
				x += Layout_PutGlyph(scratch, &s_glyphOR, x) + 1;
				break;
			case 'V':	/* two strokes meeting at the bottom, serifs at the top */
				Layout_Stroke(x + 1, 0, x + 5, bottom, 2, fg);
				Layout_Stroke(x + 10, 0, x + 6, bottom, 1, fg);
				Layout_Stroke(x, 0, x + 3, 0, 1, fg);
				Layout_Stroke(x + 9, 0, x + 11, 0, 1, fg);
				x += 12;
				break;
			default: break;
		}
	}
	return x + 1;	/* + the bold column */
}

/* s_letter (w columns) drawn at x, y, a little bold. */
static void Layout_DrawWord(uint8 *buf, int w, int x, int y)
{
	int ox, oy;
	for (oy = 0; oy < GLYPH_H; oy++) {
		for (ox = 0; ox < w - 1; ox++) {
			uint8 v = s_letter[oy][ox];
			if (v == 0) continue;
			buf[(y + oy) * SCREEN_WIDTH + x + ox] = v;
			buf[(y + oy) * SCREEN_WIDTH + x + ox + 1] = v;
		}
	}
}

/**
 * The buttons drawn into the top bar's back buffer (GUI_DrawInterfaceAndRadar):
 * the OPTIONS button's picture with its lettering replaced by "LOAD" / "SAVE"
 * put together from the letters of MENTAT and OPTIONS, in the button's own
 * colours; where the top bar has room for one button only (400x300) one
 * LOAD/SAVE in the 8 pixel font, which opens the small window.
 */
void Layout_DrawLoadSaveButton(Screen screenID)
{
	uint16 count[256];
	uint8 *buf, *scratch;
	uint8 bg = 0, fg = 0, shade = 0;
	int x, y, i, n, room;
	Screen old;

	s_bar = &s_barLayouts[0];
	if (!s_big || LAN_GAME) return;	/* (a LAN game: no saving) */
	buf = GFX_Screen_Get_ByIndex(screenID);
	scratch = GFX_Screen_Get_ByIndex(SCREEN_3);	/* credits and intro only */

	/* both original buttons in the scratch screen, for their letters */
	for (y = 0; y < 40; y++) memset(scratch + y * SCREEN_WIDTH, 0, 80);
	GUI_DrawSprite(SCREEN_3, g_sprites[7], 0, 0, 0, 0);
	GUI_DrawSprite(SCREEN_3, g_sprites[9], 0, 20, 0, 0);
	/* the colours from the OPTIONS button drawn where ours go, the top bar
	 * there put back afterwards */
	for (y = 0; y < 16; y++) memcpy(s_barSave[y], buf + (1 + y) * SCREEN_WIDTH + BUTTON_X0, BUTTON_W);
	GUI_DrawSprite(screenID, g_sprites[9], BUTTON_X0, 1, 0, 0);
	memset(count, 0, sizeof(count));
	for (y = 3; y < 15; y++) {
		for (x = BUTTON_X0 + 3; x < BUTTON_X0 + 75; x++) count[buf[y * SCREEN_WIDTH + x]]++;
	}
	for (y = 0; y < 16; y++) memcpy(buf + (1 + y) * SCREEN_WIDTH + BUTTON_X0, s_barSave[y], BUTTON_W);
	for (i = 0; i < 256; i++) if (count[i] > count[bg]) bg = (uint8)i;
	fg = (bg == 0) ? 1 : 0;
	for (i = 0; i < 256; i++) if (i != bg && count[i] > count[fg]) fg = (uint8)i;
	/* the third colour: the lettering's dark gold edge (the small font's shadow) */
	for (i = 0; i < 256; i++) if (i != bg && i != fg && (shade == bg || shade == fg || count[i] > count[shade])) shade = (uint8)i;
	s_glyphBg = bg;

	room = 192 + g_sidebarDX - 4;	/* "Credits" */
	{
		/* large: the words from MENTAT / OPTIONS, MENTAT and OPTIONS moved
		 * left if needed (400x300); else the 8 pixel font; one LOAD/SAVE only
		 * if not even that fits */
		int tw[2], ws[2];
		unsigned k;
		GUI_DrawText_Wrapper(NULL, 0, 0, 0, 0, 0x22);
		for (i = 0; i < 2; i++) {
			tw[i] = Layout_MakeWord(scratch, s_buttons[i].word, fg);
			ws[i] = Font_GetStringWidth(s_buttons[i].word) + 8;
		}
		s_buttonStyle = -1;
		for (k = 0; k < lengthof(s_barLayouts); k++) {
			const BarLayout *b = &s_barLayouts[k];
			if (b->loadX + tw[0] + tw[1] + 4 * b->margin + b->gap <= room) {
				s_bar = b;
				s_buttonStyle = 2;
				break;
			}
		}
		if (s_buttonStyle == 2) {
		} else if (BUTTON_X0 + ws[0] + 4 + ws[1] <= room) {
			s_buttonStyle = 1;
		} else {
			s_buttonStyle = 0;
		}
		s_twoButtons = s_buttonStyle != 0;
		n = s_twoButtons ? 2 : 1;

		old = GFX_Screen_SetActive(screenID);
		{
			int bx = s_bar->loadX;
			for (i = 0; i < n; i++) {
				int c, r, w, lw = 0;
				if (s_buttonStyle == 2) {
					lw = Layout_MakeWord(scratch, s_buttons[i].word, fg);
					w = lw + 2 * s_bar->margin;		/* as narrow as its word */
				} else if (s_buttonStyle == 1) {
					w = ws[i];
				} else {
					w = BUTTON_W;
				}
				s_btnX[i] = (int16)bx;
				s_btnW[i] = (int16)w;
				/* the button: the OPTIONS picture's left and right ends (scratch,
				 * y 20), as wide as w; the inside in the background colour */
				for (r = 0; r < 16; r++) {
					for (c = 0; c < w; c++) {
						int sc = (c < w / 2) ? c : BUTTON_W - (w - c);
						uint8 v = scratch[(20 + r) * SCREEN_WIDTH + sc];
						if (v != 0) buf[(1 + r) * SCREEN_WIDTH + bx + c] = v;
					}
				}
				for (y = 3; y < 15; y++) memset(buf + y * SCREEN_WIDTH + bx + 3, bg, w - 6);
				if (s_buttonStyle == 2) {
					Layout_DrawWord(buf, lw, bx + s_bar->margin, GLYPH_TOP);
				} else {
					/* as the OPTIONS lettering: its colour, a dark gold edge
					 * (not the font's black outline), at the top of the button */
					const char *word = s_buttonStyle == 1 ? s_buttons[i].word : "LOAD/SAVE";
					GUI_DrawText_Wrapper(word, bx + w / 2 + 1, 4, shade, 0, 0x112);
					GUI_DrawText_Wrapper(word, bx + w / 2, 3, fg, 0, 0x112);
				}
				bx += w + (s_buttonStyle == 2 ? s_bar->gap : 4);
			}
		}
	}
	GFX_Screen_SetActive(old);
	Layout_PlaceBarWidgets();	/* drawn next by GUI_DrawInterfaceAndRadar */
	GFX_Screen_SetDirty(screenID, 0, 1, s_btnX[n - 1] + s_btnW[n - 1], 17);
}

/**
 * The large sidebar's action panel reaches the bottom of the screen; below
 * its contents (the original's 82 rows, or the lowest visible button of the
 * panel) it is black, its frame ending there (GUI_Widget_ActionPanel_Draw,
 * on the active screen before the panel is copied to the screen).
 */
void Layout_PanelBlackBelow(void)
{
	const WidgetProperties *wp = &g_widgetProperties[6];
	int16 left = wp->xBase << 3, top = wp->yBase, width = wp->width << 3, bottom = wp->yBase + wp->height;
	int16 end = top + 82;
	uint16 i;

	if (!s_big) return;
	for (i = 3; i <= 11; i++) {
		Widget *w = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, i);
		if (w == NULL || w->index != i || w->flags.invisible) continue;
		if (w->offsetY + (int16)w->height + 3 > end) end = w->offsetY + w->height + 3;
	}
	if (end >= bottom) return;
	GUI_DrawFilledRectangle(left, end, left + width - 1, bottom - 1, 12);
	GUI_DrawBorder(left, top, width, end - top, 1, false);
}

/* ---- zoom of the map view ---- */

/** The largest zoom of the map screen (as the Settlers port): 4:3 above 400
 * wide x2, from 800 x3; 16:9 from 960 x3 (640x360 none). */
int Layout_MaxZoom(void)
{
	if (!s_big) return 1;
	if (SCREEN_WIDTH * 9 == SCREEN_HEIGHT * 16) return SCREEN_WIDTH >= 960 ? 3 : 1;
	if (SCREEN_WIDTH >= 800) return 3;
	if (SCREEN_WIDTH > 400) return 2;
	return 1;
}

/** Z (step 0) or the wheel (+1 / -1), from the video driver's key handling. */
void Layout_ZoomRequest(int step)
{
	s_zoomRequest = step == 0 ? 1 : step > 0 ? 2 : 3;
}

/** A text was typed (edit box, chat): its Z is no zoom. */
void Layout_ZoomCancel(void)
{
	s_zoomRequest = 0;
}

static void Layout_ChangeZoom(int zoom)
{
	int cx = Tile_GetPackedX(g_viewportPosition) + g_viewportTilesW / 2;
	int cy = Tile_GetPackedY(g_viewportPosition) + g_viewportTilesH / 2;

	s_zoomWanted = zoom;
	if (zoom == g_viewZoom) return;
	GUI_Mouse_Hide_Safe();
	Map_UpdateMinimapPosition(0xFFFF, true);	/* off the radar at the old size */
	g_viewZoom = zoom;
	g_viewportTilesW = (s_fullTilesW + zoom - 1) / zoom;
	g_viewportTilesH = (s_fullTilesH + zoom - 1) / zoom;
	/* the middle of the view stays */
	g_viewportPosition = Tile_PackXY((uint16)max(cx - g_viewportTilesW / 2, 0), (uint16)max(cy - g_viewportTilesH / 2, 0));
	Layout_ClampViewport();
	Layout_Properties();
	Layout_GameWidgets();
	g_viewport_forceRedraw = true;
	Map_UpdateMinimapPosition(g_minimapPosition, true);
	/* (it copies the radar to the screen only when it cleared something) */
	GUI_Screen_Copy(32 + g_sidebarDX / 8, g_radarY, 32 + g_sidebarDX / 8, g_radarY, 8, 64, SCREEN_1, SCREEN_0);
	GUI_Mouse_Show_Safe();
}

/** On the map screen (RTS_Tick): a zoom asked for by Z or the wheel. */
void Layout_ZoomTick(void)
{
	int request = s_zoomRequest, most = Layout_MaxZoom(), z = g_viewZoom;
	if (request == 0) return;
	s_zoomRequest = 0;
	if (most <= 1) return;
	if (request == 1) z = z >= most ? 1 : z + 1;
	else if (request == 2) z = min(z + 1, most);
	else z = max(z - 1, 1);
	Layout_ChangeZoom(z);
}

/** The point of the screen is over the viewport (zoomed or not). */
bool Layout_InView(int x, int y)
{
	return x >= 0 && x < s_fullTilesW * 16 && y >= 40 && y < 40 + s_fullTilesH * 16;
}

/** A point of the screen over the zoomed viewport -> the logical picture's pixel. */
void Layout_ViewPoint(int16 *x, int16 *y)
{
	if (g_viewZoom <= 1) return;
	if (*x < 0 || *x >= s_fullTilesW * 16 || *y < 40 || *y >= 40 + s_fullTilesH * 16) return;
	*x = (int16)(*x / g_viewZoom);
	*y = (int16)(40 + (*y - 40) / g_viewZoom);
}

/* Rows [y1, y2) of SCREEN_1's logical viewport, columns [x1, x2), stretched
 * into SCREEN_0. */
static void Layout_Stretch(int x1, int y1, int x2, int y2)
{
	const uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_1);
	uint8 *dst = GFX_Screen_Get_ByIndex(SCREEN_0);
	int z = g_viewZoom, fw = s_fullTilesW * 16, fy2 = 40 + s_fullTilesH * 16;
	int dx1 = x1 * z, dx2 = min(x2 * z, fw), dy1 = 40 + (y1 - 40) * z, dy2 = min(40 + (y2 - 40) * z, fy2);
	int y;

	if (dx1 >= dx2 || dy1 >= dy2) return;
	for (y = y1; y < y2; y++) {
		const uint8 *s = src + y * SCREEN_WIDTH + x1;
		int ty = 40 + (y - 40) * z, r, n = dx2 - dx1;
		uint8 *d = dst + ty * SCREEN_WIDTH + dx1;
		int x;
		if (ty >= fy2) break;
		if (z == 2) {
			for (x = 0; x + 1 < n; x += 2) { uint8 p = *s++; d[x] = p; d[x + 1] = p; }
			if (x < n) d[x] = *s;
		} else {
			for (x = 0; x < n; x++) d[x] = s[x / z];
		}
		for (r = 1; r < z && ty + r < fy2; r++) memcpy(d + r * SCREEN_WIDTH, d, n);
	}
	GFX_Screen_SetDirty(SCREEN_0, (uint16)dx1, (uint16)dy1, (uint16)dx2, (uint16)dy2);
}

/**
 * GUI_Screen_Copy from SCREEN_1 to SCREEN_0, the same place, while the view is
 * zoomed (x, w in pixels): what of it is the logical viewport is stretched over
 * the real one, what is outside the real viewport copied 1:1, the rest of the
 * real viewport in SCREEN_1 (beyond the logical picture) left alone.
 */
bool Layout_ZoomCopy(int x, int y, int w, int h)
{
	const uint8 *src;
	uint8 *dst;
	int fw = s_fullTilesW * 16, fy2 = 40 + s_fullTilesH * 16;
	int lw = g_viewportTilesW * 16, ly2 = 40 + g_viewportTilesH * 16;
	int x2 = x + w, y2 = y + h, r;

	if (g_viewZoom <= 1 || !s_big) return false;
	if (x2 <= 0 || y2 <= 40 || x >= fw || y >= fy2) return false;	/* not over the viewport: as always */

	GUI_Mouse_Hide_Safe();
	src = GFX_Screen_Get_ByIndex(SCREEN_1);
	dst = GFX_Screen_Get_ByIndex(SCREEN_0);
	/* the logical viewport */
	Layout_Stretch(max(x, 0), max(y, 40), min(x2, lw), min(y2, ly2));
	/* 1:1 outside the real viewport: above, below, right of it */
	for (r = y; r < y2; r++) {
		int a = x, b = x2;
		if (r >= 40 && r < fy2) a = max(a, fw);
		if (a >= b) continue;
		memcpy(dst + r * SCREEN_WIDTH + a, src + r * SCREEN_WIDTH + a, b - a);
	}
	GFX_Screen_SetDirty(SCREEN_0, (uint16)max(x, 0), (uint16)max(y, 0), (uint16)min(x2, SCREEN_WIDTH), (uint16)min(y2, SCREEN_HEIGHT));
	GUI_Mouse_Show_Safe();
	return true;
}

/**
 * The view moved by dx, dy logical pixels (GUI_DrawScreen): SCREEN_1's logical
 * viewport and SCREEN_0's real one each by their own amount, on the card by
 * the blitter; the edge tiles cut by the real viewport stretched again (their
 * cut part comes into view).
 */
void Layout_ZoomShift(int16 dx, int16 dy)
{
	int z = g_viewZoom, fw = s_fullTilesW * 16, fh = s_fullTilesH * 16;
	int lw = g_viewportTilesW * 16, lh = g_viewportTilesH * 16;

	GUI_Mouse_Hide_Safe();
	GFX_ShiftScreenRect(SCREEN_1, dx, dy, 0, 40, (int16)lw, (int16)lh);
	GFX_ShiftScreenRect(SCREEN_0, (int16)(dx * z), (int16)(dy * z), 0, 40, (int16)fw, (int16)fh);
	if (!Video_ScrollView((int16)(dx * z), (int16)(dy * z), 0, 40, (int16)fw, (int16)fh)) {
		GFX_Screen_SetDirty(SCREEN_0, 0, 40, (uint16)fw, (uint16)(40 + fh));
	} else {
		/* the card moves the pointer it still shows too: send that place again */
		int x1 = max(g_mouseX - 16 + dx * z, 0), y1 = max(g_mouseY - 16 + dy * z, 40);
		int x2 = min(g_mouseX + 32 + dx * z, fw), y2 = min(g_mouseY + 32 + dy * z, 40 + fh);
		if (x1 < x2 && y1 < y2) GFX_Screen_SetDirty(SCREEN_0, (uint16)x1, (uint16)y1, (uint16)x2, (uint16)y2);
	}
	/* the last two columns and rows of tiles: partly outside before */
	Layout_Stretch(max(lw - 32, 0), 40, lw, 40 + lh);
	Layout_Stretch(0, max(40 + lh - 32, 40), lw, 40 + lh);
	GUI_Mouse_Show_Safe();
}
