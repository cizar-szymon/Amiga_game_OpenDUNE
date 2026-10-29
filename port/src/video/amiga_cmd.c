/** @file src/video/amiga_cmd.c Test commands for remote control (AmiAgent).
 *
 * With debug_commands=1 in opendune.ini the game looks for RAM:od-cmd four
 * times a second, reads it, deletes it and runs its lines one after another,
 * in game coordinates (320x200, on the map 640x480), so that a test does not depend on the screen
 * mode or on reading the screen:
 *   move X Y | click X Y | rclick X Y   mouse (move, press, held 6 ticks)
 *   ldown X Y | lmove X Y | lup         drag with the left button
 *   key NAME                            esc space return enter tab backspace del
 *                                       up down left right f1..f10 a..z 0..9, 0xRAW
 *   type TEXT                           letters, digits, spaces
 *   wait N                              N ticks of 1/60 s
 *   shot                                RAM:od-shot.ppm, the game's buffer
 *                                       (320x200, on the map 640x480) and
 *                                       palette (no VRAM read)
 *   state                               RAM:od-state.txt (mode, mission, mouse,
 *                                       game timer)
 *   spawn TYPE HOUSE X Y DX DY          a unit at X,Y sent to DX,DY (tests)
 *   redraw                              the whole map drawn again: a shot
 *                                       before and after shows leftovers
 * When a file is finished, RAM:od-done holds the number of its lines run. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

/* as in video_amiga.c: gfx.h's enum Screen and exec's Debug() */
#undef Debug
#define Screen DuneScreen
#include "types.h"
#include "../gfx.h"
#include "../opendune.h"
#include "../house.h"


#include "../input/mouse.h"
#include "../timer.h"
extern bool g_viewport_forceRedraw;
#include "../os/error.h"
#undef Screen

extern void Video_Amiga_Mouse(int x, int y, bool left, bool right);
extern void Video_Amiga_Key(uint16 code);
extern const uint8 *Video_Amiga_Palette(void);
extern int RTS_GroupCount(void);
extern bool RTS_TestSpawn(uint8 type, uint8 house, int16 x, int16 y, int16 dx, int16 dy);
extern int RTS_SelectedUnit(void);
extern int RTS_SelectionType(void);

#define CMD_FILE "RAM:od-cmd"
#define MAX_TEXT 8192

static char s_text[MAX_TEXT];
static char *s_next;		/* next line to run, NULL = idle */
static int s_lines;
static int s_wait;		/* ticks before the next step */
static int s_poll;
static int s_mx = 160, s_my = 100;

/* the second half of a click / key / typed character */
static int s_pressButton;	/* 1 left, 2 right: press after the move */
static int s_releaseButton;
static bool s_holdButton;	/* ldown: no release */
static int s_releaseKey = -1;
static bool s_releaseShift;
static const char *s_typing;

static int Cmd_KeyCode(const char *name)
{
	static const char row1[] = "1234567890";
	static const char rowq[] = "qwertyuiop";
	static const char rowa[] = "asdfghjkl";
	static const char rowz[] = "zxcvbnm";
	static const struct { const char *n; int c; } names[] = {
		{"esc", 0x45}, {"space", 0x40}, {"return", 0x44}, {"enter", 0x43}, {"tab", 0x42},
		{"backspace", 0x41}, {"del", 0x46}, {"up", 0x4C}, {"down", 0x4D}, {"right", 0x4E},
		{"left", 0x4F}, {"lshift", 0x60}, {"ctrl", 0x63}, {"lalt", 0x64}
	};
	const char *p;
	unsigned i;

	if (name[0] == '0' && name[1] == 'x') return (int)strtol(name, NULL, 16);
	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (strcmp(name, names[i].n) == 0) return names[i].c;
	}
	if ((name[0] == 'f' || name[0] == 'F') && name[1] >= '1' && name[1] <= '9') {
		int n = atoi(name + 1);
		if (n >= 1 && n <= 10) return 0x50 + n - 1;
	}
	if (name[0] != '\0' && name[1] == '\0') {
		char c = name[0];
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
		if (c == ' ') return 0x40;
		if (c == '.') return 0x39;
		if (c == ',') return 0x38;
		if (c == '-') return 0x0B;
		if ((p = strchr(row1, c)) != NULL) return 0x01 + (int)(p - row1);
		if ((p = strchr(rowq, c)) != NULL) return 0x10 + (int)(p - rowq);
		if ((p = strchr(rowa, c)) != NULL) return 0x20 + (int)(p - rowa);
		if ((p = strchr(rowz, c)) != NULL) return 0x31 + (int)(p - rowz);
	}
	return -1;
}

static void Cmd_Shot(void)
{
	static uint8 row[SCREEN_WIDTH_MAX * 3];
	const uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_0);
	const uint8 *pal = Video_Amiga_Palette();
	char head[32];
	BPTR f;
	int x, y;

	f = Open((CONST_STRPTR)"RAM:od-shot.ppm", MODE_NEWFILE);
	if (f == 0) return;
	sprintf(head, "P6\n%d %d\n255\n", SCREEN_WIDTH, SCREEN_HEIGHT);
	Write(f, head, strlen(head));
	for (y = 0; y < SCREEN_HEIGHT; y++) {
		for (x = 0; x < SCREEN_WIDTH; x++) {
			const uint8 *c = pal + src[y * SCREEN_WIDTH + x] * 3;
			row[x * 3 + 0] = (uint8)((c[0] << 2) | (c[0] >> 4));
			row[x * 3 + 1] = (uint8)((c[1] << 2) | (c[1] >> 4));
			row[x * 3 + 2] = (uint8)((c[2] << 2) | (c[2] >> 4));
		}
		Write(f, row, SCREEN_WIDTH * 3);
	}
	Close(f);
}

static void Cmd_State(void)
{
	char buf[256];
	BPTR f = Open((CONST_STRPTR)"RAM:od-state.txt", MODE_NEWFILE);
	int n;
	if (f == 0) return;
	n = sprintf(buf, "gamemode %d\nhouse %d\ncampaign %u\nscenario %u\nmouse %u %u\nselectiontype %d\nunit %d\ngroup %d\ntimergame %lu\n",
	            (int)g_gameMode, (int)g_playerHouseID, (unsigned)g_campaignID, (unsigned)g_scenarioID,
	            (unsigned)g_mouseX, (unsigned)g_mouseY, RTS_SelectionType(),
	            RTS_SelectedUnit(), RTS_GroupCount(), (unsigned long)g_timerGame);
	Write(f, buf, n);
	Close(f);
}

static void Cmd_Done(void)
{
	char buf[32];
	BPTR f = Open((CONST_STRPTR)"RAM:od-done", MODE_NEWFILE);
	if (f == 0) return;
	Write(f, buf, sprintf(buf, "%d\n", s_lines));
	Close(f);
}

static void Cmd_Load(void)
{
	BPTR f = Open((CONST_STRPTR)CMD_FILE, MODE_OLDFILE);
	LONG n;
	if (f == 0) return;
	n = Read(f, s_text, MAX_TEXT - 1);
	Close(f);
	DeleteFile((CONST_STRPTR)CMD_FILE);
	if (n <= 0) return;
	s_text[n] = '\0';
	s_next = s_text;
	s_lines = 0;
}

/* Runs one line; returns the ticks to wait before the next step. */
static int Cmd_Run(char *line)
{
	char *arg = line;
	int x, y, k;

	while (*arg != '\0' && *arg != ' ') arg++;
	if (*arg == ' ') *arg++ = '\0';
	s_lines++;

	if (strcmp(line, "move") == 0 || strcmp(line, "click") == 0 || strcmp(line, "rclick") == 0) {
		/* by hand: libnix sscanf is not to be trusted */
		x = atoi(arg);
		while (*arg != '\0' && *arg != ' ') arg++;
		y = atoi(arg);
		s_mx = x;
		s_my = y;
		if (line[0] == 'm') {
			Video_Amiga_Mouse(x, y, false, false);
			return 2;
		}
		/* first the move, the press in a later tick (the game looks at
		 * where the pointer was when the button changed) */
		Video_Amiga_Mouse(x, y, false, false);
		s_pressButton = line[0] == 'r' ? 2 : 1;
		return 2;
	}
	if (strcmp(line, "ldown") == 0 || strcmp(line, "lmove") == 0) {
		/* a drag: press (ldown), move with the button held (lmove), lup */
		x = atoi(arg);
		while (*arg != '\0' && *arg != ' ') arg++;
		y = atoi(arg);
		s_mx = x;
		s_my = y;
		if (line[1] == 'd') {
			Video_Amiga_Mouse(x, y, false, false);
			s_pressButton = 1;
			s_holdButton = true;
			return 2;
		}
		Video_Amiga_Mouse(x, y, true, false);
		return 2;
	}
	if (strcmp(line, "lup") == 0) {
		Video_Amiga_Mouse(s_mx, s_my, false, false);
		return 3;
	}
	if (strcmp(line, "key") == 0) {
		k = Cmd_KeyCode(arg);
		if (k < 0) return 0;
		Video_Amiga_Key((uint16)k);
		s_releaseKey = k;
		return 3;
	}
	if (strcmp(line, "type") == 0) {
		s_typing = arg;
		return 0;
	}
	if (strcmp(line, "wait") == 0) return atoi(arg);
	if (strcmp(line, "spawn") == 0) {
		/* spawn TYPE HOUSE X Y DX DY: a unit sent across the viewport */
		int v[6], k;
		for (k = 0; k < 6; k++) {
			v[k] = atoi(arg);
			while (*arg != '\0' && *arg != ' ') arg++;
			while (*arg == ' ') arg++;
		}
		RTS_TestSpawn((uint8)v[0], (uint8)v[1], v[2], v[3], v[4], v[5]);
		return 2;
	}
	if (strcmp(line, "redraw") == 0) {
		g_viewport_forceRedraw = true;	/* the whole map drawn again (ghost check) */
		return 0;
	}
	if (strcmp(line, "shot") == 0) Cmd_Shot();
	if (strcmp(line, "state") == 0) Cmd_State();
	return 0;
}

/* Called from Video_Tick() once per tick, before the screen is locked. */
void AmigaCmd_Tick(void)
{
	if (s_wait > 0) {
		s_wait--;
		return;
	}
	if (s_pressButton != 0) {
		Video_Amiga_Mouse(s_mx, s_my, s_pressButton == 1, s_pressButton == 2);
		s_releaseButton = s_holdButton ? 0 : s_pressButton;
		s_holdButton = false;
		s_pressButton = 0;
		s_wait = 6;
		return;
	}
	if (s_releaseButton != 0) {
		Video_Amiga_Mouse(s_mx, s_my, false, false);
		s_releaseButton = 0;
		s_wait = 3;
		return;
	}
	if (s_releaseKey >= 0) {
		Video_Amiga_Key((uint16)(s_releaseKey | 0x80));
		s_releaseKey = -1;
		if (s_releaseShift) {
			Video_Amiga_Key(0x60 | 0x80);
			s_releaseShift = false;
		}
		s_wait = 2;
		return;
	}
	if (s_typing != NULL) {
		char c[2];
		int k;
		if (*s_typing == '\0') {
			s_typing = NULL;
			return;
		}
		c[0] = *s_typing++;
		c[1] = '\0';
		k = Cmd_KeyCode(c);
		if (k >= 0) {
			if (c[0] >= 'A' && c[0] <= 'Z') {
				Video_Amiga_Key(0x60);
				s_releaseShift = true;
			}
			Video_Amiga_Key((uint16)k);
			s_releaseKey = k;
			s_wait = 2;
		}
		return;
	}
	if (s_next == NULL) {
		if (++s_poll < 15) return;
		s_poll = 0;
		Cmd_Load();
		return;
	}
	while (*s_next == '\n' || *s_next == '\r' || *s_next == ' ') s_next++;
	if (*s_next == '\0') {
		s_next = NULL;
		Cmd_Done();
		return;
	}
	{
		char *line = s_next;
		char *end = strchr(line, '\n');
		if (end != NULL) {
			*end = '\0';
			s_next = end + 1;
			if (end > line && end[-1] == '\r') end[-1] = '\0';
		} else {
			s_next = line + strlen(line);
		}
		if (line[0] != '#' && line[0] != ';') s_wait = Cmd_Run(line);
	}
}
