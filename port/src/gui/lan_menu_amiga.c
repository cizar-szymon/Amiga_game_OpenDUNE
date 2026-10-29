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
	GUI_DrawText_Wrapper(b->text, b->x + b->w / 2, b->y + 3, b->enabled ? (hover ? COL_HOVER : COL_TEXT) : COL_DIM, 0, 0x122);
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
	Lan_Text("Players", 26, y, COL_TITLE);
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		LanPlayer *p = &g_lanPlayers[slot];
		int yy = y + 10 + slot * 9;
		if (!p->used) {
			snprintf(line, sizeof(line), "%d. (free)", slot + 1);
			Lan_Text(line, 30, yy, COL_DIM);
			continue;
		}
		snprintf(line, sizeof(line), "%d. %s%s", slot + 1, p->name, slot == g_lanMe ? " (you)" : "");
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
	Lan_Text("Your house", 26, y + 3, COL_TITLE);
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
			GUI_DrawText_Wrapper(b->text, b->x + b->w / 2, b->y + 3, COL_HOVER, 0, 0x122);
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

static void Lan_Draw(void)
{
	char line[48];
	int i;

	GUI_Mouse_Hide_Safe();
	GUI_DrawBorder(16, 6, 288, 188, 1, true);
	GUI_DrawText_Wrapper(NULL, 0, 0, 0, 0, 0x22);

	switch (s_view) {
		case VIEW_CHOOSE:
			Lan_TextCentre("Play on LAN", 14, COL_TITLE);
			Lan_TextCentre("Two or three players, each on an Amiga", 40, COL_TEXT);
			Lan_TextCentre("in one network. Each plays a house:", 50, COL_TEXT);
			Lan_TextCentre("Atreides, Harkonnen or Ordos.", 60, COL_TEXT);
			Lan_TextCentre("START SERVER: this Amiga sets up the game.", 84, COL_TEXT);
			Lan_TextCentre("JOIN SERVER: find the server or type its", 100, COL_TEXT);
			Lan_TextCentre("address and connect.", 110, COL_TEXT);
			if (!s_netOk) Lan_TextCentre("No TCP/IP stack (bsdsocket.library).", 140, COL_HOVER);
			Lan_SetButtons(3, "START SERVER", s_netOk, "JOIN SERVER", s_netOk, "BACK", true);
			break;

		case VIEW_SERVER:
			Lan_TextCentre("LAN game - server", 14, COL_TITLE);
			Lan_Text("Server name", 26, 30, COL_TEXT);
			Lan_DrawField(110, 28, 100, s_serverName, s_field == FIELD_NAME);
			Lan_Text("Server address", 26, 44, COL_TEXT);
			Lan_Text(s_myAddress, 112, 44, COL_TEXT);
			Lan_Text("Map", 26, 56, COL_TEXT);
			snprintf(line, sizeof(line), "< %s >", Lan_MapName(g_lanMap));
			Lan_Text(line, 112, 56, Lan_Over(110, 54, 170, 10) ? COL_HOVER : COL_TEXT);
			Lan_Text("Level", 26, 66, COL_TEXT);
			snprintf(line, sizeof(line), "< %d >  (as mission %d)", g_lanLevel, g_lanLevel);
			Lan_Text(line, 112, 66, Lan_Over(110, 64, 170, 10) ? COL_HOVER : COL_TEXT);
			Lan_DrawPlayers(78);
			Lan_DrawHouses(118);
			if (s_serving && Lan_PlayerCount() > Lan_MapPlayers(g_lanMap)) {
				Lan_TextCentre("This map is for 2 players.", 152, COL_HOVER);
			} else {
				Lan_TextCentre(s_status, 152, COL_HOVER);
			}
			Lan_SetButtons(3, s_serving ? "STOP SERVER" : "RUN SERVER", true,
			               "START GAME", s_serving && Lan_PlayerCount() >= 2 && Lan_PlayerCount() <= Lan_MapPlayers(g_lanMap)
			               && g_lanPlayers[0].house != HOUSE_INVALID, "CANCEL", true);
			break;

		case VIEW_JOIN:
			Lan_TextCentre("LAN game - join", 14, COL_TITLE);
			Lan_Text(s_searching ? "Searching the network..." : "Servers found", 26, 28, COL_TITLE);
			if (s_foundCount == 0 && !s_searching) {
				Lan_Text("None - SEARCH or type the address below.", 30, 40, COL_DIM);
			}
			for (i = 0; i < s_foundCount; i++) {
				int y = 40 + i * 10;
				bool same = strcmp(s_found[i].version, LAN_VERSION) == 0;
				snprintf(line, sizeof(line), "%s  %s  %d/3%s%s", s_found[i].name, s_found[i].address, s_found[i].players,
				         same ? "" : "  v", same ? "" : s_found[i].version);
				Lan_Text(line, 30, y, !same ? COL_DIM : Lan_Over(26, y - 1, 260, 10) ? COL_HOVER : COL_TEXT);
			}
			Lan_Text("Your name", 26, 100, COL_TEXT);
			Lan_DrawField(110, 98, 100, s_playerName, s_field == FIELD_NAME);
			Lan_Text("Server address", 26, 116, COL_TEXT);
			Lan_DrawField(110, 114, 100, s_address, s_field == FIELD_ADDRESS);
			Lan_TextCentre(s_status, 152, COL_HOVER);
			Lan_SetButtons(3, "SEARCH", !s_searching && !s_connecting, s_connecting ? "CONNECTING" : "CONNECT", s_address[0] != 0 && !s_connecting, "BACK", true);
			break;

		case VIEW_JOINED:
			Lan_TextCentre("LAN game - joined", 14, COL_TITLE);
			snprintf(line, sizeof(line), "Server %s  %s", g_lanPlayers[0].name, s_address);
			Lan_Text(line, 26, 30, COL_TEXT);
			snprintf(line, sizeof(line), "Map  %s", Lan_MapName(g_lanMap));
			Lan_Text(line, 26, 44, COL_TEXT);
			snprintf(line, sizeof(line), "Level  %d  (as mission %d)", g_lanLevel, g_lanLevel);
			Lan_Text(line, 26, 54, COL_TEXT);
			Lan_DrawPlayers(74);
			Lan_DrawHouses(118);
			Lan_TextCentre(g_lanPlayers[0].house == HOUSE_INVALID ? "The server chooses its house first." : "Waiting for the server to start the game.", 140, COL_TEXT);
			Lan_TextCentre(s_status, 152, COL_HOVER);
			Lan_SetButtons(1, "DISCONNECT", true, NULL, false, NULL, false);
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
	Lan_Status("Starting the server...");
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
	Lan_SendStart(s_startSeed, s_startDelay);
	Net_Close();
	return true;
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
				Lan_Status(ev.text1);
				if (ev.value) {
					s_serving = s_searching = s_connecting = false;
				}
				break;
			case NEV_SERVING:
				s_serving = true;
				Lan_Status("Waiting for players...");
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
					g_lanPlayers[slot].house = HOUSE_INVALID;
					g_lanPlayers[slot].ping = -1;
					strncpy(g_lanPlayers[slot].name, ev.text1, 15);
					strncpy(g_lanPlayers[slot].address, ev.text2, 15);
					Lan_SendLobby();
					Lan_Status(Lan_PlayerCount() == 3 ? "Three players: START GAME." : "A player has joined.");
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
				if (g_lanServer) {
					if (slot < LAN_MAX_PLAYERS) {
						g_lanPlayers[slot].used = false;
						g_lanPlayers[slot].house = HOUSE_INVALID;
					}
					Lan_SendLobby();
					Lan_Status("A player has left.");
				} else {
					s_view = VIEW_JOIN;
					Lan_Status(ev.text1);
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
				} else if (!g_lanServer && ev.value == Lan_MsgType(2) && ev.length >= 7) {
					s_startSeed = ((uint32)ev.data[0] << 24) | ((uint32)ev.data[1] << 16) | ((uint32)ev.data[2] << 8) | ev.data[3];
					s_startDelay = ev.data[4];
					g_lanMap = ev.data[5];
					g_lanLevel = ev.data[6];
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
				g_lanServer = true;
				g_lanMe = 0;
				Lan_Status("RUN SERVER to be found.");
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
			/* map and level: the left half back, the right half on */
			if (Lan_Over(110, 54, 170, 10)) {
				g_lanMap = (g_lanMap + (g_mouseX < 195 ? Lan_MapCount() - 1 : 1)) % Lan_MapCount();
				if (s_serving) Lan_SendLobby();
				return false;
			}
			if (Lan_Over(110, 64, 170, 10)) {
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
				snprintf(text, sizeof(text), "Connecting to %s...", s_address);
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
				Lan_Status("Disconnected.");
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

	GUI_Mouse_Hide_Safe();
	GUI_ClearScreen(SCREEN_0);
	GUI_Mouse_Show_Safe();
	Input_History_Clear();
	s_dirty = true;

	while (!leave && !start) {
		uint16 key = 0;

		if (s_netOk && Lan_Events()) start = true;
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
