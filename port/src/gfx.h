/** @file src/gfx.h Graphics definitions. */

#ifndef GFX_H
#define GFX_H

extern uint8 g_paletteActive[256 * 3];
extern uint8 *g_palette1;
extern uint8 *g_palette2;
extern uint8 *g_paletteMapping1;
extern uint8 *g_paletteMapping2;

#if defined(__amigaos__)
/* The size of the screen the game draws, in pixels: 320x200 in the menus and
 * cutscenes, larger on the map (GFX_SetScreenSize()). Buffers are made for
 * the maximum. */
#define SCREEN_WIDTH_MAX  960
#define SCREEN_HEIGHT_MAX 600
extern uint16 g_screenWidth;
extern uint16 g_screenHeight;
#define SCREEN_WIDTH  g_screenWidth
#define SCREEN_HEIGHT g_screenHeight
extern void GFX_SetScreenSize(uint16 width, uint16 height);
extern void GFX_ShiftViewport(int16 dx, int16 dy, int16 x, int16 y, int16 w, int16 h);
extern void GFX_ShiftScreenRect(int screenID, int16 dx, int16 dy, int16 x, int16 y, int16 w, int16 h);
extern void GFX_MoveRow(uint8 *dst, const uint8 *src, int n);
/* the map screen's layout (gui/layout_amiga.c) */
extern uint16 g_viewportTilesW;
extern uint16 g_viewportTilesH;
extern int16 g_sidebarDX;
#define VIEWPORT_TILES_W g_viewportTilesW	/* viewport width in tiles (15) */
#define VIEWPORT_TILES_H g_viewportTilesH	/* viewport height in tiles (10) */
#define VIEWPORT_TILES_H_MAX 40
#define SIDEBAR_DX g_sidebarDX			/* shift of the sidebar and radar (0) */
extern int16 g_radarY;
extern int16 g_panelDY;
#define RADAR_Y g_radarY			/* top of the radar (136) */
#define PANEL_DY g_panelDY			/* shift down of the action panel (0) */
#else
enum {
	SCREEN_WIDTH  = 320, /*!< Width of the screen in pixels. */
	SCREEN_HEIGHT = 200  /*!< Height of the screen in pixels. */
};
#define SCREEN_WIDTH_MAX  SCREEN_WIDTH
#define SCREEN_HEIGHT_MAX SCREEN_HEIGHT
#define VIEWPORT_TILES_W 15
#define VIEWPORT_TILES_H 10
#define VIEWPORT_TILES_H_MAX 10
#define SIDEBAR_DX 0
#define RADAR_Y 136
#define PANEL_DY 0
#endif

typedef enum Screen {
	SCREEN_0 = 0,
	SCREEN_1 = 1,
	SCREEN_2 = 2,
	SCREEN_3 = 3,
	SCREEN_ACTIVE = -1
} Screen;

extern void GFX_Init(void);
extern void GFX_Uninit(void);
extern Screen GFX_Screen_SetActive(Screen screenID);
extern bool GFX_Screen_IsActive(Screen screenID);
extern uint32 GFX_Screen_GetSize_ByIndex(Screen screenID);
extern void *GFX_Screen_Get_ByIndex(Screen screenID);

#define GFX_Screen_GetActive() GFX_Screen_Get_ByIndex(SCREEN_ACTIVE)

extern void GFX_DrawTile(uint16 spriteID, uint16 x, uint16 y, uint8 houseID);
extern void GFX_Init_TilesInfo(uint16 widthSize, uint16 heightSize);
extern void GFX_PutPixel(uint16 x, uint16 y, uint8 colour);
extern void GFX_Screen_Copy2(int16 xSrc, int16 ySrc, int16 xDst, int16 yDst, int16 width, int16 height, Screen screenSrc, Screen screenDst, bool skipNull);
extern void GFX_Screen_Copy(int16 xSrc, int16 ySrc, int16 xDst, int16 yDst, int16 width, int16 height, Screen screenSrc, Screen screenDst);
extern void GFX_ClearScreen(Screen screenID);
extern void GFX_ClearBlock(Screen index);
extern void GFX_SetPalette(uint8 *palette);
extern uint8 GFX_GetPixel(uint16 x, uint16 y);
extern uint32 GFX_GetSize(int16 width, int16 height);
extern void GFX_CopyFromBuffer(int16 left, int16 top, uint16 width, uint16 height, uint8 *buffer);
extern void GFX_CopyToBuffer(int16 left, int16 top, uint16 width, uint16 height, uint8 *buffer);

#define GFX_STORE_DIRTY_AREA
#if defined(TOS) || defined(__amigaos__)
#define GFX_STORE_DIRTY_AREA_BLOCKS
#endif

struct dirty_area { uint16 left; uint16 top; uint16 right; uint16 bottom; };
#ifdef GFX_STORE_DIRTY_AREA
extern void GFX_Screen_SetDirty(Screen screenID, uint16 left, uint16 top, uint16 right, uint16 bottom);
extern void GFX_Screen_SetClean(Screen screenID);
extern bool GFX_Screen_IsDirty(Screen screenID);
extern struct dirty_area * GFX_Screen_GetDirtyArea(Screen screenID);
#ifdef GFX_STORE_DIRTY_AREA_BLOCKS
#if defined(__amigaos__)
/* per row: GFX_DIRTY_WORDS words of 32 blocks of 16 pixels */
#define GFX_DIRTY_WORDS ((SCREEN_WIDTH_MAX + 511) / 512)
extern uint32 g_dirty_blocks[SCREEN_HEIGHT_MAX][GFX_DIRTY_WORDS];
#else
extern uint32 g_dirty_blocks[200];
#endif
#endif
#else
#define GFX_Screen_SetDirty(screenID, left, top, right, bottom)
#define GFX_Screen_SetClean(screenID)
#define GFX_Screen_IsDirty(screenID) true
#define GFX_Screen_GetDirtyArea(screenID) NULL
#endif /* GFX_STORE_DIRTY_AREA */

#endif /* GFX_H */
