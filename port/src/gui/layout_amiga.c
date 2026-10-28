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
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "../os/common.h"
#include "../os/math.h"
#include "../gfx.h"
#include "../map.h"
#include "../scenario.h"
#include "../tile.h"
#include "../input/mouse.h"
#include "gui.h"
#include "widget.h"
#include "layout_amiga.h"

uint16 g_viewportTilesW = 15;
uint16 g_viewportTilesH = 10;
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
static bool s_saved = false;
static WidgetProperties s_props[22];		/* g_widgetProperties as in the original */

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
	uint16 vw = g_viewportTilesW * 16, vh = g_viewportTilesH * 16;
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
	if (!s_big) return;

	g_widgetProperties[0].width = SCREEN_WIDTH / 8;
	g_widgetProperties[0].height = SCREEN_HEIGHT;
	/* message box: centred over the viewport */
	g_widgetProperties[1].xBase += (g_viewportTilesW * 16 - 240) / 16;
	g_widgetProperties[1].yBase += (g_viewportTilesH * 16 - 160) / 2;
	g_widgetProperties[2].width = g_viewportTilesW * 2;
	g_widgetProperties[2].height = g_viewportTilesH * 16;
	for (i = 3; i <= 6; i++) g_widgetProperties[i].xBase += dx8;	/* radar, credits, action panel */
	g_widgetProperties[3].yBase = g_radarY;
	g_widgetProperties[4].yBase += g_panelDY;	/* credits staged in the panel's back buffer */
	g_widgetProperties[6].yBase += g_panelDY;
	g_widgetProperties[6].height = SCREEN_HEIGHT - 8 - g_widgetProperties[6].yBase;
	g_widgetProperties[7].width = SCREEN_WIDTH / 8 - 2;			/* text line */
	for (i = 14; i <= 18; i++) {						/* windows: centred */
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
	if (big == s_big) return;

	GUI_Mouse_Hide_Safe();
	s_big = big;
	if (big) {
		GFX_SetScreenSize(LAYOUT_BIG_WIDTH, LAYOUT_BIG_HEIGHT);
		Mouse_SetRegion(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
		g_viewportTilesW = (SCREEN_WIDTH - 80) / 16;
		g_viewportTilesH = (SCREEN_HEIGHT - 40) / 16;
		g_sidebarDX = SCREEN_WIDTH - 320;
		g_radarY = BIG_RADAR_Y;
		g_panelDY = BIG_PANEL_Y - 42;
	} else {
		GFX_SetScreenSize(320, 200);
		Mouse_SetRegion(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
		g_viewportTilesW = 15;
		g_viewportTilesH = 10;
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
