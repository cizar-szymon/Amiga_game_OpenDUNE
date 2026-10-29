/** @file src/gui/lan_chat_amiga.c Chat between the players of a LAN game (AmigaOS).
 *
 * As in the Settlers port: Tab on the map screen opens a box at the top left
 * of the viewport (the last LAN_CHAT_LINES lines, the line being typed below
 * them), Return sends, Esc or Tab closes (the typed text stays), Backspace
 * deletes; up to LAN_CHAT_LENGTH letters, digits and . , : ? ! - and space.
 * A line that comes in while the box is closed shows it for 10 s with a sound.
 *
 * The keys come straight from the video driver's key handling (Lan_ChatKey,
 * raw Amiga codes), so while the box is open the game gets none of them. The
 * lines go at once through the network (MT_CHAT, outside the turns: they do
 * not change the game); the server passes a joiner's line on to the others.
 * The box is drawn over SCREEN_0 after each frame of the map screen.
 */

#include <stdio.h>
#include <string.h>

#include "types.h"
#include "../os/common.h"
#include "../os/math.h"
#include "../gfx.h"
#include "../house.h"
#include "../opendune.h"
#include "../audio/driver.h"
#include "../lan_amiga.h"
#include "gui.h"
#include "layout_amiga.h"
#include "../lang_amiga.h"

#define LAN_CHAT_LENGTH 32
#define LAN_CHAT_LINES  8
#define LAN_CHAT_SHOW   10000	/* ms a line that came in is shown */

typedef struct ChatLine {
	int8 slot;		/* the player; -1 a note of the game */
	char text[LAN_CHAT_LENGTH + 1];
} ChatLine;

static ChatLine s_lines[LAN_CHAT_LINES];
static int s_lineCount;
static char s_typed[LAN_CHAT_LENGTH + 1];
static bool s_open;
static uint32 s_shownUntil;
static bool s_shown;		/* the box was drawn in the last frame */
static bool s_shift;

/* raw key codes (US positions) -> letters; shifted in the second table */
static const char s_rawChars[0x41] =
	"`1234567890-=\\\0\0"	/* 00-0F */
	"qwertyuiop[]\0\0\0\0"	/* 10-1F */
	"asdfghjkl;'\0\0\0\0\0"	/* 20-2F */
	"\0zxcvbnm,./\0\0\0\0\0"	/* 30-3F */
	" ";			/* 40 */
static const char s_rawShifted[0x41] =
	"~!@#$%^&*()_+|\0\0"
	"QWERTYUIOP{}\0\0\0\0"
	"ASDFGHJKL:\"\0\0\0\0\0"
	"\0ZXCVBNM<>?\0\0\0\0\0"
	" ";

#define RAW_TAB       0x42
#define RAW_RETURN    0x44
#define RAW_ESC       0x45
#define RAW_BACKSPACE 0x41
#define RAW_LSHIFT    0x60
#define RAW_RSHIFT    0x61

/* What the game's font shows (the Settlers port's set and a few more). */
static bool Chat_Allowed(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' ||
	       c == '.' || c == ',' || c == ':' || c == '?' || c == '!' || c == '-';
}

static void Chat_Add(int slot, const uint8 *text, int length)
{
	ChatLine *l;
	int i, n = 0;

	if (s_lineCount == LAN_CHAT_LINES) {
		memmove(s_lines, s_lines + 1, sizeof(s_lines[0]) * (LAN_CHAT_LINES - 1));
		s_lineCount--;
	}
	l = &s_lines[s_lineCount++];
	l->slot = (int8)slot;
	for (i = 0; i < length && n < LAN_CHAT_LENGTH; i++) {
		if (Chat_Allowed((char)text[i])) l->text[n++] = (char)text[i];
	}
	l->text[n] = '\0';
}

static void Chat_Send(void)
{
	uint8 b[1 + LAN_CHAT_LENGTH];
	int n = (int)strlen(s_typed);
	if (n == 0) return;
	b[0] = (uint8)g_lanMe;
	memcpy(b + 1, s_typed, n);
	if (!Lan_SendChat(b, 1 + n)) {
		const char *note = Lang_T("NOT SENT - NO CONNECTION");
		Chat_Add(-1, (const uint8 *)note, (int)strlen(note));
	} else {
		Chat_Add(g_lanMe, (const uint8 *)s_typed, n);
	}
	s_typed[0] = '\0';
}

/** A line from another player (lan_amiga.c): slot, text. */
void Lan_ChatReceived(int slot, const uint8 *text, int length)
{
	Chat_Add(slot, text, length);
	if (!s_open) s_shownUntil = AmigaTimer_GetMs() + LAN_CHAT_SHOW;
	Driver_Sound_Play(52, 0xFF);
}

/* The map screen of a LAN game, no window over it. */
static bool Chat_OnMap(void)
{
	return g_lanGame && g_gameMode == GM_NORMAL && g_selectionType != SELECTIONTYPE_MENTAT && Layout_IsBig() && g_lanModal == 0;
}

/**
 * A key from the video driver (raw code, up). True when the chat takes it
 * (the game does not get it).
 */
bool Lan_ChatKey(uint16 code, bool up)
{
	if (code == RAW_LSHIFT || code == RAW_RSHIFT) {
		s_shift = !up;
		return false;
	}
	if (!s_open) {
		if (code == RAW_TAB && !up && !s_shift && Chat_OnMap()) {
			s_open = true;
			return true;
		}
		return false;
	}
	if (!Chat_OnMap()) {
		s_open = false;
		return false;
	}
	if (code >= 0x50 && code <= 0x59) return false;	/* F1..F10: as always */
	if (up) return true;
	switch (code) {
		case RAW_TAB:
		case RAW_ESC:
			s_open = false;
			s_shownUntil = 0;
			return true;
		case RAW_RETURN:
			Chat_Send();
			return true;
		case RAW_BACKSPACE: {
			int n = (int)strlen(s_typed);
			if (n > 0) s_typed[n - 1] = '\0';
			return true;
		}
		default: break;
	}
	if (code <= 0x40) {
		char c = s_shift ? s_rawShifted[code] : s_rawChars[code];
		int n = (int)strlen(s_typed);
		if (c != '\0' && Chat_Allowed(c) && n < LAN_CHAT_LENGTH) {
			s_typed[n] = c;
			s_typed[n + 1] = '\0';
		}
	}
	return true;	/* every other key too: no game key while typing */
}

/** The chat is open (its keys are not the game's: no zoom on Z). */
bool Lan_ChatOpen(void)
{
	return s_open;
}

/* The box looks like the message bar of the top of the screen (the user's
 * wish): its ends, its top and bottom edge (rows BAR_Y.., taken once from the
 * map screen) around a fill of its colour. */
#define BAR_Y      17	/* the bar's first row */
#define BAR_TOP    6	/* rows of its top edge */
#define BAR_MIDDLE 26	/* a row of its inside */
#define BAR_BOTTOM 8	/* rows of its bottom edge (31..38) */
#define BAR_ROWS   22
#define BAR_CAP    8	/* columns of each end */
#define CHAT_TEXT  12	/* dark text */
static uint8 s_barEnd[2][BAR_ROWS][BAR_CAP];	/* left, right end */
static uint8 s_barEdge[BAR_ROWS];		/* a column of the middle */
static bool s_barTaken;

static void Chat_TakeBar(void)
{
	const uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_0);
	int r;
	for (r = 0; r < BAR_ROWS; r++) {
		const uint8 *row = src + (BAR_Y + r) * SCREEN_WIDTH;
		memcpy(s_barEnd[0][r], row, BAR_CAP);
		memcpy(s_barEnd[1][r], row + SCREEN_WIDTH - BAR_CAP, BAR_CAP);
		s_barEdge[r] = row[40];
	}
	/* the fill: inside the left end (no message text there) */
	s_barEdge[BAR_MIDDLE - BAR_Y] = s_barEnd[0][BAR_MIDDLE - BAR_Y][6];
	s_barTaken = true;
}

static void Chat_DrawFrame(int x, int y, int w, int h)
{
	uint8 *dst = GFX_Screen_Get_ByIndex(SCREEN_0);
	int r;
	if (!s_barTaken) Chat_TakeBar();
	for (r = 0; r < h; r++) {
		uint8 *d = dst + (y + r) * SCREEN_WIDTH + x;
		int b = r < BAR_TOP ? r : r >= h - BAR_BOTTOM ? BAR_ROWS - (h - r) : BAR_MIDDLE - BAR_Y;
		memcpy(d, s_barEnd[0][b], BAR_CAP);
		memset(d + BAR_CAP, s_barEdge[b], w - 2 * BAR_CAP);
		memcpy(d + w - BAR_CAP, s_barEnd[1][b], BAR_CAP);
	}
	GFX_Screen_SetDirty(SCREEN_0, (uint16)x, (uint16)y, (uint16)(x + w), (uint16)(y + h));
}

static int s_boxX, s_boxY, s_boxW, s_boxH;	/* where the box was drawn */

/**
 * The box off SCREEN_0: the map under it again from SCREEN_1 (stretched when
 * zoomed), before the view is moved in SCREEN_0 (the box would move with it,
 * also into SCREEN_1). The next frame draws it again.
 */
void Lan_ChatHide(void)
{
	if (!s_shown) return;
	GUI_Mouse_Hide_Safe();
	g_zoomCopy++;
	GUI_Screen_Copy((int16)(s_boxX / 8), (int16)s_boxY, (int16)(s_boxX / 8), (int16)s_boxY,
	                (int16)((s_boxX + s_boxW + 7) / 8 - s_boxX / 8), (int16)s_boxH, SCREEN_1, SCREEN_0);
	g_zoomCopy--;
	GUI_Mouse_Show_Safe();
	s_shown = false;
}

/** After each frame of the map screen: the box over SCREEN_0, or the viewport again where it was. */
void Lan_ChatDraw(void)
{
	bool show;
	int i, rows, x = 4, y = 44, w, h, nameW = 0;
	Screen old;

	if (!g_lanGame) {
		s_open = false;
		s_lineCount = 0;
		s_typed[0] = '\0';
		s_shown = false;
		return;
	}
	show = Chat_OnMap() && (s_open || (s_lineCount > 0 && (int32)(s_shownUntil - AmigaTimer_GetMs()) > 0));
	if (!show) {
		if (s_shown) g_viewport_forceRedraw = true;	/* the map under the box again */
		s_shown = false;
		return;
	}

	for (i = 0; i < LAN_MAX_PLAYERS; i++) nameW = max(nameW, (int)strlen(g_lanPlayers[i].name));
	nameW = min(nameW, 10);
	w = 20 + (nameW + 2 + LAN_CHAT_LENGTH + 1) * 6;
	w = min(w, Layout_ViewWidth() - 8);
	rows = s_open ? LAN_CHAT_LINES : s_lineCount;
	h = BAR_TOP + 3 + rows * 8 + (s_open ? 11 : 0) + BAR_BOTTOM;

	old = GFX_Screen_SetActive(SCREEN_0);
	GUI_Mouse_Hide_Safe();
	Chat_DrawFrame(x, y, w, h);

	/* the history, bottom aligned; dark text as on the message bar */
	for (i = 0; i < s_lineCount; i++) {
		const ChatLine *l = &s_lines[i];
		int ty = y + BAR_TOP + 2 + (rows - s_lineCount + i) * 8;
		if (l->slot < 0) {
			GUI_DrawText_Wrapper("%s", (int16)(x + 10), (int16)ty, CHAT_TEXT, 0, 0x22, l->text);	/* a note of the game */
			continue;
		}
		GUI_DrawText_Wrapper("%.10s:", (int16)(x + 10), (int16)ty, (uint8)g_table_houseInfo[g_lanPlayers[l->slot].house].minimapColor, 0, 0x22,
		                     g_lanPlayers[l->slot].name);
		GUI_DrawText_Wrapper("%s", (int16)(x + 10 + (nameW + 2) * 6), (int16)ty, CHAT_TEXT, 0, 0x22, l->text);
	}
	if (s_open) {
		int ty = y + BAR_TOP + 2 + rows * 8 + 3;
		uint8 colour = (uint8)g_table_houseInfo[g_lanPlayers[g_lanMe].house].minimapColor;
		GUI_DrawFilledRectangle((int16)(x + 8), (int16)(ty - 2), (int16)(x + w - 9), (int16)(ty - 2), colour);
		GUI_DrawText_Wrapper("%.10s:", (int16)(x + 10), (int16)ty, colour, 0, 0x22, g_lanPlayers[g_lanMe].name);
		GUI_DrawText_Wrapper("%s%s", (int16)(x + 10 + (nameW + 2) * 6), (int16)ty, CHAT_TEXT, 0, 0x22, s_typed,
		                     strlen(s_typed) < LAN_CHAT_LENGTH ? "_" : "");
	}
	GUI_Mouse_Show_Safe();
	GFX_Screen_SetActive(old);
	s_shown = true;
	s_boxX = x;
	s_boxY = y;
	s_boxW = w;
	s_boxH = h;
}
