/** @file src/gui/report_amiga.c The window of a bug report (AmigaOS).
 *
 * As in the Settlers port: after F10 has written its files (bugreport_amiga.c)
 * and when a TCP/IP stack runs, a window on the map screen asks for an e-mail
 * address (not needed, kept in REPORT.CFG of the save directory) and for what
 * happened (up to 1000 letters); SEND sends both with the description of the
 * computer and the files to the game server (os/report_amiga.c; ini
 * lan_relay=), CANCEL leaves the files where they are. Tab changes the field,
 * Return in the e-mail field sends, Esc closes.
 *
 * The same window sends the states of a LAN game that went out of step (kind
 * "LAN"). A LAN game goes on under the window (g_lanModal).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "../os/math.h"
#include "../os/sleep.h"
#include "../os/report_amiga.h"
#include "../os/sysinfo_amiga.h"

#include "gui.h"
#include "font.h"
#include "../file.h"
#include "../gfx.h"
#include "../house.h"
#include "../inifile.h"
#include "../input/input.h"
#include "../input/mouse.h"
#include "../opendune.h"
#include "../rev.h"
#include "../timer.h"
#include "../lang_amiga.h"

extern uint32 AmigaTimer_GetMs(void);
extern volatile bool g_amigaEditBox;	/* video_amiga.c: the keys are typed, not commands */
extern void File_PersonalPath(char *buf, size_t len, const char *filename);

#define WIN_W    296
#define WIN_H    150
#define TEXT_MAX 1000
#define LINES    6
#define LINE_PX  (WIN_W - 28)
#define CFG_FILE "REPORT.CFG"
#define RELAY_DEFAULT "83.12.152.135:6810"

#define COL_TITLE 238
#define COL_TEXT  232
#define COL_HOVER 231
#define COL_DIM   233

enum { FIELD_EMAIL, FIELD_TEXT };
enum { ST_EDIT, ST_SENDING, ST_SENT, ST_FAILED };

static bool s_wanted;
static uint32 s_wantedAt;
static char s_kind[8];
static char s_names[REPORT_MAX_FILES][24];
static int s_nameCount;

static char s_email[80];
static char s_text[TEXT_MAX + 1];
static char s_status[64];
static int s_field, s_state;
static int16 s_x, s_y;

/** Asks for the window (video driver's tick, lan_amiga.c): shown by Report_Poll. */
void Report_Open(const char *kind, const char *const *names, int count)
{
	int i;
	if (s_wanted || count <= 0) return;
	strncpy(s_kind, kind, sizeof(s_kind) - 1);
	s_nameCount = min(count, REPORT_MAX_FILES);
	for (i = 0; i < s_nameCount; i++) {
		strncpy(s_names[i], names[i], sizeof(s_names[i]) - 1);
		s_names[i][sizeof(s_names[i]) - 1] = '\0';
	}
	s_wantedAt = AmigaTimer_GetMs();
	s_wanted = true;
}

static bool Report_Over(int x, int y, int w, int h)
{
	return g_mouseX >= s_x + x && g_mouseX < s_x + x + w && g_mouseY >= s_y + y && g_mouseY < s_y + y + h;
}

static void Report_Button(int x, const char *text, bool enabled)
{
	bool hover = enabled && Report_Over(x, 130, 100, 13);
	GUI_DrawBorder(s_x + x, s_y + 130, 100, 13, 2, true);
	GUI_DrawText_Wrapper(text, s_x + x + 50, s_y + 132, enabled ? (hover ? COL_HOVER : COL_TEXT) : COL_DIM, 0, 0x122);
}

/* The words in lines of LINE_PX pixels, broken between words; the last LINES
 * lines are shown (the end is where the player types). */
static void Report_DrawText(void)
{
	const char *start[80];
	int length[80];
	int count = 0, pos = 0, len = (int)strlen(s_text), i, first;

	while (pos < len && count < 80) {
		int w = 0, n = 0, space = -1;
		while (pos + n < len) {
			w += Font_GetCharWidth((unsigned char)s_text[pos + n]);
			if (w > LINE_PX) break;
			if (s_text[pos + n] == ' ') space = n;
			n++;
		}
		if (pos + n < len && space > 0) n = space + 1;
		if (n == 0) n = 1;
		start[count] = s_text + pos;
		length[count] = n;
		count++;
		pos += n;
	}
	first = max(0, count - LINES);
	for (i = first; i < count; i++) {
		char line[80];
		int n = min(length[i], (int)sizeof(line) - 2);
		memcpy(line, start[i], n);
		line[n] = '\0';
		if (i == count - 1 && s_field == FIELD_TEXT && s_state == ST_EDIT) strcat(line, "_");
		GUI_DrawText_Wrapper(line, s_x + 14, s_y + 54 + (i - first) * 9, COL_TEXT, 0, 0x22);
	}
	if (count == 0 && s_field == FIELD_TEXT && s_state == ST_EDIT) GUI_DrawText_Wrapper("_", s_x + 14, s_y + 54, COL_TEXT, 0, 0x22);
}

static void Report_Draw(void)
{
	char line[64];
	const char *shown = s_email;
	bool edit = s_state == ST_EDIT || s_state == ST_FAILED;

	GUI_Mouse_Hide_Safe();
	GUI_DrawBorder(s_x, s_y, WIN_W, WIN_H, 1, true);
	GUI_DrawText_Wrapper(NULL, 0, 0, 0, 0, 0x22);
	GUI_DrawText_Wrapper(strcmp(s_kind, "LAN") == 0 ? Lang_T("The game went out of step - send the states?") : Lang_T("Bug report"),
	                     s_x + WIN_W / 2, s_y + 8, COL_TITLE, 0, 0x122);

	GUI_DrawText_Wrapper(Lang_T("Your e-mail"), s_x + 10, s_y + 24, COL_TEXT, 0, 0x22);
	GUI_DrawBorder(s_x + 84, s_y + 22, 202, 11, 0, true);
	/* the end of a long address */
	while (Font_GetStringWidth(shown) > 190 && *shown != '\0') shown++;
	snprintf(line, sizeof(line), "%.60s%s", shown, s_field == FIELD_EMAIL && s_state == ST_EDIT ? "_" : "");
	GUI_DrawText_Wrapper(line, s_x + 87, s_y + 24, s_field == FIELD_EMAIL ? COL_HOVER : COL_TEXT, 0, 0x22);

	snprintf(line, sizeof(line), "%s  %d / %d", Lang_T("What happened"), (int)strlen(s_text), TEXT_MAX);
	GUI_DrawText_Wrapper(line, s_x + 10, s_y + 40, COL_TEXT, 0, 0x22);
	GUI_DrawBorder(s_x + 10, s_y + 50, WIN_W - 20, LINES * 9 + 8, 0, true);
	Report_DrawText();

	GUI_DrawText_Wrapper(s_status, s_x + WIN_W / 2, s_y + 117, COL_HOVER, 0, 0x122);
	Report_Button(40, Lang_T("SEND"), edit);
	Report_Button(156, s_state == ST_SENT ? Lang_T("CLOSE") : Lang_T("CANCEL"), s_state != ST_SENDING);
	GUI_Mouse_Show_Safe();
}

static void Report_Status(const char *text)
{
	strncpy(s_status, text, sizeof(s_status) - 1);
	s_status[sizeof(s_status) - 1] = '\0';
}

static void Report_LoadEmail(void)
{
	uint8 fd;
	uint32 n;
	if (s_email[0] != '\0' || !File_Exists_Personal(CFG_FILE)) return;
	fd = File_Open_Personal(CFG_FILE, FILE_MODE_READ);
	if (fd == 0xFF) return;
	n = File_Read(fd, s_email, sizeof(s_email) - 1);
	File_Close(fd);
	s_email[n] = '\0';
	while (n > 0 && (uint8)s_email[n - 1] <= ' ') s_email[--n] = '\0';
}

static void Report_SaveEmail(void)
{
	uint8 fd = File_Open_Personal(CFG_FILE, FILE_MODE_WRITE);
	if (fd == 0xFF) return;
	File_Write(fd, s_email, (uint32)strlen(s_email));
	File_Close(fd);
}

/* the computer, in a line (up to 120 letters on the server) */
static void Report_Machine(char *text, size_t size)
{
	AmigaInfo info;
	char clock[16];
	amiga_info_read(&info);
	clock[0] = '\0';
	if (info.warp_mhz > 0) snprintf(clock, sizeof(clock), " %d MHz", info.warp_mhz);
	else if (info.measured_mhz > 0) snprintf(clock, sizeof(clock), " ~%d MHz", info.measured_mhz);
	snprintf(text, size, "%s%s, %s, Kick %d.%d, Fast %lu MB, %s, %dx%d", info.cpu, clock,
	         info.warp ? info.warp_model : "no Warp", info.kick_version, info.kick_revision,
	         info.fast_kb / 1024, info.board[0] != '\0' ? info.board : "no RTG", SCREEN_WIDTH, SCREEN_HEIGHT);
}

static void Report_Start(void)
{
	char host[64], version[24], machine[128];
	char paths[REPORT_MAX_FILES][108];
	const char *files[REPORT_MAX_FILES];
	char *colon;
	int port = 6810, i;

	if (IniFile_GetString("lan_relay", RELAY_DEFAULT, host, sizeof(host)) == NULL || host[0] == '\0') strcpy(host, RELAY_DEFAULT);
	colon = strrchr(host, ':');
	if (colon != NULL) {
		int p = atoi(colon + 1);
		*colon = '\0';
		if (p > 0 && p < 65536) port = p;
	}
	snprintf(version, sizeof(version), "OpenDUNE-%s", g_opendune_revision);
	Report_Machine(machine, sizeof(machine));
	for (i = 0; i < s_nameCount; i++) {
		File_PersonalPath(paths[i], sizeof(paths[i]), s_names[i]);
		files[i] = paths[i];
	}
	Report_SaveEmail();
	if (Report_Send(host, port, version, s_kind, s_email, s_text, machine, files, s_nameCount)) {
		s_state = ST_SENDING;
		Report_Status(Lang_T("Sending the report..."));
	} else {
		s_state = ST_FAILED;
		Report_Status(Lang_T("The report cannot be sent now."));
	}
}

static bool Report_EmailChar(uint8 c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
	       c == '@' || c == '.' || c == '_' || c == '-' || c == '+';
}

/* a key (the game's key number with the shift bit); true: close */
static bool Report_Key(uint16 key)
{
	char *text = s_field == FIELD_EMAIL ? s_email : s_text;
	int size = s_field == FIELD_EMAIL ? (int)sizeof(s_email) : (int)sizeof(s_text);
	int len = (int)strlen(text);
	uint8 c;

	if ((key & 0xFF) == 0x6E) return s_state != ST_SENDING;	/* Esc */
	if (s_state == ST_SENT) return (key & 0xFF) == 0x2B;
	if (s_state != ST_EDIT && s_state != ST_FAILED) return false;
	s_state = ST_EDIT;
	if ((key & 0xFF) == 0x0F) {	/* backspace */
		if (len > 0) text[len - 1] = '\0';
		return false;
	}
	if ((key & 0xFF) == 0x10) {	/* Tab */
		s_field = s_field == FIELD_EMAIL ? FIELD_TEXT : FIELD_EMAIL;
		return false;
	}
	if ((key & 0xFF) == 0x2B) {	/* Return */
		if (s_field == FIELD_EMAIL) Report_Start();
		return false;
	}
	c = (uint8)(Input_Keyboard_HandleKeys(key) & 0xFF);
	if (c < ' ' || c > '~' || len >= size - 1) return false;
	if (s_field == FIELD_EMAIL && !Report_EmailChar(c)) return false;
	if (Font_GetCharWidth(c) == 0) return false;	/* not in the game's font */
	text[len] = (char)c;
	text[len + 1] = '\0';
	return false;
}

static void Report_Window(void)
{
	Screen old = GFX_Screen_SetActive(SCREEN_0);
	uint8 *backup = malloc(WIN_W * WIN_H);
	uint16 lastX = 0xFFFF, lastY = 0xFFFF;
	bool done = false, dirty = true, timer;

	s_x =(int16)((SCREEN_WIDTH - WIN_W) / 2);
	s_y = (int16)((SCREEN_HEIGHT - WIN_H) / 2);
	s_field = s_email[0] == '\0' ? FIELD_EMAIL : FIELD_TEXT;
	s_state = ST_EDIT;
	s_text[0] = '\0';
	Report_LoadEmail();
	if (s_email[0] != '\0') s_field = FIELD_TEXT;
	Report_Status("");

	GUI_Mouse_Hide_Safe();
	if (backup != NULL) GFX_CopyToBuffer(s_x, s_y, WIN_W, WIN_H, backup);
	GUI_Mouse_Show_Safe();
	Input_History_Clear();
	g_lanModal++;
	g_amigaEditBox = true;
	/* the game waits (a LAN game goes on: its steps do not come from this timer) */
	timer = Timer_SetTimer(TIMER_GAME, false);

	while (!done) {
		uint16 key = 0;
		char text[64];
		int ok;

		if (s_state == ST_SENDING && Report_Result(text, sizeof(text), &ok)) {
			if (ok) {
				char line[64];
				snprintf(line, sizeof(line), Lang_T("Report no. %s sent. Thank you!"), text);
				Report_Status(line);
				s_state = ST_SENT;
			} else {
				Report_Status(Lang_T(text));	/* os/report_amiga.c's and the server's: INDIRECT */
				s_state = ST_FAILED;
			}
			dirty = true;
		}
		if (Input_IsInputAvailable() != 0) key = Input_Wait() & 0x9FF;
		if ((key & 0xFF) == 0x41 || (key & 0xFF) == 0x42) {
			if ((key & 0x800) == 0) {
				int x = g_mouseClickX - s_x, y = g_mouseClickY - s_y;
				if (y >= 130 && y < 143 && x >= 40 && x < 140 && (s_state == ST_EDIT || s_state == ST_FAILED)) Report_Start();
				else if (y >= 130 && y < 143 && x >= 156 && x < 256 && s_state != ST_SENDING) done = true;
				else if (y >= 22 && y < 33) s_field = FIELD_EMAIL;
				else if (y >= 50 && y < 112) s_field = FIELD_TEXT;
				dirty = true;
			}
		} else if (key != 0 && (key & 0x800) == 0) {
			if (Report_Key(key)) done = true;
			dirty = true;
		}
		if (g_mouseX != lastX || g_mouseY != lastY) {
			lastX = g_mouseX;
			lastY = g_mouseY;
			dirty = true;
		}
		if (dirty && !done) {
			Report_Draw();
			dirty = false;
		}
		GUI_PaletteAnimate();
		sleepIdle();
	}

	Timer_SetTimer(TIMER_GAME, timer);
	g_amigaEditBox = false;
	g_lanModal--;
	GUI_Mouse_Hide_Safe();
	if (backup != NULL) GFX_CopyFromBuffer(s_x, s_y, WIN_W, WIN_H, backup);
	GUI_Mouse_Show_Safe();
	free(backup);
	Input_History_Clear();
	GFX_Screen_SetActive(old);
}

/** In the game's loop on the map screen: the window that was asked for (not
 * one asked for long ago, on another screen). */
void Report_Poll(void)
{
	if (!s_wanted) return;
	s_wanted = false;
	/* (a game out of step: also after a window that was open then) */
	if (AmigaTimer_GetMs() - s_wantedAt > (strcmp(s_kind, "LAN") == 0 ? 60000UL : 3000UL) || g_gameMode != GM_NORMAL) return;
	if (SCREEN_WIDTH < WIN_W || SCREEN_HEIGHT < WIN_H) return;
	Report_Window();
}
