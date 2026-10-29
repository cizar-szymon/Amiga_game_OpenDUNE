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
		const char *note = "NOT SENT - NO CONNECTION";
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
	w = 8 + (nameW + 2 + LAN_CHAT_LENGTH + 1) * 6;
	w = min(w, g_viewportTilesW * 16 * g_viewZoom - 8);
	rows = s_open ? LAN_CHAT_LINES : s_lineCount;
	h = 6 + rows * 8 + (s_open ? 12 : 0);

	old = GFX_Screen_SetActive(SCREEN_0);
	GUI_Mouse_Hide_Safe();
	GUI_DrawFilledRectangle((int16)x, (int16)y, (int16)(x + w - 1), (int16)(y + h - 1), 12);
	GUI_DrawFilledRectangle((int16)x, (int16)y, (int16)(x + w - 1), (int16)y, 15);
	GUI_DrawFilledRectangle((int16)x, (int16)(y + h - 1), (int16)(x + w - 1), (int16)(y + h - 1), 15);
	GUI_DrawFilledRectangle((int16)x, (int16)y, (int16)x, (int16)(y + h - 1), 15);
	GUI_DrawFilledRectangle((int16)(x + w - 1), (int16)y, (int16)(x + w - 1), (int16)(y + h - 1), 15);

	/* the history, bottom aligned */
	for (i = 0; i < s_lineCount; i++) {
		const ChatLine *l = &s_lines[i];
		int ty = y + 4 + (rows - s_lineCount + i) * 8;
		if (l->slot < 0) {
			GUI_DrawText_Wrapper("%s", (int16)(x + 4), (int16)ty, 15, 0, 0x22, l->text);	/* a note of the game */
			continue;
		}
		GUI_DrawText_Wrapper("%.10s:", (int16)(x + 4), (int16)ty, (uint8)g_table_houseInfo[g_lanPlayers[l->slot].house].minimapColor, 0, 0x22,
		                     g_lanPlayers[l->slot].name);
		GUI_DrawText_Wrapper("%s", (int16)(x + 4 + (nameW + 2) * 6), (int16)ty, 15, 0, 0x22, l->text);
	}
	if (s_open) {
		int ty = y + 4 + rows * 8 + 3;
		GUI_DrawFilledRectangle((int16)(x + 2), (int16)(ty - 2), (int16)(x + w - 3), (int16)(ty - 2),
		                        (uint8)g_table_houseInfo[g_lanPlayers[g_lanMe].house].minimapColor);
		GUI_DrawText_Wrapper("%.10s:", (int16)(x + 4), (int16)ty, (uint8)g_table_houseInfo[g_lanPlayers[g_lanMe].house].minimapColor, 0, 0x22,
		                     g_lanPlayers[g_lanMe].name);
		GUI_DrawText_Wrapper("%s%s", (int16)(x + 4 + (nameW + 2) * 6), (int16)ty, 15, 0, 0x22, s_typed,
		                     strlen(s_typed) < LAN_CHAT_LENGTH ? "_" : "");
	}
	GUI_Mouse_Show_Safe();
	GFX_Screen_SetActive(old);
	s_shown = true;
}
