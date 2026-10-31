/** @file src/gui/lan_menu_amiga.c The windows of a LAN game in the main menu (Amiga). */

/*
 * As the LAN window of the Settlers port:
 * - choose: START SERVER (this Amiga sets up the game) or JOIN SERVER;
 * - server: its name, address, the map, the players (up to three) and their
 *   houses; RUN SERVER / STOP SERVER, START GAME, CANCEL;
 * - join: the servers found (SEARCH) or an address typed in, your name;
 *   CONNECT, BACK; then joined: the players and houses, DISCONNECT.
 * The server chooses its house first (Atreides, Harkonnen, Ordos); the others
 * choose from what is left; who has not chosen at START GAME gets what is left.
 * Everything is drawn here on the 320x200 menu screen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "../file.h"
#include "../os/math.h"
#include "../os/net_amiga.h"
#include "../os/sleep.h"

#include "gui.h"
#include "font.h"
#include "../gfx.h"
#include "../house.h"
#include "../input/input.h"
#include "../input/mouse.h"
#include "../lan_amiga.h"
#include "../timer.h"
#include "../lang_amiga.h"

enum { VIEW_CHOOSE, VIEW_SERVER, VIEW_JOIN, VIEW_JOINED };
enum { FIELD_NONE, FIELD_NAME, FIELD_ADDRESS };

#define COL_TITLE 238
#define COL_TEXT  232
#define COL_HOVER 231
#define COL_DIM   233

typedef struct Button { int16 x, y, w; const char *text; bool enabled; } Button;

typedef struct Found { char name[16]; char address[16]; char version[16]; int players; } Found;

static int s_view;
static int s_field;
static char s_serverName[16] = "MASTER";
static char s_playerName[16] = "SLAVE";
static char s_address[16] = "";
static char s_myAddress[32] = "";
static char s_status[48] = "";
static Found s_found[4];
static int s_foundCount;
static bool s_searching, s_serving, s_connecting;
static bool s_netOk;
/* the server's LAN saves (_lanNNN.dat), the one chosen (-1: a new game) */
#define LAN_SAVES 10
static int s_saveCount, s_saveSel = -1;
static char s_saveFiles[LAN_SAVES][16];
static LanSaveInfo s_saveInfo[LAN_SAVES];

/* sending the chosen save to the joiners (MT_FILE [size][offset][data]) */
static uint8 *s_xferBuf;
static uint32 s_xferSize, s_xferSum;
static uint32 s_xferOff[LAN_MAX_PLAYERS];
static bool s_xferEnd[LAN_MAX_PLAYERS], s_xferOk[LAN_MAX_PLAYERS];
static bool s_xferring;
/* receiving it */
static uint8 *s_rxBuf;
static uint32 s_rxSize, s_rxGot;

static uint32 s_startSeed;
static int s_startDelay;
static bool s_dirty;

/* ---- drawing */

static void Lan_Text(const char *t, int x, int y, uint8 colour)
{
	GUI_DrawText_Wrapper(t, x, y, colour, 0, 0x22);
}

static void Lan_TextCentre(const char *t, int y, uint8 colour)
{
	GUI_DrawText_Wrapper(t, 160, y, colour, 0, 0x122);
}

static bool Lan_Over(int x, int y, int w, int h)
{
	return g_mouseX >= x && g_mouseX < x + w && g_mouseY >= y && g_mouseY < y + h;
}

static void Lan_DrawButton(const Button *b)
{
	bool hover = b->enabled && Lan_Over(b->x, b->y, b->w, 13);
	GUI_DrawBorder(b->x, b->y, b->w, 13, 2, true);
	GUI_DrawText_Wrapper(b->text, b->x + b->w / 2, b->y + 2, b->enabled ? (hover ? COL_HOVER : COL_TEXT) : COL_DIM, 0, 0x122);
}

static void Lan_DrawField(int x, int y, int w, const char *text, bool focus)
{
	char buf[24];
	GUI_DrawBorder(x, y, w, 11, 0, true);
	snprintf(buf, sizeof(buf), "%s%s", text, focus ? "_" : "");
	Lan_Text(buf, x + 3, y + 2, focus ? COL_HOVER : COL_TEXT);
}

static const char *Lan_HouseName(uint8 house)
{
	return house < HOUSE_MAX ? g_table_houseInfo[house].name : "-";
}

/* the players and their houses (server and joined views) */
static void Lan_DrawPlayers(int y)
{
	int slot;
	char line[48];
	Lan_Text(Lang_T("Players"), 26, y, COL_TITLE);
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		LanPlayer *p = &g_lanPlayers[slot];
		int yy = y + 10 + slot * 9;
		if (!p->used) {
			snprintf(line, sizeof(line), Lang_T("%d. (free)"), slot + 1);
			Lan_Text(line, 30, yy, COL_DIM);
			continue;
		}
		snprintf(line, sizeof(line), "%d. %s%s", slot + 1, p->name, slot == g_lanMe ? Lang_T(" (you)") : "");
		Lan_Text(line, 30, yy, slot == g_lanMe ? COL_HOVER : COL_TEXT);
		Lan_Text(Lan_HouseName(p->house), 150, yy, COL_TEXT);
		if (slot != g_lanMe && p->ping >= 0 && (g_lanServer || slot == 0)) {
			snprintf(line, sizeof(line), "%d ms", p->ping);
			Lan_Text(line, 236, yy, COL_TEXT);
		}
	}
}

/* the three houses to choose (server and joined views) */
static Button s_houseButtons[3];

static bool Lan_CanChoose(uint8 house)
{
	if (!Lan_HouseFree(house) && g_lanPlayers[g_lanMe].house != house) return false;
	/* the server chooses first */
	if (g_lanMe != 0 && g_lanPlayers[0].house == HOUSE_INVALID) return false;
	return true;
}

static void Lan_DrawHouses(int y)
{
	int i;
	Lan_Text(Lang_T("Your house"), 26, y + 3, COL_TITLE);
	for (i = 0; i < 3; i++) {
		uint8 house = Lan_Houses()[i];
		Button *b = &s_houseButtons[i];
		b->x = (int16)(100 + i * 66);
		b->y = (int16)y;
		b->w = 62;
		b->text = g_table_houseInfo[house].name;
		b->enabled = Lan_CanChoose(house);
		if (g_lanPlayers[g_lanMe].house == house) {
			GUI_DrawBorder(b->x, b->y, b->w, 13, 0, true);
			GUI_DrawText_Wrapper(b->text, b->x + b->w / 2, b->y + 2, COL_HOVER, 0, 0x122);
		} else {
			Lan_DrawButton(b);
		}
	}
}

static Button s_buttons[4];
static int s_buttonCount;

static void Lan_SetButtons(int count, const char *t0, bool e0, const char *t1, bool e1, const char *t2, bool e2)
{
	const char *t[3];
	bool e[3];
	int i, w = count == 3 ? 82 : 100, gap = count == 3 ? 8 : 20;
	int x = 160 - (count * w + (count - 1) * gap) / 2;
	t[0] = t0; t[1] = t1; t[2] = t2;
	e[0] = e0; e[1] = e1; e[2] = e2;
	s_buttonCount = count;
	for (i = 0; i < count; i++) {
		s_buttons[i].x = (int16)(x + i * (w + gap));
		s_buttons[i].y = 172;
		s_buttons[i].w = (int16)w;
		s_buttons[i].text = t[i];
		s_buttons[i].enabled = e[i];
	}
}

static int Lan_PlayerCount(void)
{
	int slot, n = 0;
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) if (g_lanPlayers[slot].used) n++;
	return n;
}

static uint32 Lan_FileSum(const uint8 *d, uint32 n)
{
	uint32 h = 2166136261UL, i;
	for (i = 0; i < n; i++) { h ^= d[i]; h *= 16777619UL; }
	return h;
}

static void Lan_PutBE(uint8 *b, uint32 v)
{
	b[0] = (uint8)(v >> 24); b[1] = (uint8)(v >> 16); b[2] = (uint8)(v >> 8); b[3] = (uint8)v;
}

static uint32 Lan_GetBE(const uint8 *b)
{
	return ((uint32)b[0] << 24) | ((uint32)b[1] << 16) | ((uint32)b[2] << 8) | b[3];
}

static void Lan_ScanSaves(void)
{
	int i;
	s_saveCount = 0;
	for (i = 0; i < 100 && s_saveCount < LAN_SAVES; i++) {
		char name[16];
		snprintf(name, sizeof(name), "_lan%03d.dat", i);
		if (!File_Exists_Personal(name)) continue;
		if (!Lan_ReadSaveInfo(name, &s_saveInfo[s_saveCount], NULL, 0)) continue;
		strcpy(s_saveFiles[s_saveCount], name);
		s_saveCount++;
	}
	s_saveSel = -1;
}

/* the server: a new game or one of the saves */
static void Lan_ChooseSave(int sel)
{
	int slot;
	s_saveSel = sel;
	if (sel < 0) {
		g_lanFromSave = false;
		g_lanSaveMask = 0;
		g_lanSaveDesc[0] = 0;
	} else {
		const LanSaveInfo *in = &s_saveInfo[sel];
		g_lanFromSave = true;
		strcpy(g_lanSaveFile, s_saveFiles[sel]);
		g_lanSaveMask = in->mask;
		strncpy(g_lanSaveDesc, in->desc, sizeof(g_lanSaveDesc) - 1);
		g_lanSaveDesc[sizeof(g_lanSaveDesc) - 1] = 0;
		g_lanMap = in->map;
		g_lanLevel = in->level;
		for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
			uint8 h = g_lanPlayers[slot].house;
			if (h != HOUSE_INVALID && (g_lanSaveMask & (1 << h)) == 0) g_lanPlayers[slot].house = HOUSE_INVALID;
		}
	}
	if (s_serving) Lan_SendLobby();
}

static int Lan_SaveHouses(void)
{
	int i, n = 0;
	for (i = 0; i < 8; i++) if (g_lanSaveMask & (1 << i)) n++;
	return n;
}

static void Lan_Draw(void)
{
	char line[48];
	int i;

	GUI_Mouse_Hide_Safe();
	GUI_DrawBorder(16, 6, 288, 188, 1, true);
	GUI_DrawText_Wrapper(NULL, 0, 0, 0, 0, 0x22);

	switch (s_view) {
		case VIEW_CHOOSE:
			Lan_TextCentre(Lang_T("Play on LAN"), 14, COL_TITLE);
			Lan_TextCentre(Lang_T("Two or three players, each on an Amiga"), 40, COL_TEXT);
			Lan_TextCentre(Lang_T("in one network. Each plays a house:"), 50, COL_TEXT);
			Lan_TextCentre(Lang_T("Atreides, Harkonnen or Ordos."), 60, COL_TEXT);
			Lan_TextCentre(Lang_T("START SERVER: this Amiga sets up the game."), 84, COL_TEXT);
			Lan_TextCentre(Lang_T("JOIN SERVER: find the server or type its"), 100, COL_TEXT);
			Lan_TextCentre(Lang_T("address and connect."), 110, COL_TEXT);
			if (!s_netOk) Lan_TextCentre(Lang_T("No TCP/IP stack (bsdsocket.library)."), 140, COL_HOVER);
			Lan_SetButtons(3, Lang_T("START SERVER"), s_netOk, Lang_T("JOIN SERVER"), s_netOk, Lang_T("BACK"), true);
			break;

		case VIEW_SERVER:
			Lan_TextCentre(Lang_T("LAN game - server"), 14, COL_TITLE);
			Lan_Text(Lang_T("Server name"), 26, 30, COL_TEXT);
			Lan_DrawField(110, 28, 100, s_serverName, s_field == FIELD_NAME);
			Lan_Text(Lang_T("Server address"), 26, 44, COL_TEXT);
			Lan_Text(s_myAddress, 112, 44, COL_TEXT);
			Lan_Text(Lang_T("Game"), 26, 54, COL_TEXT);
			if (s_saveSel < 0) {
				snprintf(line, sizeof(line), Lang_T("< New game >%s"), s_saveCount > 0 ? "" : Lang_T("  (no saves)"));
			} else {
				snprintf(line, sizeof(line), Lang_T("< Saved: %.20s >"), s_saveInfo[s_saveSel].desc);
			}
			Lan_Text(line, 112, 54, Lan_Over(110, 52, 170, 10) && s_saveCount > 0 ? COL_HOVER : COL_TEXT);
			Lan_Text(Lang_T("Map"), 26, 64, COL_TEXT);
			snprintf(line, sizeof(line), s_saveSel < 0 ? "< %s >" : "%s", Lan_MapName(g_lanMap));
			Lan_Text(line, 112, 64, Lan_Over(110, 62, 170, 10) && s_saveSel < 0 ? COL_HOVER : COL_TEXT);
			Lan_Text(Lang_T("Level"), 26, 74, COL_TEXT);
			snprintf(line, sizeof(line), s_saveSel < 0 ? "< %d >" : "%d", g_lanLevel);
			Lan_Text(line, 112, 74, Lan_Over(110, 72, 170, 10) && s_saveSel < 0 ? COL_HOVER : COL_TEXT);
			Lan_DrawPlayers(86);
			Lan_DrawHouses(124);
			if (s_xferring) {
				Lan_TextCentre(s_status, 152, COL_HOVER);
			} else if (s_saveSel >= 0 && s_serving && Lan_PlayerCount() != Lan_SaveHouses()) {
				snprintf(line, sizeof(line), Lang_T("The saved game is for %d players."), Lan_SaveHouses());
				Lan_TextCentre(line, 152, COL_HOVER);
			} else if (s_saveSel < 0 && s_serving && Lan_PlayerCount() > Lan_MapPlayers(g_lanMap)) {
				Lan_TextCentre(Lang_T("This map is for 2 players."), 152, COL_HOVER);
			} else {
				Lan_TextCentre(s_status, 152, COL_HOVER);
			}
			Lan_SetButtons(3, s_serving ? Lang_T("STOP SERVER") : Lang_T("RUN SERVER"), true,
			               Lang_T("START GAME"), s_serving && !s_xferring && Lan_PlayerCount() >= 2
			               && (s_saveSel < 0 ? Lan_PlayerCount() <= Lan_MapPlayers(g_lanMap) : Lan_PlayerCount() == Lan_SaveHouses())
			               && g_lanPlayers[0].house != HOUSE_INVALID, Lang_T("CANCEL"), true);
			break;

		case VIEW_JOIN:
			Lan_TextCentre(Lang_T("LAN game - join"), 14, COL_TITLE);
			Lan_Text(s_searching ? Lang_T("Searching the network...") : Lang_T("Servers found"), 26, 28, COL_TITLE);
			if (s_foundCount == 0 && !s_searching) {
				Lan_Text(Lang_T("None - SEARCH or type the address below."), 30, 40, COL_DIM);
			}
			for (i = 0; i < s_foundCount; i++) {
				int y = 40 + i * 10;
				bool same = strcmp(s_found[i].version, LAN_VERSION) == 0;
				snprintf(line, sizeof(line), "%s  %s  %d/3%s%s", s_found[i].name, s_found[i].address, s_found[i].players,
				         same ? "" : "  v", same ? "" : s_found[i].version);
				Lan_Text(line, 30, y, !same ? COL_DIM : Lan_Over(26, y - 1, 260, 10) ? COL_HOVER : COL_TEXT);
			}
			Lan_Text(Lang_T("Your name"), 26, 100, COL_TEXT);
			Lan_DrawField(110, 98, 100, s_playerName, s_field == FIELD_NAME);
			Lan_Text(Lang_T("Server address"), 26, 116, COL_TEXT);
			Lan_DrawField(110, 114, 100, s_address, s_field == FIELD_ADDRESS);
			Lan_TextCentre(s_status, 152, COL_HOVER);
			Lan_SetButtons(3, Lang_T("SEARCH"), !s_searching && !s_connecting, s_connecting ? Lang_T("CONNECTING") : Lang_T("CONNECT"), s_address[0] != 0 && !s_connecting, Lang_T("BACK"), true);
			break;

		case VIEW_JOINED:
			Lan_TextCentre(Lang_T("LAN game - joined"), 14, COL_TITLE);
			snprintf(line, sizeof(line), Lang_T("Server %s  %s"), g_lanPlayers[0].name, s_address);
			Lan_Text(line, 26, 30, COL_TEXT);
			snprintf(line, sizeof(line), Lang_T("Map  %s"), Lan_MapName(g_lanMap));
			Lan_Text(line, 26, 44, COL_TEXT);
			snprintf(line, sizeof(line), Lang_T("Level  %d"), g_lanLevel);
			Lan_Text(line, 26, 54, COL_TEXT);
			if (g_lanSaveMask != 0) {
				snprintf(line, sizeof(line), Lang_T("Saved game: %.20s"), g_lanSaveDesc);
				Lan_Text(line, 26, 64, COL_TITLE);
			}
			Lan_DrawPlayers(74);
			Lan_DrawHouses(118);
			Lan_TextCentre(g_lanPlayers[0].house == HOUSE_INVALID ? Lang_T("The server chooses its house first.") : Lang_T("Waiting for the server to start the game."), 140, COL_TEXT);
			Lan_TextCentre(s_status, 152, COL_HOVER);
			Lan_SetButtons(1, Lang_T("DISCONNECT"), true, NULL, false, NULL, false);
			break;
	}
	for (i = 0; i < s_buttonCount; i++) Lan_DrawButton(&s_buttons[i]);
	GUI_Mouse_Show_Safe();
	s_dirty = false;
}

/* ---- actions */

static void Lan_Status(const char *t)
{
	strncpy(s_status, t, sizeof(s_status) - 1);
	s_status[sizeof(s_status) - 1] = 0;
	s_dirty = true;
}

static void Lan_ClearPlayers(void)
{
	int slot;
	memset(g_lanPlayers, 0, sizeof(g_lanPlayers));
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		g_lanPlayers[slot].house = HOUSE_INVALID;
		g_lanPlayers[slot].ping = -1;
	}
}

static void Lan_RunServer(void)
{
	if (s_serving) {
		Net_Stop();
		s_serving = false;
		Lan_ClearPlayers();
		Lan_Status("");
		return;
	}
	Lan_ClearPlayers();
	g_lanServer = true;
	g_lanMe = 0;
	g_lanPlayers[0].used = true;
	strcpy(g_lanPlayers[0].name, s_serverName);
	Net_Serve(s_serverName);
	Lan_Status(Lang_T("Starting the server..."));
}

static void Lan_PickHouse(uint8 house)
{
	if (!Lan_CanChoose(house)) return;
	if (g_lanServer) {
		g_lanPlayers[0].house = house;
		Lan_SendLobby();
	} else {
		Net_Send(0, Lan_MsgType(1), &house, 1);
	}
	s_dirty = true;
}

static bool Lan_StartServerGame(void)
{
	int slot, ping = 0;
	Lan_GiveHouses();
	for (slot = 1; slot < LAN_MAX_PLAYERS; slot++) {
		if (g_lanPlayers[slot].used && g_lanPlayers[slot].ping > ping) ping = g_lanPlayers[slot].ping;
	}
	s_startDelay = Lan_DelayForPing(ping);
	s_startSeed = AmigaTimer_GetMs() * 2654435761UL;
	Lan_SendLobby();
	if (g_lanFromSave) {
		/* the saved game to every joiner first (Lan_SendFile), then START */
		FILE *fp = fopendatadir(SEARCHDIR_PERSONAL_DATA_DIR, g_lanSaveFile, "rb");
		long size;
		if (fp == NULL) { Lan_Status(Lang_T("The saved game cannot be read.")); return false; }
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		free(s_xferBuf);
		s_xferBuf = (size > 0) ? malloc(size) : NULL;
		if (s_xferBuf == NULL || fread(s_xferBuf, size, 1, fp) != 1) {
			fclose(fp);
			Lan_Status(Lang_T("The saved game cannot be read."));
			return false;
		}
		fclose(fp);
		s_xferSize = (uint32)size;
		s_xferSum = Lan_FileSum(s_xferBuf, s_xferSize);
		for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
			s_xferOff[slot] = 0;
			s_xferEnd[slot] = false;
			s_xferOk[slot] = !g_lanPlayers[slot].used || slot == 0;
		}
		s_xferring = true;
		Net_Close();
		Lan_Status(Lang_T("Sending the saved game: 0%"));
		return false;
	}
	Lan_SendStart(s_startSeed, s_startDelay);
	Net_Close();
	return true;
}

/* The server: the saved game to the joiners, as much as the network takes
 * now; true when every one has it (then START). */
static bool Lan_SendFile(void)
{
	int slot;
	uint32 sent = 0, total = 0;
	bool allOk = true;
	char text[48];

	for (slot = 1; slot < LAN_MAX_PLAYERS; slot++) {
		if (!g_lanPlayers[slot].used) continue;
		while (s_xferOff[slot] < s_xferSize) {
			uint8 b[8 + 200];
			uint32 n = min(200, s_xferSize - s_xferOff[slot]);
			Lan_PutBE(b, s_xferSize);
			Lan_PutBE(b + 4, s_xferOff[slot]);
			memcpy(b + 8, s_xferBuf + s_xferOff[slot], n);
			if (!Net_Send(slot - 1, Lan_MsgType(3), b, 8 + (int)n)) break;
			s_xferOff[slot] += n;
		}
		if (s_xferOff[slot] == s_xferSize && !s_xferEnd[slot]) {
			uint8 b[8];
			Lan_PutBE(b, s_xferSize);
			Lan_PutBE(b + 4, s_xferSum);
			if (Net_Send(slot - 1, Lan_MsgType(4), b, 8)) s_xferEnd[slot] = true;
		}
		sent += s_xferOff[slot];
		total += s_xferSize;
		if (!s_xferOk[slot]) allOk = false;
	}
	if (allOk) return true;
	if (sent < total) {
		snprintf(text, sizeof(text), Lang_T("Sending the saved game: %lu%%"), (unsigned long)(sent * 100 / max(total, 1)));
	} else {
		snprintf(text, sizeof(text), Lang_T("Sent: waiting for the players..."));
	}
	if (strcmp(text, s_status) != 0) Lan_Status(text);
	return false;
}

/* network events in the lobby; true: the game starts */
static bool Lan_Events(void)
{
	NetEvent ev;
	while (Net_Event(&ev)) {
		int slot = g_lanServer ? ev.peer + 1 : 0;
		s_dirty = true;
		switch (ev.kind) {
			case NEV_ADDRESS:
				strncpy(s_myAddress, ev.text1, sizeof(s_myAddress) - 1);
				break;
			case NEV_ERROR:
				Lan_Status(Lang_T(ev.text1));	/* net_amiga.c's texts: INDIRECT in make_port_strings.py */
				if (ev.value) {
					s_serving = s_searching = s_connecting = false;
				}
				break;
			case NEV_SERVING:
				s_serving = true;
				Lan_Status(Lang_T("Waiting for players..."));
				break;
			case NEV_SERVER: {
				int i;
				for (i = 0; i < s_foundCount; i++) if (strcmp(s_found[i].address, ev.text2) == 0) break;
				if (i == s_foundCount && s_foundCount < 4) s_foundCount++;
				if (i < 4) {
					strncpy(s_found[i].name, ev.text1, 15);
					strncpy(s_found[i].address, ev.text2, 15);
					strncpy(s_found[i].version, ev.text3, 15);
					s_found[i].players = (int)(ev.value & 15);
					if (s_address[0] == 0) strcpy(s_address, s_found[i].address);
				}
				break;
			}
			case NEV_SEARCH_DONE:
				s_searching = false;
				break;
			case NEV_PEER:
				if (g_lanServer) {
					if (slot >= LAN_MAX_PLAYERS) break;
					g_lanPlayers[slot].used = true;
					g_lanPlayers[slot].gone = false;
					g_lanPlayers[slot].lost = false;
					g_lanPlayers[slot].house = HOUSE_INVALID;
					g_lanPlayers[slot].ping = -1;
					strncpy(g_lanPlayers[slot].name, ev.text1, 15);
					strncpy(g_lanPlayers[slot].address, ev.text2, 15);
					Lan_SendLobby();
					Lan_Status(Lan_PlayerCount() == 3 ? Lang_T("Three players: START GAME.") : Lang_T("A player has joined."));
				} else {
					s_connecting = false;
					s_view = VIEW_JOINED;
					Lan_ClearPlayers();
					strncpy(g_lanPlayers[0].name, ev.text1, 15);
					g_lanPlayers[0].used = true;
					Lan_Status("");
				}
				break;
			case NEV_LOST:
				if (g_lanServer && s_xferring) {
					s_xferring = false;
					Lan_Status(Lang_T("A player left: the saved game is not sent."));
				}
				if (g_lanServer) {
					if (slot < LAN_MAX_PLAYERS) {
						g_lanPlayers[slot].used = false;
						g_lanPlayers[slot].house = HOUSE_INVALID;
					}
					Lan_SendLobby();
					Lan_Status(Lang_T("A player has left."));
				} else {
					s_view = VIEW_JOIN;
					Lan_Status(Lang_T(ev.text1));
				}
				break;
			case NEV_PING:
				if (slot < LAN_MAX_PLAYERS) g_lanPlayers[slot].ping = (int)ev.value;
				break;
			case NEV_MSG:
				if (g_lanServer && ev.value == Lan_MsgType(1) && ev.length >= 1 && slot < LAN_MAX_PLAYERS) {
					/* a player chooses a house: free, and the server has chosen */
					uint8 house = ev.data[0];
					if (g_lanPlayers[0].house != HOUSE_INVALID && (Lan_HouseFree(house) || g_lanPlayers[slot].house == house)) {
						g_lanPlayers[slot].house = house;
					}
					Lan_SendLobby();
				} else if (!g_lanServer && ev.value == Lan_MsgType(0)) {
					Lan_TakeLobby(ev.data, ev.length);
				} else if (g_lanServer && ev.value == Lan_MsgType(5) && ev.length >= 1 && slot < LAN_MAX_PLAYERS) {
					/* a joiner has the saved game (1) or not (0) */
					if (ev.data[0]) {
						s_xferOk[slot] = true;
					} else if (s_xferring) {
						s_xferring = false;
						Lan_Status(Lang_T("A player did not get the saved game."));
					}
				} else if (!g_lanServer && ev.value == Lan_MsgType(3) && ev.length > 8) {
					/* a part of the saved game */
					uint32 size = Lan_GetBE(ev.data), off = Lan_GetBE(ev.data + 4), n = ev.length - 8;
					char text[48];
					if (s_rxBuf == NULL || s_rxSize != size) {
						free(s_rxBuf);
						s_rxBuf = malloc(size);
						s_rxSize = size;
						s_rxGot = 0;
					}
					if (s_rxBuf != NULL && off + n <= size) {
						memcpy(s_rxBuf + off, ev.data + 8, n);
						s_rxGot = max(s_rxGot, off + n);
					}
					snprintf(text, sizeof(text), Lang_T("Receiving the saved game: %lu%%"), (unsigned long)(s_rxGot * 100 / max(size, 1)));
					Lan_Status(text);
				} else if (!g_lanServer && ev.value == Lan_MsgType(4) && ev.length >= 8) {
					/* all of it: checked, kept as _lanrecv.dat */
					uint8 ok = 0;
					if (s_rxBuf != NULL && s_rxGot == Lan_GetBE(ev.data) && Lan_FileSum(s_rxBuf, s_rxGot) == Lan_GetBE(ev.data + 4)) {
						uint8 fd = File_Open_Personal("_lanrecv.dat", FILE_MODE_WRITE);
						if (fd != 0xFF) {
							ok = File_Write(fd, s_rxBuf, s_rxGot) == s_rxGot;
							File_Close(fd);
						}
					}
					Lan_Status(ok ? Lang_T("The saved game is here: starting...") : Lang_T("The saved game came broken."));
					Net_Send(0, Lan_MsgType(5), &ok, 1);
				} else if (!g_lanServer && ev.value == Lan_MsgType(2) && ev.length >= 7) {
					s_startSeed = ((uint32)ev.data[0] << 24) | ((uint32)ev.data[1] << 16) | ((uint32)ev.data[2] << 8) | ev.data[3];
					s_startDelay = ev.data[4];
					g_lanMap = ev.data[5];
					g_lanLevel = ev.data[6];
					g_lanFromSave = ev.length >= 8 && ev.data[7] != 0;
					if (g_lanFromSave) strcpy(g_lanSaveFile, "_lanrecv.dat");
					free(s_rxBuf);
					s_rxBuf = NULL;
					return true;
				}
				break;
			default: break;
		}
	}
	return false;
}

static void Lan_TypeKey(uint16 key)
{
	char *text;
	int len, size = 16;
	uint8 c;

	if (s_field == FIELD_NONE) return;
	text = s_field == FIELD_ADDRESS ? s_address : (s_view == VIEW_SERVER ? s_serverName : s_playerName);
	len = (int)strlen(text);
	if (key == 0x0F) {	/* backspace */
		if (len > 0) text[len - 1] = 0;
		s_dirty = true;
		return;
	}
	if (key == 0x2B || key == 0x6E) {	/* return, esc */
		s_field = FIELD_NONE;
		s_dirty = true;
		return;
	}
	c = (uint8)(Input_Keyboard_HandleKeys(key) & 0xFF);
	if (s_field == FIELD_ADDRESS) {
		if (!((c >= '0' && c <= '9') || c == '.')) return;
	} else {
		if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return;
	}
	if (len >= size - 1) return;
	text[len] = (char)c;
	text[len + 1] = 0;
	s_dirty = true;
}

/* a click; true: the game starts */
static bool Lan_Click(int x, int y, bool *leave)
{
	int i, hit = -1;
	for (i = 0; i < s_buttonCount; i++) {
		Button *b = &s_buttons[i];
		if (b->enabled && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + 13) hit = i;
	}
	s_dirty = true;

	switch (s_view) {
		case VIEW_CHOOSE:
			if (hit == 0) {
				s_view = VIEW_SERVER;
				s_field = FIELD_NONE;
				Lan_ClearPlayers();
				Lan_ScanSaves();
				Lan_ChooseSave(-1);
				g_lanServer = true;
				g_lanMe = 0;
				Lan_Status(Lang_T("RUN SERVER to be found."));
			} else if (hit == 1) {
				s_view = VIEW_JOIN;
				s_field = FIELD_ADDRESS;
				s_foundCount = 0;
				g_lanServer = false;
				Lan_Status("");
			} else if (hit == 2) {
				*leave = true;
			}
			return false;

		case VIEW_SERVER:
			if (!s_serving && Lan_Over(110, 28, 100, 11)) {
				s_field = FIELD_NAME;
				return false;
			}
			s_field = FIELD_NONE;
			if (s_xferring) return false;
			/* game (new / a save), map and level: the left half back, the right half on */
			if (Lan_Over(110, 52, 170, 10)) {
				if (s_saveCount > 0) {
					int n = s_saveCount + 1;	/* -1 .. count-1 */
					Lan_ChooseSave((s_saveSel + 1 + (g_mouseX < 195 ? n - 1 : 1)) % n - 1);
				}
				return false;
			}
			if (s_saveSel >= 0 && (Lan_Over(110, 62, 170, 10) || Lan_Over(110, 72, 170, 10))) return false;
			if (Lan_Over(110, 62, 170, 10)) {
				g_lanMap = (g_lanMap + (g_mouseX < 195 ? Lan_MapCount() - 1 : 1)) % Lan_MapCount();
				if (s_serving) Lan_SendLobby();
				return false;
			}
			if (Lan_Over(110, 72, 170, 10)) {
				{
					int n = Lan_LevelCount() - Lan_LevelMin() + 1;
					g_lanLevel = (g_lanLevel - Lan_LevelMin() + (g_mouseX < 195 ? n - 1 : 1)) % n + Lan_LevelMin();
				}
				if (s_serving) Lan_SendLobby();
				return false;
			}
			if (s_serving || g_lanPlayers[0].used) {
				for (i = 0; i < 3; i++) {
					Button *b = &s_houseButtons[i];
					if (b->enabled && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + 13) Lan_PickHouse(Lan_Houses()[i]);
				}
			}
			if (hit == 0) Lan_RunServer();
			if (hit == 1) return Lan_StartServerGame();
			if (hit == 2) {
				Net_Stop();
				s_serving = false;
				s_view = VIEW_CHOOSE;
			}
			return false;

		case VIEW_JOIN:
			if (Lan_Over(110, 98, 100, 11)) { s_field = FIELD_NAME; return false; }
			if (Lan_Over(110, 114, 100, 11)) { s_field = FIELD_ADDRESS; return false; }
			for (i = 0; i < s_foundCount; i++) {
				if (Lan_Over(26, 40 + i * 10 - 1, 260, 10)) strcpy(s_address, s_found[i].address);
			}
			if (hit == 0) {
				s_foundCount = 0;
				s_searching = true;
				Net_Search(3000);
				Lan_Status("");
			} else if (hit == 1) {
				char text[48];
				s_connecting = true;
				g_lanServer = false;
				Net_Connect(s_address, s_playerName);
				snprintf(text, sizeof(text), Lang_T("Connecting to %s..."), s_address);
				Lan_Status(text);
			} else if (hit == 2) {
				Net_Stop();
				s_searching = s_connecting = false;
				s_view = VIEW_CHOOSE;
			}
			return false;

		case VIEW_JOINED:
			for (i = 0; i < 3; i++) {
				Button *b = &s_houseButtons[i];
				if (b->enabled && x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + 13) Lan_PickHouse(Lan_Houses()[i]);
			}
			if (hit == 0) {
				Net_Stop();
				s_view = VIEW_JOIN;
				Lan_Status(Lang_T("Disconnected."));
			}
			return false;
	}
	return false;
}

static uint32 s_lanSeed;
static int s_lanDelay;

/** The seed and delay of the game the lobby started (Lan_StartGame). */
void Lan_StartValues(uint32 *seed, int *delay)
{
	*seed = s_lanSeed;
	*delay = s_lanDelay;
}

/**
 * The LAN windows, from the main menu. True: a game was agreed on (the
 * players, houses, map, seed, delay are set; Lan_StartGame() starts it).
 */
bool Lan_Menu(void)
{
	static bool s_started = false;
	bool leave = false, start = false;
	uint16 lastX = 0xFFFF, lastY = 0xFFFF;

	if (!s_started) {
		s_netOk = Net_Start(LAN_VERSION) != 0;
		if (s_netOk) atexit(Net_Quit);
		s_started = true;
	}
	s_view = VIEW_CHOOSE;
	s_field = FIELD_NONE;
	s_serving = s_searching = s_connecting = false;
	s_status[0] = 0;
	Lan_ClearPlayers();
	g_lanFromSave = false;
	g_lanSaveMask = 0;
	g_lanSaveDesc[0] = 0;
	s_xferring = false;
	s_saveSel = -1;

	GUI_Mouse_Hide_Safe();
	GUI_ClearScreen(SCREEN_0);
	GUI_Mouse_Show_Safe();
	Input_History_Clear();
	s_dirty = true;

	while (!leave && !start) {
		uint16 key = 0;

		if (s_netOk && Lan_Events()) start = true;
		if (s_xferring && Lan_SendFile()) {
			s_xferring = false;
			free(s_xferBuf);
			s_xferBuf = NULL;
			Lan_SendStart(s_startSeed, s_startDelay);
			start = true;
		}
		if (Input_IsInputAvailable() != 0) key = Input_Wait() & 0x8FF;
		if (key == 0x41 || key == 0x42) {
			if (Lan_Click(g_mouseClickX, g_mouseClickY, &leave)) start = true;
		} else if (key == 0x6E && s_field == FIELD_NONE) {
			/* Esc: back */
			if (s_view == VIEW_CHOOSE) leave = true;
			else {
				Net_Stop();
				s_serving = s_searching = s_connecting = false;
				s_view = VIEW_CHOOSE;
				s_dirty = true;
			}
		} else if (key != 0 && (key & 0x800) == 0) {
			Lan_TypeKey(key);
		}
		if (g_mouseX != lastX || g_mouseY != lastY) {
			lastX = g_mouseX;
			lastY = g_mouseY;
			s_dirty = true;
		}
		if (s_dirty) Lan_Draw();
		GUI_PaletteAnimate();
		sleepIdle();
	}

	if (start) {
		s_lanSeed = s_startSeed;
		s_lanDelay = s_startDelay;
		return true;
	}
	Net_Stop();
	return false;
}
