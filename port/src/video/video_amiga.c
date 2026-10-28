/** @file src/video/video_amiga.c AmigaOS 3 + Picasso96 (Warp RTG) video driver.
 *
 * The game draws 8-bit into a buffer in fast RAM: 320x200 in the menus and
 * cutscenes, a larger size (SCREEN_WIDTH x SCREEN_HEIGHT, e.g. 640x480) on the
 * map. It is shown on an own 8-bit CLUT screen (640x480 by default), 1:1 when
 * the size fits, else scaled by whole pixels (320x200 -> 2x2) or by 2.5x3
 * (320x200 on 800x600), centred. A change of the game's size is noticed in
 * Video_Tick() (new scale, screen cleared, everything sent again).
 *
 * Only the 16-pixel blocks the game marked dirty are sent. At 1:1 (the map
 * screen) a copy of the screen is kept in fast RAM (s_shadow, the screen's
 * layout) and only the blocks that differ from it are sent; when the map
 * scrolls (Video_ScrollView() from GUI_DrawScreen) the card's blitter moves
 * the viewport in VRAM and the copy is moved the same way, so a scroll step
 * sends the new strip and what moved instead of the whole viewport
 * (opendune.ini vram_scroll=1, the default; 0 = without the copy). Each source row is
 * widened once into a line buffer in fast RAM and written to the locked
 * bitmap with long moves, once per screen row it covers (Warp RTG notes:
 * write VRAM only, never read it; the lock is held only inside Video_Tick and
 * no system call that draws or allocates is made while it is held).
 *
 * Mouse: the (hidden) Intuition pointer's position mapped to 320x200, so it
 * moves at the Workbench's speed; the game draws its own pointer and moves
 * Intuition's through input.device when it sets the position. (Relative
 * moves with IDCMP_DELTAMOVE lost movement: Intuition drops MOUSEMOVE
 * messages while its queue is full.) Keyboard: Amiga raw keys translated to
 * AT scancodes.
 * opendune.ini: screenmode=640x480 (default), 800x600, ...
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <graphics/gfx.h>
#include <libraries/Picasso96.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/Picasso96.h>
#include <proto/dos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* gfx.h's enum Screen clashes with Intuition's struct Screen; exec's Debug()
 * with error.h's */
#undef Debug
#define Screen DuneScreen
#include "types.h"
#include "video.h"
#include "../gfx.h"
#include "../inifile.h"
#include "../timer.h"
#include "../input/input.h"
#include "../input/mouse.h"
#include "../os/error.h"
#include "../gui/rts_amiga.h"
#include "../os/amiga_prof.h"
#undef Screen

struct Library *P96Base;

static struct Screen *s_screen;
static struct Window *s_window;
static UWORD *s_blankPointer;		/* chip RAM */

static uint8 *s_framebuffer;		/* SCREEN_WIDTH_MAX x (SCREEN_HEIGHT_MAX + 4), fast RAM */
static uint16 s_screenOffset;
static bool s_needRepaint = true;

/* geometry: source x -> screen x0 + (x * s_sx2) / 2, source y -> y0 + y * s_sy */
static int s_width, s_height;		/* screen */
static int s_lw, s_lh;			/* the game's size the geometry was made for */
static int s_sx2, s_sy;			/* horizontal scale in halves, vertical scale */
static int s_x0, s_y0;			/* offset of the picture (x0 multiple of 4) */
static uint16 s_colX[SCREEN_WIDTH_MAX + 1];	/* screen x of every source column, relative to x0 */
static uint32 *s_line;			/* one widened row (long aligned) */

/* mouse */
static int s_mouseX = 160, s_mouseY = 100;
static struct MsgPort *s_inputPort;	/* input.device, to move the pointer */
static struct IOStdReq *s_inputReq;
static int s_mouseMinX = 0, s_mouseMaxX = 319;
static int s_mouseMinY = 0, s_mouseMaxY = 199;
static bool s_mouseLeft, s_mouseRight;

/* FPS counter (F8 or show_fps=1): frames sent per second, bottom left corner */
static bool s_showFPS;
static uint32 s_fpsTime, s_fpsFrames, s_fpsShown;

static bool s_atexitDone;

/* test commands (amiga_cmd.c), debug_commands=1 in opendune.ini */
extern void AmigaCmd_Tick(void);
static bool s_commands;
static uint8 s_palette[256 * 3];		/* the game's palette, 6-bit */

/* the copy of the screen at 1:1 and the pending scroll of the viewport */
static uint8 *s_shadow;			/* s_width x s_height, as the bitmap */
static bool s_shadowValid;
static bool s_vramScroll;
static int16 s_scrollDX, s_scrollDY;	/* pixels the viewport's picture moved */
static int16 s_scrollRect[4];		/* x, y, w, h in game pixels */
static bool s_scrollPending;

/* the selection rectangle (rts_amiga.c), drawn over the picture in VRAM */
static bool s_rectShown;
static int16 s_rect[4];
#define RECT_COLOUR 15

/* Amiga raw key -> AT set 1 scancode; 0x80 = with the E0 prefix, 0 = none */
static const uint8 s_rawToAT[0x68] = {
	/*00*/ 0x29, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x2B, 0x00, 0x52,
	/*10*/ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x00, 0x4F, 0x50, 0x51,
	/*20*/ 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2B, 0x00, 0x4B, 0x4C, 0x4D,
	/*30*/ 0x56, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x00, 0x53, 0x47, 0x48, 0x49,
	/*40*/ 0x39, 0x0E, 0x0F, 0x9C, 0x1C, 0x01, 0xD3, 0x00, 0x00, 0x00, 0x4A, 0x00, 0xC8, 0xD0, 0xCD, 0xCB,
	/*50*/ 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x00, 0x00, 0xB5, 0x37, 0x4E, 0x00,
	/*60*/ 0x2A, 0x36, 0x3A, 0x1D, 0x38, 0xB8, 0xDB, 0xDC
};

#define RAW_F8 0x57
#define RAW_F10 0x59
#define RAW_P   0x19

extern void AmigaBug_Request(void);
extern void AmigaBug_Printf(const char *format, ...);

static bool Video_OpenP96(void)
{
	if (P96Base == NULL) P96Base = OpenLibrary((CONST_STRPTR)"Picasso96API.library", 2);
	return P96Base != NULL;
}

/* The 8-bit CLUT mode of exactly w x h, else the smallest one that holds it
 * (p96BestModeIDTags() answered 320x200 for 640x480 on a Warp, Settlers). */
static ULONG Video_FindClutMode(int w, int h, int *mw, int *mh)
{
	struct List *ml;
	struct P96Mode *m;
	ULONG best = INVALID_ID;
	ULONG bestArea = 0xFFFFFFFFUL;

	ml = p96AllocModeListTags(P96MA_FormatsAllowed, RGBFF_CLUT, TAG_DONE);
	if (ml == NULL) return INVALID_ID;
	for (m = (struct P96Mode *)ml->lh_Head; m->Node.ln_Succ != NULL; m = (struct P96Mode *)m->Node.ln_Succ) {
		ULONG area;
		if (m->Depth != 8 || m->Width < w || m->Height < h) continue;
		area = (ULONG)m->Width * m->Height;
		if (area < bestArea) {
			bestArea = area;
			best = m->DisplayID;
			*mw = m->Width;
			*mh = m->Height;
		}
	}
	p96FreeModeList(ml);
	return best;
}

/* Scale and offset of the game's current size on the screen. */
static void Video_SetGeometry(void)
{
	int x, w = s_width, h = s_height;

	s_lw = SCREEN_WIDTH;
	s_lh = SCREEN_HEIGHT;
	s_sx2 = (2 * w) / SCREEN_WIDTH;
	s_sy = h / SCREEN_HEIGHT;
	if (s_sx2 < 2) s_sx2 = 2;
	if (s_sy < 1) s_sy = 1;
	/* not wider than the vertical scale allows (keeps 2.5x3, 2x2, 3x3) */
	if (s_sx2 > 2 * s_sy) s_sx2 = 2 * s_sy;
	if (s_sy * 2 > s_sx2 + 1) s_sy = (s_sx2 + 1) / 2;
	s_x0 = ((w - SCREEN_WIDTH * s_sx2 / 2) / 2) & ~3;
	s_y0 = (h - SCREEN_HEIGHT * s_sy) / 2;
	for (x = 0; x <= SCREEN_WIDTH; x++) s_colX[x] = (uint16)((x * s_sx2) / 2);
}

static void Video_Cleanup(void)
{
	Video_Uninit();
}

bool Video_Init(int screen_magnification, VideoScaleFilter filter)
{
	char mode[32];
	int reqW = 640, reqH = 480, mw = 0, mh = 0;
	ULONG id;

	VARIABLE_NOT_USED(screen_magnification);
	VARIABLE_NOT_USED(filter);

	if (IniFile_GetString("screenmode", NULL, mode, sizeof(mode)) != NULL) {
		char *x = strchr(mode, 'x');
		if (x != NULL) {
			reqW = atoi(mode);
			reqH = atoi(x + 1);
		}
		if (reqW < SCREEN_WIDTH_MAX || reqH < SCREEN_HEIGHT_MAX) {
			Warning("screenmode %s too small, using %dx%d\n", mode, SCREEN_WIDTH_MAX, SCREEN_HEIGHT_MAX);
			reqW = SCREEN_WIDTH_MAX;
			reqH = SCREEN_HEIGHT_MAX;
		}
	}

	s_framebuffer = calloc(1, SCREEN_WIDTH_MAX * (SCREEN_HEIGHT_MAX + 4));
	if (s_framebuffer == NULL) return false;

	if (!Video_OpenP96()) {
		Error("Picasso96API.library v2 is needed\n");
		return false;
	}
	id = Video_FindClutMode(reqW, reqH, &mw, &mh);
	if (id == INVALID_ID) {
		Error("No 8-bit RTG screen mode of %dx%d or larger\n", reqW, reqH);
		return false;
	}
	s_width = reqW;
	s_height = reqH;
	Video_SetGeometry();
	s_line = AllocVec(s_width + 16, MEMF_ANY | MEMF_CLEAR);
	s_blankPointer = AllocVec(16, MEMF_CHIP | MEMF_CLEAR);
	if (s_line == NULL || s_blankPointer == NULL) return false;

	s_screen = p96OpenScreenTags(P96SA_DisplayID, id,
	                             P96SA_Width, s_width,
	                             P96SA_Height, s_height,
	                             P96SA_Depth, 8,
	                             P96SA_RGBFormat, RGBFB_CLUT,
	                             P96SA_Quiet, TRUE,
	                             P96SA_ShowTitle, FALSE,
	                             P96SA_Title, (ULONG)"OpenDUNE",
	                             P96SA_Exclusive, TRUE,
	                             TAG_DONE);
	if (s_screen == NULL) {
		Error("Cannot open a %dx%d 8-bit screen\n", s_width, s_height);
		return false;
	}
	s_window = OpenWindowTags(NULL,
	                          WA_CustomScreen, (ULONG)s_screen,
	                          WA_Left, 0, WA_Top, 0,
	                          WA_Width, s_width, WA_Height, s_height,
	                          WA_Backdrop, TRUE,
	                          WA_Borderless, TRUE,
	                          WA_Activate, TRUE,
	                          WA_RMBTrap, TRUE,
	                          WA_ReportMouse, TRUE,
	                          WA_SimpleRefresh, TRUE,
	                          WA_NoCareRefresh, TRUE,
	                          WA_Title, (ULONG)"OpenDUNE",
	                          WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY,
	                          TAG_DONE);
	if (s_window == NULL) {
		Error("Cannot open the window\n");
		return false;
	}
	SetPointer(s_window, s_blankPointer, 1, 1, 0, 0);
	s_inputPort = CreateMsgPort();
	if (s_inputPort != NULL) s_inputReq = (struct IOStdReq *)CreateIORequest(s_inputPort, sizeof(struct IOStdReq));
	if (s_inputReq != NULL && OpenDevice((CONST_STRPTR)"input.device", 0, (struct IORequest *)s_inputReq, 0) != 0) {
		DeleteIORequest((struct IORequest *)s_inputReq);
		s_inputReq = NULL;
	}
	SetRast(s_window->RPort, 0);
	g_consoleActive = false;
	s_needRepaint = true;
	s_commands = IniFile_GetInteger("debug_commands", 0) != 0;
	s_showFPS = IniFile_GetInteger("show_fps", 0) != 0;
	s_vramScroll = IniFile_GetInteger("vram_scroll", 1) != 0;
	if (s_vramScroll) s_shadow = AllocVec((ULONG)s_width * s_height, MEMF_ANY);
	s_shadowValid = false;

	if (!s_atexitDone) {
		atexit(Video_Cleanup);
		s_atexitDone = true;
	}
	Debug("Video_Init: mode %dx%d (board %dx%d), scale %d/2 x %d, offset %d,%d\n",
	      s_width, s_height, mw, mh, s_sx2, s_sy, s_x0, s_y0);
	return true;
}

void Video_Uninit(void)
{
	if (s_inputReq != NULL) {
		CloseDevice((struct IORequest *)s_inputReq);
		DeleteIORequest((struct IORequest *)s_inputReq);
		s_inputReq = NULL;
	}
	if (s_inputPort != NULL) { DeleteMsgPort(s_inputPort); s_inputPort = NULL; }
	if (s_window != NULL) {
		ClearPointer(s_window);
		CloseWindow(s_window);
		s_window = NULL;
	}
	if (s_screen != NULL) {
		p96CloseScreen(s_screen);
		s_screen = NULL;
	}
	if (s_blankPointer != NULL) { FreeVec(s_blankPointer); s_blankPointer = NULL; }
	if (s_line != NULL) { FreeVec(s_line); s_line = NULL; }
	if (s_shadow != NULL) { FreeVec(s_shadow); s_shadow = NULL; }
	if (P96Base != NULL) { CloseLibrary(P96Base); P96Base = NULL; }
	free(s_framebuffer);
	s_framebuffer = NULL;
	g_consoleActive = true;
}

static void Video_SendMouse(void)
{
	Mouse_EventHandler((uint16)s_mouseX, (uint16)s_mouseY, s_mouseLeft, s_mouseRight);
}

/* Screen position of the pointer -> game pixels. */
static void Video_MouseAt(int sx, int sy)
{
	s_mouseX = ((sx - s_x0) * 2) / s_sx2;
	s_mouseY = (sy - s_y0) / s_sy;
	if (s_mouseX < s_mouseMinX) s_mouseX = s_mouseMinX;
	if (s_mouseX > s_mouseMaxX) s_mouseX = s_mouseMaxX;
	if (s_mouseY < s_mouseMinY) s_mouseY = s_mouseMinY;
	if (s_mouseY > s_mouseMaxY) s_mouseY = s_mouseMaxY;
}

void Video_Amiga_Key(uint16 code)
{
	bool up = (code & IECODE_UP_PREFIX) != 0;
	uint8 sc;

	code &= ~IECODE_UP_PREFIX;
	if (code >= sizeof(s_rawToAT)) return;
	if (code == RAW_P && s_lw > 320) {
		/* the map screen: pause (standard keys of the ports: P) */
		if (!up) RTS_PauseRequest();
		return;
	}
	if (code == RAW_F10) {
		if (!up) AmigaBug_Request();	/* bug report, written by the game loop */
		return;
	}
	if (code == RAW_F8) {
		if (!up) {
			s_showFPS = !s_showFPS;
			s_needRepaint = true;
		}
		return;
	}
	sc = s_rawToAT[code];
	if (sc == 0) return;
	if (sc & 0x80) Input_EventHandler(0xE0);
	Input_EventHandler((uint8)((sc & 0x7F) | (up ? 0x80 : 0)));
}

/* for amiga_cmd.c */
void Video_Amiga_Mouse(int x, int y, bool left, bool right)
{
	/* the hidden Intuition pointer too, or its next move puts the mouse back */
	if (x != s_mouseX || y != s_mouseY) Video_Mouse_SetPosition((uint16)x, (uint16)y);
	s_mouseX = x;
	s_mouseY = y;
	s_mouseLeft = left;
	s_mouseRight = right;
	Video_SendMouse();
}

const uint8 *Video_Amiga_Palette(void)
{
	return s_palette;
}

static void Video_HandleEvents(void)
{
	struct IntuiMessage *msg;
	bool moved = false;

	while ((msg = (struct IntuiMessage *)GetMsg(s_window->UserPort)) != NULL) {
		ULONG cls = msg->Class;
		UWORD code = msg->Code;
		WORD mx = msg->MouseX, my = msg->MouseY;
		ReplyMsg((struct Message *)msg);

		switch (cls) {
			case IDCMP_MOUSEMOVE:
				Video_MouseAt(mx, my);
				moved = true;
				break;
			case IDCMP_MOUSEBUTTONS:
				Video_MouseAt(mx, my);
				moved = false;
				switch (code) {
					case SELECTDOWN: s_mouseLeft = true; break;
					case SELECTUP:   s_mouseLeft = false; break;
					case MENUDOWN:   s_mouseRight = true; break;
					case MENUUP:     s_mouseRight = false; break;
				}
				Video_SendMouse();
				break;
			case IDCMP_RAWKEY:
				Video_Amiga_Key(code);
				break;
		}
	}
	if (moved) Video_SendMouse();
	if (s_commands) AmigaCmd_Tick();
}

/* Any dirty 16-pixel block in the rectangle (for the stats). */
static bool Video_AreaDirty(int x0, int y0, int x1, int y1)
{
	int y, b;
	if (x1 >= SCREEN_WIDTH) x1 = SCREEN_WIDTH - 1;
	if (y1 >= SCREEN_HEIGHT) y1 = SCREEN_HEIGHT - 1;
	for (y = y0; y <= y1; y++) {
		for (b = x0 >> 4; b <= (x1 >> 4); b++) {
			if (g_dirty_blocks[y][b >> 5] & (1UL << (b & 31))) return true;
		}
	}
	return false;
}

/* Long copy of n bytes (multiple of 4) to VRAM. */
static void Video_CopyLongs(uint32 *dst, const uint32 *src, int n)
{
	n >>= 2;
	while (n >= 4) {
		dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
		dst += 4; src += 4; n -= 4;
	}
	while (n-- > 0) *dst++ = *src++;
}

/* Widens source pixels [x1, x2) of one row into s_line; returns the bytes
 * written (x1 and x2 multiples of 16, so the start is long aligned). */
static int Video_WidenRow(const uint8 *row, int x1, int x2)
{
	uint8 *d = (uint8 *)s_line;
	int x;

	if (s_sx2 == 2) {
		memcpy(d, row + x1, x2 - x1);
	} else if (s_sx2 == 4) {
		uint16 *dw = (uint16 *)d;
		for (x = x1; x < x2; x++) {
			uint16 p = row[x];
			*dw++ = (uint16)(p | (p << 8));
		}
	} else if (s_sx2 == 5) {
		/* 2.5x: two source pixels -> 3 + 2 screen pixels */
		for (x = x1; x < x2; x += 2) {
			uint8 a = row[x], b = row[x + 1];
			d[0] = a; d[1] = a; d[2] = a; d[3] = b; d[4] = b;
			d += 5;
		}
	} else {
		for (x = x1; x < x2; x++) {
			uint8 p = row[x];
			int n = s_colX[x + 1] - s_colX[x];
			while (n-- > 0) *d++ = p;
		}
	}
	return s_colX[x2] - s_colX[x1];
}

/* 3x5 digits of video_fps.c, drawn straight into the screen in colour 15 */
static void Video_DrawFPS(uint8 *base, int bpr)
{
	static const uint8 fontdigits[10] = {0167,044,0135,0155,056,0153,0173,045,0177,0157};
	static const uint8 segtest[15] = {03,01,05, 02,0,04, 032,010,054, 020,0,040, 0120,0100,0140};
	char text[8];
	int len, i, j, r, c, k;
	int cell = 2;			/* screen pixels per font pixel */
	int x0, y0;

	len = sprintf(text, "%lu", (unsigned long)s_fpsShown);
	x0 = 0;
	y0 = s_height - 7 * cell;
	for (r = 0; r < 7 * cell; r++) memset(base + (y0 + r) * bpr + x0, 0, (len * 4 + 1) * cell);
	for (i = 0; i < len; i++) {
		uint8 seg = fontdigits[text[i] - '0'];
		for (j = 0; j < 15; j++) {
			if (!(seg & segtest[j])) continue;
			c = j % 3;
			r = j / 3;
			for (k = 0; k < cell; k++) {
				memset(base + (y0 + (r + 1) * cell + k) * bpr + x0 + (i * 4 + 1 + c) * cell, 15, cell);
			}
		}
	}
}

/**
 * The game moved the picture of the viewport (x, y, w, h) by dx, dy pixels
 * (GUI_DrawScreen scrolling): the next tick moves it in VRAM with the blitter.
 * Returns true when the card follows (the moved part need not be sent; when
 * it then cannot, the whole screen is sent), false when the caller must mark
 * the viewport changed.
 */
bool Video_ScrollView(int16 dx, int16 dy, int16 x, int16 y, int16 w, int16 h)
{
	if (!s_vramScroll || s_shadow == NULL || !s_shadowValid || s_sx2 != 2 || s_sy != 1 || s_screenOffset != 0) return false;
	if (s_scrollPending && (x != s_scrollRect[0] || y != s_scrollRect[1] || w != s_scrollRect[2] || h != s_scrollRect[3])) {
		s_shadowValid = false;	/* two different areas: send everything */
		return true;
	}
	s_scrollRect[0] = x;
	s_scrollRect[1] = y;
	s_scrollRect[2] = w;
	s_scrollRect[3] = h;
	s_scrollDX += dx;
	s_scrollDY += dy;
	s_scrollPending = true;
	return true;
}

/* The pending scroll in VRAM (blitter) and in the copy; false if not possible. */
static bool Video_DoScroll(void)
{
	int16 dx = s_scrollDX, dy = s_scrollDY;
	int16 x = s_x0 + s_scrollRect[0], y = s_y0 + s_scrollRect[1], w = s_scrollRect[2], h = s_scrollRect[3];
	int16 cw, ch, sx, sy, tx, ty, r;

	s_scrollPending = false;
	s_scrollDX = s_scrollDY = 0;
	if (dx == 0 && dy == 0) return true;
	cw = w - (dx < 0 ? -dx : dx);
	ch = h - (dy < 0 ? -dy : dy);
	if (cw <= 0 || ch <= 0) return false;
	sx = dx < 0 ? x - dx : x;
	tx = dx < 0 ? x : x + dx;
	sy = dy < 0 ? y - dy : y;
	ty = dy < 0 ? y : y + dy;

	BltBitMap(s_window->RPort->BitMap, sx, sy, s_window->RPort->BitMap, tx, ty, cw, ch, 0xC0, 0xFF, NULL);
	WaitBlit();
	g_profBlits++;
	if (dy > 0) {
		for (r = ch - 1; r >= 0; r--) memmove(s_shadow + (ty + r) * s_width + tx, s_shadow + (sy + r) * s_width + sx, cw);
	} else {
		for (r = 0; r < ch; r++) memmove(s_shadow + (ty + r) * s_width + tx, s_shadow + (sy + r) * s_width + sx, cw);
	}
	return true;
}

/* Sends the blocks [x1, x2) of row y that differ from the copy (1:1). */
static void Video_SendRowCompared(uint8 *base, int bpr, const uint8 *row, int y, int x1, int x2)
{
	uint8 *sh = s_shadow + (s_y0 + y) * s_width + s_x0;
	uint8 *dst = base + (s_y0 + y) * bpr + s_x0;
	int x;

	for (x = x1; x < x2; x += 16) {
		const uint32 *a = (const uint32 *)(row + x);
		uint32 *b = (uint32 *)(sh + x);
		if (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]) continue;
		b[0] = a[0]; b[1] = a[1]; b[2] = a[2]; b[3] = a[3];
		Video_CopyLongs((uint32 *)(dst + x), a, 16);
		g_profBytes += 16;
	}
}

static void Video_Tick_(void);

void Video_Tick(void)
{
	PROF_BEGIN(PROF_VIDEO)
	Video_Tick_();
	PROF_END(PROF_VIDEO)
}

static void Video_Tick_(void)
{
	struct RenderInfo ri;
	LONG lock;
	uint8 *data;
	uint8 *base;
	int bpr, y;
	bool drawn = false;
	bool rectShow;
	bool compare;
	uint32 sendT0;

	if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
		SetSignal(0, SIGBREAKF_CTRL_C);
		exit(0);
	}
	if (s_window == NULL) return;

	PROF_BEGIN(PROF_V_EVENTS)
	Video_HandleEvents();
	PROF_END(PROF_V_EVENTS)

	if (SCREEN_WIDTH != s_lw || SCREEN_HEIGHT != s_lh) {
		/* the game switched between the menus and the map */
		Video_SetGeometry();
		SetRast(s_window->RPort, 0);
		s_needRepaint = true;
	}

	{
		int16 r[4];
		rectShow = RTS_GetDragRect(&r[0], &r[1], &r[2], &r[3]);
		if (s_rectShown && (!rectShow || memcmp(r, s_rect, sizeof(r)) != 0)) {
			/* send the picture under the old rectangle's edges again */
			GFX_Screen_SetDirty(SCREEN_0, s_rect[0], s_rect[1], s_rect[2] + 1, s_rect[1] + 1);
			GFX_Screen_SetDirty(SCREEN_0, s_rect[0], s_rect[3], s_rect[2] + 1, s_rect[3] + 1);
			GFX_Screen_SetDirty(SCREEN_0, s_rect[0], s_rect[1], s_rect[0] + 1, s_rect[3] + 1);
			GFX_Screen_SetDirty(SCREEN_0, s_rect[2], s_rect[1], s_rect[2] + 1, s_rect[3] + 1);
			s_rectShown = false;
		}
		if (rectShow) memcpy(s_rect, r, sizeof(r));
	}

	/* 1:1 with the copy: compare with it, scroll with the blitter */
	compare = s_shadow != NULL && s_sx2 == 2 && s_sy == 1 && s_screenOffset == 0;
	if (!compare) s_shadowValid = false;
	if (s_scrollPending) {
		if (!compare || !s_shadowValid || s_needRepaint || !Video_DoScroll()) {
			s_scrollPending = false;
			s_scrollDX = s_scrollDY = 0;
			s_shadowValid = false;
		}
	}
	if (compare && !s_shadowValid) s_needRepaint = true;

	if (!GFX_Screen_IsDirty(SCREEN_0) && !s_needRepaint && !s_showFPS && !(rectShow && !s_rectShown)) return;

	sendT0 = AmigaTimer_EClock();
	lock = p96LockBitMap(s_window->RPort->BitMap, (UBYTE *)&ri, sizeof(ri));
	g_profTicks[PROF_V_LOCK] += AmigaTimer_EClock() - sendT0;
	g_profCalls[PROF_V_LOCK]++;
	if (lock == 0) return;
	base = (uint8 *)ri.Memory;
	bpr = ri.BytesPerRow;

	data = GFX_Screen_Get_ByIndex(SCREEN_0) + (s_screenOffset << 2);

	if (GFX_Screen_IsDirty(SCREEN_0) || s_needRepaint) {
		struct dirty_area *area = GFX_Screen_GetDirtyArea(SCREEN_0);
		int top = 0, bottom = SCREEN_HEIGHT;

		if (!s_needRepaint && area != NULL && g_profStatsOn) {
			/* which parts of the screen changed (debug_stats only) */
			int sx = SCREEN_WIDTH - 80;
			PROF_BEGIN(PROF_V_AREAS)
			if (Video_AreaDirty(0, 0, SCREEN_WIDTH - 1, 39)) g_profAreas[PROF_AREA_TOP]++;
			if (Video_AreaDirty(0, 40, sx - 1, SCREEN_HEIGHT - 1)) g_profAreas[PROF_AREA_VIEW]++;
			if (Video_AreaDirty(sx + 16, 40, SCREEN_WIDTH - 1, 105)) g_profAreas[PROF_AREA_RADAR]++;
			if (Video_AreaDirty(sx, 106, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1)) g_profAreas[PROF_AREA_PANEL]++;
			PROF_END(PROF_V_AREAS)
			top = area->top;
			bottom = area->bottom > SCREEN_HEIGHT ? SCREEN_HEIGHT : area->bottom;
		}
		for (y = top; y < bottom; y++) {
			int x1, x2, n, r;
			uint8 *dst;

			if (s_needRepaint) {
				x1 = 0;
				x2 = SCREEN_WIDTH;
			} else {
				/* GFX_DIRTY_WORDS words of 32 blocks of 16 pixels */
				const uint32 *b = g_dirty_blocks[y];
				int w1 = 0, w2 = GFX_DIRTY_WORDS - 1;
				while (w1 < GFX_DIRTY_WORDS && b[w1] == 0) w1++;
				if (w1 == GFX_DIRTY_WORDS) continue;
				while (b[w2] == 0) w2--;
				x1 = (w1 * 32 + __builtin_ctz(b[w1])) << 4;
				x2 = (w2 * 32 + 32 - __builtin_clz(b[w2])) << 4;
				if (x2 > SCREEN_WIDTH) x2 = SCREEN_WIDTH;
			}
			if (compare && !s_needRepaint) {
				Video_SendRowCompared(base, bpr, data + y * SCREEN_WIDTH, y, x1, x2);
				continue;
			}
			if (compare) memcpy(s_shadow + (s_y0 + y) * s_width + s_x0 + x1, data + y * SCREEN_WIDTH + x1, x2 - x1);
			n = Video_WidenRow(data + y * SCREEN_WIDTH, x1, x2);
			n = (n + 3) & ~3;
			dst = base + (s_y0 + y * s_sy) * bpr + s_x0 + s_colX[x1];
			for (r = 0; r < s_sy; r++, dst += bpr) {
				Video_CopyLongs((uint32 *)dst, s_line, n);
			}
			g_profBytes += n * s_sy;
		}
		GFX_Screen_SetClean(SCREEN_0);
		if (compare && s_needRepaint) s_shadowValid = true;
		s_needRepaint = false;
		drawn = true;
		g_profFrames++;
	}

	if (rectShow && (drawn || !s_rectShown)) {
		/* over what was just sent */
		int x0 = s_x0 + s_colX[s_rect[0]], x1 = s_x0 + s_colX[s_rect[2] + 1] - 1;
		int y0 = s_y0 + s_rect[1] * s_sy, y1 = s_y0 + (s_rect[3] + 1) * s_sy - 1;
		int y, k;
		for (k = 0; k < 2; k++) {
			/* the card, and the copy that mirrors it */
			uint8 *b = k == 0 ? base : s_shadow;
			int pitch = k == 0 ? bpr : s_width;
			if (k == 1 && !compare) break;
			memset(b + y0 * pitch + x0, RECT_COLOUR, x1 - x0 + 1);
			memset(b + y1 * pitch + x0, RECT_COLOUR, x1 - x0 + 1);
			for (y = y0; y <= y1; y++) {
				b[y * pitch + x0] = RECT_COLOUR;
				b[y * pitch + x1] = RECT_COLOUR;
			}
		}
		s_rectShown = true;
	}

	if (s_showFPS) {
		uint32 now = Timer_GetTime();
		if (drawn) s_fpsFrames++;
		if (now - s_fpsTime >= 1000) {
			s_fpsShown = s_fpsFrames * 1000 / (now - s_fpsTime);
			s_fpsFrames = 0;
			s_fpsTime = now;
		}
		Video_DrawFPS(base, bpr);
		if (compare) Video_DrawFPS(s_shadow, s_width);
	}

	p96UnlockBitMap(s_window->RPort->BitMap, lock);
	g_profTicks[PROF_V_SEND] += AmigaTimer_EClock() - sendT0;
	g_profCalls[PROF_V_SEND]++;
}

/** Bug report: what the screen shows, read back from the card (s_width x
 * s_height bytes into dst). Only for reports: the card is read. */
bool Video_Amiga_ReadScreen(uint8 *dst, int *w, int *h)
{
	struct RenderInfo ri;
	LONG lock;
	int y;

	if (s_window == NULL) return false;
	lock = p96LockBitMap(s_window->RPort->BitMap, (UBYTE *)&ri, sizeof(ri));
	if (lock == 0) return false;
	for (y = 0; y < s_height; y++) memcpy(dst + y * s_width, (uint8 *)ri.Memory + y * ri.BytesPerRow, s_width);
	p96UnlockBitMap(s_window->RPort->BitMap, lock);
	*w = s_width;
	*h = s_height;
	return true;
}

/** Bug report: the copy of the screen (NULL when not in use). */
const uint8 *Video_Amiga_Shadow(int *w, int *h)
{
	if (s_shadow == NULL || !s_shadowValid) return NULL;
	*w = s_width;
	*h = s_height;
	return s_shadow;
}

/** Bug report: the video driver's state. */
void Video_Amiga_Report(void)
{
	AmigaBug_Printf("screen %dx%d, game %dx%d at %d,%d, scale %d/2 x %d; copy %d valid %d, scroll pending %d (%d,%d), repaint %d, offset %u, fps shown %lu\n",
	                s_width, s_height, s_lw, s_lh, s_x0, s_y0, s_sx2, s_sy, s_shadow != NULL, (int)s_shadowValid,
	                (int)s_scrollPending, s_scrollDX, s_scrollDY, (int)s_needRepaint, (unsigned)s_screenOffset,
	                (unsigned long)s_fpsShown);
}

void Video_SetPalette(void *palette, int from, int length)
{
	static ULONG table[1 + 256 * 3 + 1];
	uint8 *p = palette;
	int i;

	memcpy(s_palette + from * 3, p, length * 3);
	if (s_screen == NULL) return;
	table[0] = ((ULONG)length << 16) | (ULONG)from;
	for (i = 0; i < length * 3; i++) {
		uint8 v = p[i] & 0x3F;		/* VGA DAC: 6 bits */
		v = (uint8)((v << 2) | (v >> 4));
		table[1 + i] = (ULONG)v * 0x01010101UL;
	}
	table[1 + length * 3] = 0;
	LoadRGB32(&s_screen->ViewPort, table);
}

void Video_Mouse_SetPosition(uint16 x, uint16 y)
{
	struct InputEvent ie;
	struct IEPointerPixel pp;

	s_mouseX = x;
	s_mouseY = y;
	if (s_inputReq == NULL || s_screen == NULL) return;
	/* the middle of the game pixel, so that it maps back to x, y */
	memset(&ie, 0, sizeof(ie));
	pp.iepp_Screen = s_screen;
	pp.iepp_Position.X = s_x0 + (x * s_sx2 + 1) / 2;
	pp.iepp_Position.Y = s_y0 + y * s_sy + s_sy / 2;
	ie.ie_Class = IECLASS_NEWPOINTERPOS;
	ie.ie_SubClass = IESUBCLASS_PIXEL;
	ie.ie_EventAddress = &pp;
	s_inputReq->io_Command = IND_WRITEEVENT;
	s_inputReq->io_Data = &ie;
	s_inputReq->io_Length = sizeof(ie);
	DoIO((struct IORequest *)s_inputReq);
}

void Video_Mouse_SetRegion(uint16 minX, uint16 maxX, uint16 minY, uint16 maxY)
{
	s_mouseMinX = minX;
	s_mouseMaxX = maxX;
	s_mouseMinY = minY;
	s_mouseMaxY = maxY;
	if (s_mouseX < s_mouseMinX) s_mouseX = s_mouseMinX;
	if (s_mouseX > s_mouseMaxX) s_mouseX = s_mouseMaxX;
	if (s_mouseY < s_mouseMinY) s_mouseY = s_mouseMinY;
	if (s_mouseY > s_mouseMaxY) s_mouseY = s_mouseMaxY;
}

/* Screen shake: the picture starts offset * 4 bytes later (VGA start address). */
void Video_SetOffset(uint16 offset)
{
	s_screenOffset = offset;
	s_needRepaint = true;
}

void *Video_GetFrameBuffer(uint32 size)
{
	(void)size;
	return s_framebuffer;
}
