/** @file src/os/bugreport_amiga.c Bug report on F10 (AmigaOS).
 *
 * F10 (Video_Key, the video driver's tick) asks for a report; the game loop
 * writes it (AmigaBug_Tick), with the game in a consistent state, into the
 * save directory, NNN the first free number:
 *   bugNNN.txt         version, game and screen state, every unit
 *   bugNNN.dat         a saved game of that moment (loadable: copy it to
 *                      _saveNNN.dat)
 *   bugNNN-screen.bmp  what the screen shows (read back from the card)
 *   bugNNN-game.bmp    the game's own picture (SCREEN_0)
 *   bugNNN-copy.bmp    the video driver's copy of the screen (vram_scroll)
 * Differences between the three pictures show where a drawing error comes
 * from (the game, the upload, the blitter scroll).
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "../file.h"
#include "../gfx.h"
#include "../house.h"
#include "../map.h"
#include "../opendune.h"
#include "../rev.h"
#include "../save.h"
#include "../tile.h"
#include "../timer.h"
#include "../unit.h"
#include "../gui/gui.h"
#include "../pool/pool.h"
#include "../pool/unit.h"
#include "error.h"
#include "../input/mouse.h"

extern bool Video_Amiga_ReadScreen(uint8 *dst, int *w, int *h);
extern const uint8 *Video_Amiga_Shadow(int *w, int *h);
extern const uint8 *Video_Amiga_Palette(void);
extern void Video_Amiga_Report(void);
extern void Smooth_Report(void);
extern int RTS_GroupCount(void);

static volatile bool s_requested;

#define TEXT_MAX 32768
static char *s_text;
static int s_textLen;

/** Appends to the report's text (also for the other parts' reports). */
void AmigaBug_Printf(const char *format, ...)
{
	va_list ap;
	int n;
	if (s_text == NULL || s_textLen >= TEXT_MAX - 1) return;
	va_start(ap, format);
	n = vsnprintf(s_text + s_textLen, TEXT_MAX - s_textLen, format, ap);
	va_end(ap);
	if (n > 0) s_textLen += n;
	if (s_textLen > TEXT_MAX - 1) s_textLen = TEXT_MAX - 1;
}

/** F10 was pressed (video driver's tick). */
void AmigaBug_Request(void)
{
	s_requested = true;
}

/* An 8-bit BMP with the game's palette, w x h from pixels (pitch bytes a row). */
static bool AmigaBug_WriteBMP(const char *name, const uint8 *pixels, int pitch, int w, int h)
{
	const uint8 *pal = Video_Amiga_Palette();
	uint8 header[54];
	uint8 quad[4];
	uint32 rowSize = (w + 3) & ~3, image = rowSize * h, offset = 54 + 1024, size = offset + image;
	uint8 *row;
	uint8 fd;
	int i, y;

	fd = File_Open_Personal(name, FILE_MODE_WRITE);
	if (fd == 0xFF) return false;
	memset(header, 0, sizeof(header));
	header[0] = 'B'; header[1] = 'M';
	header[2] = size; header[3] = size >> 8; header[4] = size >> 16; header[5] = size >> 24;
	header[10] = offset; header[11] = offset >> 8;
	header[14] = 40;
	header[18] = w; header[19] = w >> 8;
	header[22] = h; header[23] = h >> 8;
	header[26] = 1;
	header[28] = 8;
	header[34] = image; header[35] = image >> 8; header[36] = image >> 16; header[37] = image >> 24;
	header[47] = 1;		/* 256 colours */
	File_Write(fd, header, sizeof(header));
	for (i = 0; i < 256; i++) {
		/* the VGA DAC's 6 bits, as BGR0 */
		quad[0] = (uint8)((pal[i * 3 + 2] << 2) | (pal[i * 3 + 2] >> 4));
		quad[1] = (uint8)((pal[i * 3 + 1] << 2) | (pal[i * 3 + 1] >> 4));
		quad[2] = (uint8)((pal[i * 3 + 0] << 2) | (pal[i * 3 + 0] >> 4));
		quad[3] = 0;
		File_Write(fd, quad, 4);
	}
	row = calloc(1, rowSize);
	if (row != NULL) {
		for (y = h - 1; y >= 0; y--) {	/* bottom-up */
			memcpy(row, pixels + y * pitch, w);
			File_Write(fd, row, rowSize);
		}
		free(row);
	}
	File_Close(fd);
	return true;
}

static void AmigaBug_WriteText(const char *name, int number)
{
	PoolFindStruct find;
	Unit *u;
	uint8 fd;

	s_text = malloc(TEXT_MAX);
	if (s_text == NULL) return;
	s_textLen = 0;

	AmigaBug_Printf("OpenDUNE bug report %d\nversion %s, built %s\n\n", number, g_opendune_revision, g_opendune_build_date);
	AmigaBug_Printf("game: mode %d, scenario %u, campaign %u, house %d, credits %u\n",
	        (int)g_gameMode, (unsigned)g_scenarioID, (unsigned)g_campaignID, (int)g_playerHouseID, (unsigned)g_playerCredits);
	AmigaBug_Printf("timers: game %lu, GUI %lu, now %lu ms\n",
	        (unsigned long)g_timerGame, (unsigned long)g_timerGUI, (unsigned long)Timer_GetTime());
	AmigaBug_Printf("viewport %d,%d (minimap %d,%d), %dx%d tiles; selection type %u, unit %d, group %d; mouse %u,%u\n",
	        Tile_GetPackedX(g_viewportPosition), Tile_GetPackedY(g_viewportPosition),
	        Tile_GetPackedX(g_minimapPosition), Tile_GetPackedY(g_minimapPosition),
	        (int)VIEWPORT_TILES_W, (int)VIEWPORT_TILES_H, (unsigned)g_selectionType,
	        g_unitSelected != NULL ? (int)g_unitSelected->o.index : -1, RTS_GroupCount(),
	        (unsigned)g_mouseX, (unsigned)g_mouseY);
	AmigaBug_Printf("game screen %ux%u\n", (unsigned)SCREEN_WIDTH, (unsigned)SCREEN_HEIGHT);
	Video_Amiga_Report();
	Smooth_Report();

	AmigaBug_Printf("\nunits: index type house action pos(px) flags(used alloc notOnMap dirty)\n");
	find.houseID = HOUSE_INVALID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	while ((u = Unit_Find(&find)) != NULL) {
		AmigaBug_Printf("%3u %2u %u %2u %4u,%4u %d%d%d%d\n", (unsigned)u->o.index, (unsigned)u->o.type,
		        (unsigned)u->o.houseID, (unsigned)u->actionID,
		        (unsigned)(u->o.position.x >> 4), (unsigned)(u->o.position.y >> 4),
		        (int)u->o.flags.s.used, (int)u->o.flags.s.allocated, (int)u->o.flags.s.isNotOnMap,
		        (int)u->o.flags.s.isDirty);
	}
	fd = File_Open_Personal(name, FILE_MODE_WRITE);
	if (fd != 0xFF) {
		File_Write(fd, s_text, s_textLen);
		File_Close(fd);
	}
	free(s_text);
	s_text = NULL;
}

/** In the game loop: writes a requested report. */
void AmigaBug_Tick(void)
{
	char name[32], desc[32];
	uint8 *screen;
	const uint8 *shadow;
	int number, w, h;

	if (!s_requested) return;
	s_requested = false;

	for (number = 1; number < 1000; number++) {
		sprintf(name, "bug%03d.txt", number);
		if (!File_Exists_Personal(name)) break;
	}
	if (number >= 1000) return;

	AmigaBug_WriteText(name, number);

	screen = malloc(SCREEN_WIDTH_MAX * SCREEN_HEIGHT_MAX * 4);	/* up to 1024x768 + */
	if (screen != NULL && Video_Amiga_ReadScreen(screen, &w, &h)) {
		sprintf(name, "bug%03d-screen.bmp", number);
		AmigaBug_WriteBMP(name, screen, w, w, h);
	}
	free(screen);
	sprintf(name, "bug%03d-game.bmp", number);
	AmigaBug_WriteBMP(name, GFX_Screen_Get_ByIndex(SCREEN_0), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT);
	shadow = Video_Amiga_Shadow(&w, &h);
	if (shadow != NULL) {
		sprintf(name, "bug%03d-copy.bmp", number);
		AmigaBug_WriteBMP(name, shadow, w, w, h);
	}
	if (g_gameMode == GM_NORMAL) {
		sprintf(name, "bug%03d.dat", number);
		sprintf(desc, "Bug report %d", number);
		SaveGame_SaveFile(name, desc);
	}
	GUI_DisplayText("Bug report %d saved.", 2, number);
}
