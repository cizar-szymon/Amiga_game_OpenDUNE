/** @file src/lang_amiga.c The languages of the Amiga port. */

/*
 * The game's texts come from the language files (string.c): the player's
 * original data (ENGLISH, the German .GER files) or the port's Polish
 * translation (PROGDIR:lang/POLISH.PAK). The port's own texts (LAN menus,
 * Amiga info, Resolution ...) are translated here, in lang_strings.h
 * (made by tools/lang/make_port_strings.py from tools/lang/port_strings.txt).
 *
 * The language is chosen in Options - Game controls and kept in
 * LANGUAGE.CFG of the save directory; it changes in the main menu (all texts
 * and fonts loaded again, nothing of a running game points into the old
 * ones there). Without LANGUAGE.CFG the ini's language= decides.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "os/common.h"
#include "os/endian.h"
#include "os/math.h"
#include "os/strings.h"

#include "lang_amiga.h"

#include "config.h"
#include "file.h"
#include "string.h"
#include "gfx.h"
#include "sprites.h"
#include "gui/font.h"
#include "gui/gui.h"
#include "table/strings.h"

/* English, German, Polish: German in the code page of the game's fonts
 * (CP437: 0x84 a umlaut ...), Polish ISO-8859-2 (gui/font_polish.c) */
#include "lang_strings.h"

#define LANGUAGE_FILE "LANGUAGE.CFG"

/* the names in the ini (language=) and in LANGUAGE.CFG */
static const char * const s_iniNames[LANGUAGE_MAX] = { "ENGLISH", "FRENCH", "GERMAN", "ITALIAN", "SPANISH", "POLISH" };
/* the names in Game controls, each in its own language */
static const char * const s_names[LANGUAGE_MAX] = { "English", "Fran\207ais", "Deutsch", "Italiano", "Espa\244ol", "Polski" };

static uint8 s_chosen = LANGUAGE_ENGLISH;

/* The last translations looked up, by the address of the English text (the
 * texts are constants, most are drawn again and again). */
#define LANG_CACHE 64
static const char *s_cacheKey[LANG_CACHE];
static int16 s_cacheIndex[LANG_CACHE];
static uint8 s_cacheLanguage = 0xFF;

/**
 * A text of the port in the language of the game.
 *
 * @param english The English text.
 * @return The translation; the English text if there is none.
 */
const char *Lang_T(const char *english)
{
	unsigned int slot;
	int column;
	int16 i;

	if (english == NULL) return NULL;
	if (g_config.language == LANGUAGE_GERMAN) column = 1;
	else if (g_config.language == LANGUAGE_POLISH) column = 2;
	else return english;

	if (s_cacheLanguage != g_config.language) {
		memset((void *)s_cacheKey, 0, sizeof(s_cacheKey));
		s_cacheLanguage = g_config.language;
	}
	slot = ((unsigned long)english >> 2) % LANG_CACHE;
	if (s_cacheKey[slot] != english) {
		s_cacheKey[slot] = english;
		s_cacheIndex[slot] = -1;
		for (i = 0; i < (int16)(sizeof(s_langStrings) / sizeof(s_langStrings[0])); i++) {
			if (strcmp(s_langStrings[i][0], english) == 0) {
				s_cacheIndex[slot] = i;
				break;
			}
		}
	}
	i = s_cacheIndex[slot];
	if (i < 0 || s_langStrings[i][column] == NULL) return english;
	return s_langStrings[i][column];
}

/** The name of a language in that language. */
const char *Lang_Name(uint8 language)
{
	return language < LANGUAGE_MAX ? s_names[language] : "?";
}

/** The language chosen in Game controls (the game's one after the main menu). */
uint8 Lang_Chosen(void)
{
	return s_chosen;
}

/** The next language after this one whose texts are there (English always). */
uint8 Lang_Next(uint8 language)
{
	uint8 i;

	for (i = 1; i <= LANGUAGE_MAX; i++) {
		uint8 next = (uint8)((language + i) % LANGUAGE_MAX);
		if (String_LanguageAvailable(next)) return next;
	}
	return LANGUAGE_ENGLISH;
}

/** Choose a language (Game controls): kept in LANGUAGE.CFG, used from the main menu on. */
void Lang_Choose(uint8 language)
{
	char text[16];
	uint8 fd;
	int n;

	if (language >= LANGUAGE_MAX) return;
	s_chosen = language;
	n = snprintf(text, sizeof(text), "%s\n", s_iniNames[language]);
	fd = File_Open_Personal(LANGUAGE_FILE, FILE_MODE_WRITE);
	if (fd != FILE_INVALID) {
		File_Write(fd, text, n);
		File_Close(fd);
	}
}

/**
 * The language at the start, after dune.cfg and the ini's language=
 * (opendune.c): LANGUAGE.CFG if a language was chosen in Game controls; one
 * whose texts are not there becomes English.
 */
void Lang_Init(void)
{
	char text[16];
	uint8 fd;
	uint32 n;
	uint8 i;

	if (File_Exists_Personal(LANGUAGE_FILE)) {
		fd = File_Open_Personal(LANGUAGE_FILE, FILE_MODE_READ);
		if (fd != FILE_INVALID) {
			n = File_Read(fd, text, sizeof(text) - 1);
			File_Close(fd);
			text[n] = '\0';
			for (n = 0; text[n] != '\0'; n++) {
				if (text[n] == '\n' || text[n] == '\r' || text[n] == ' ') text[n] = '\0';
			}
			for (i = 0; i < LANGUAGE_MAX; i++) {
				if (strcasecmp(text, s_iniNames[i]) == 0) g_config.language = i;
			}
		}
	}
	if (!String_LanguageAvailable(g_config.language)) g_config.language = LANGUAGE_ENGLISH;
	s_chosen = g_config.language;
}

/* ---- the lettering of the buttons ----
 *
 * The buttons with words (MENTAT, OPTIONS, Credits; the starport's Upgrade,
 * Send Order ...; the mentat's Yes, No, Exit, Proceed, Repeat) are pictures
 * of BTTN, CHOAM and MENTAT. A language without its own pictures (Polish,
 * the German of the Amiga version: its pictures are planar) gets the
 * English ones with the word painted over in the button's colour and the
 * game's text written in its place in the 8 (or 6) pixel font. The sprites
 * are rebuilt in place, so the widgets that keep their address see the new
 * words after a language change in the main menu too. */

typedef struct LangButton {
	uint16 sprite;
	uint16 stringID;		/* the game's text; STR_NULL: the port's text */
	uint8 word;			/* the port's text: Lang_ButtonWord() */
	uint8 kind;			/* 0 a gold button, 1 the mentat's, 2 "Credits" */
	uint8 fg, shade;		/* the colours of the lettering */
} LangButton;

static const LangButton s_buttons[] = {
	{   7, STR_NULL, 1, 0, 0x93, 0xE5 },	/* (each second one: pressed) */
	{   8, STR_NULL, 1, 0, 0x91, 0xE5 },
	{   9, STR_NULL, 2, 0, 0x93, 0xE5 },
	{  10, STR_NULL, 2, 0, 0x91, 0xE5 },
	{  11, STR_NULL, 3, 2, 0x92, 0xE5 },
	{ 359, STR_UPGRADE,     0, 0, 0x93, 0xE5 },
	{ 360, STR_UPGRADE,     0, 0, 0x91, 0xE5 },
	{ 361, STR_SEND_ORDER,  0, 0, 0x93, 0xE5 },
	{ 362, STR_SEND_ORDER,  0, 0, 0x91, 0xE5 },
	{ 363, STR_BUILD_THIS,  0, 0, 0x93, 0xE5 },
	{ 364, STR_BUILD_THIS,  0, 0, 0x91, 0xE5 },
	{ 365, STR_RESUME_GAME, 0, 0, 0x93, 0xE5 },
	{ 366, STR_RESUME_GAME, 0, 0, 0x91, 0xE5 },
	{ 371, STR_INVOICE,     0, 0, 0x93, 0xE5 },
	{ 372, STR_INVOICE,     0, 0, 0x91, 0xE5 },
	{ 373, STR_YES,     0, 1, 0x20, 0x86 },
	{ 374, STR_YES,     0, 1, 0x2A, 0x0C },
	{ 375, STR_NO,      0, 1, 0x20, 0x86 },
	{ 376, STR_NO,      0, 1, 0x2A, 0x0C },
	{ 377, STR_EXIT,    0, 1, 0x20, 0x86 },
	{ 378, STR_EXIT,    0, 1, 0x2A, 0x0C },
	{ 379, STR_PROCEED, 0, 1, 0x20, 0x86 },
	{ 380, STR_PROCEED, 0, 1, 0x2A, 0x0C },
	{ 381, STR_REPEAT,  0, 1, 0x20, 0x86 },
	{ 382, STR_REPEAT,  0, 1, 0x2A, 0x0C },
};

#define BUTTON_MAX_W 128
#define BUTTON_MAX_H 24

static uint8 *s_buttonOrig[lengthof(s_buttons)];	/* the sprites as loaded */
static uint8 s_pictureLanguage = LANGUAGE_ENGLISH;	/* the language of the loaded pictures */
static uint8 s_buttonsLanguage = 0xFF;			/* the language the buttons show */

/** Width and height of a sprite (the header as GUI_DrawSprite reads it). */
static void Lang_SpriteSize(const uint8 *sprite, uint16 *w, uint16 *h)
{
	*h = sprite[2];
	*w = READ_LE_UINT16(sprite + 3);
}

/** The bytes a sprite takes in memory (Sprites_Load: the size field). */
static uint16 Lang_SpriteBytes(const uint8 *sprite)
{
	return READ_LE_UINT16(sprite + 6);
}

/** The room for a rebuilt sprite: the header, two bytes per pixel at most. */
static uint16 Lang_SpriteRoom(const uint8 *sprite)
{
	uint16 w, h, room;

	Lang_SpriteSize(sprite, &w, &h);
	room = (uint16)(10 + 2 * w * h);
	return max(room, Lang_SpriteBytes(sprite));
}

/**
 * The pixels of the screen (w x h at 0,0) as an uncompressed sprite with
 * zero runs per row; the header of the original, without its palette.
 */
static void Lang_EncodeSprite(uint8 *dst, const uint8 *orig, const uint8 *src, uint16 w, uint16 h)
{
	uint8 *p = dst + 10;
	uint16 x, y;

	for (y = 0; y < h; y++) {
		const uint8 *row = src + y * SCREEN_WIDTH;
		for (x = 0; x < w; ) {
			if (row[x] == 0) {
				uint16 n = 0;
				while (x < w && row[x] == 0 && n < 255) { x++; n++; }
				*p++ = 0;
				*p++ = (uint8)n;
			} else {
				*p++ = row[x++];
			}
		}
	}
	memcpy(dst, orig, 10);
	dst[0] = (uint8)((orig[0] & ~0x1) | 0x2);	/* no palette, not Format80 */
	WRITE_LE_UINT16(dst + 6, (uint16)(p - dst));
	WRITE_LE_UINT16(dst + 8, (uint16)(w * h));
}

/** The port's words of the buttons (not in the game's texts). */
static const char *Lang_ButtonWord(uint8 word)
{
	switch (word) {
		case 1:  return Lang_T("Mentat");
		case 2:  return Lang_T("Options");
		case 3:  return Lang_T("Credits");
		default: return NULL;
	}
}

/**
 * Capitals, as the gold buttons' lettering: a-z, the Polish letters
 * (ISO-8859-2) and the German umlauts (CP437, the game's fonts).
 */
static void Lang_Upper(char *dst, const char *src, size_t size)
{
	static const uint8 polish[][2] = {
		{ 0xB1, 0xA1 }, { 0xE6, 0xC6 }, { 0xEA, 0xCA }, { 0xB3, 0xA3 }, { 0xF1, 0xD1 },
		{ 0xF3, 0xD3 }, { 0xB6, 0xA6 }, { 0xBC, 0xAC }, { 0xBF, 0xAF }
	};
	static const uint8 german[][2] = { { 0x84, 0x8E }, { 0x94, 0x99 }, { 0x81, 0x9A } };
	size_t n;
	unsigned i;

	for (n = 0; src[n] != '\0' && n + 1 < size; n++) {
		uint8 c = (uint8)src[n];
		if (c >= 'a' && c <= 'z') c = (uint8)(c - 'a' + 'A');
		else if (g_config.language == LANGUAGE_POLISH) {
			for (i = 0; i < lengthof(polish); i++) if (c == polish[i][0]) c = polish[i][1];
		} else if (g_config.language == LANGUAGE_GERMAN) {
			for (i = 0; i < lengthof(german); i++) if (c == german[i][0]) c = german[i][1];
		}
		dst[n] = (char)c;
	}
	dst[n] = '\0';
}

/** One button: the original with its word painted over and the text written on it. */
static void Lang_LetterButton(const LangButton *b, const uint8 *orig, uint8 *dst)
{
	static uint8 save[BUTTON_MAX_H][BUTTON_MAX_W];
	uint8 *scr = GFX_Screen_Get_ByIndex(SCREEN_3);	/* credits and intro only */
	const char *word = (b->stringID != STR_NULL) ? String_Get_ByIndex(b->stringID) : Lang_ButtonWord(b->word);
	char text[40];
	Font *oldFont = g_fontCurrent;
	int8 oldOffset = g_fontCharOffset;
	Screen oldScreen;
	uint16 w, h, x, y;
	int left, right, top, bottom, mid, font, style;
	uint8 bg, frame;

	Lang_SpriteSize(orig, &w, &h);
	if (w > BUTTON_MAX_W || h > BUTTON_MAX_H || word == NULL) {
		memcpy(dst, orig, Lang_SpriteBytes(orig));
		return;
	}
	for (y = 0; y < h; y++) {
		memcpy(save[y], scr + y * SCREEN_WIDTH, w);
		memset(scr + y * SCREEN_WIDTH, 0, w);
	}
	if (b->kind == 0) {
		Lang_Upper(text, word, sizeof(text));
	} else {
		strncpy(text, word, sizeof(text) - 1);
		text[sizeof(text) - 1] = '\0';
	}
	GUI_DrawSprite(SCREEN_3, orig, 0, 0, 0, 0);

	/* the inside: from the frame's colour at the left of the middle row to
	 * its last pixel of that colour (BTTN has some top bar on its right) */
	mid = h / 2;
	for (left = 0; left < w - 1 && scr[mid * SCREEN_WIDTH + left] == 0; left++) {}
	frame = scr[mid * SCREEN_WIDTH + left];
	for (right = w - 1; right > left && scr[mid * SCREEN_WIDTH + right] != frame; right--) {}
	top = 2;
	bottom = (b->kind == 1) ? h - 3 : h - 4;
	left += 2;
	right -= 2;
	bg = scr[top * SCREEN_WIDTH + left];

	if (b->kind == 2) {
		/* "Credits" on the top bar's pattern: the letters' colours replaced
		 * by the pattern of the left end, where there are none */
		left = 0;
		right = w - 1;
		for (y = 0; y < h; y++) {
			uint8 *row = scr + y * SCREEN_WIDTH;
			for (x = 9; x < w; x++) {
				if (row[x] == 0xE2 || row[x] == 0x92 || row[x] == 0x6E) row[x] = row[x % 9];
			}
		}
	} else {
		for (y = top; y <= bottom; y++) memset(scr + y * SCREEN_WIDTH + left, bg, right - left + 1);
	}

	/* the text: the 8 pixel font, the 6 pixel one if it is too wide; the
	 * mentat's with a shadow of their colour, the gold ones with the font's
	 * own dark shadow (as the game's windows) */
	oldScreen = GFX_Screen_SetActive(SCREEN_3);
	style = (b->kind == 1) ? 0x10 : 0x20;
	font = 2;
	GUI_DrawText_Wrapper(NULL, 0, 0, b->fg, 0, style | font);
	if (Font_GetStringWidth(text) > right - left - 1) {
		font = 1;
		GUI_DrawText_Wrapper(NULL, 0, 0, b->fg, 0, style | font);
	}
	y = (b->kind == 1) ? (h - g_fontCurrent->height) / 2 : 3;
	x = (left + right + 1) / 2;
	if (b->kind == 1) GUI_DrawText_Wrapper(text, x + 1, y + 1, b->shade, 0, 0x100 | style | font);
	GUI_DrawText_Wrapper(text, x, y, b->fg, 0, 0x100 | style | font);
	GFX_Screen_SetActive(oldScreen);
	g_fontCharOffset = oldOffset;
	if (oldFont != NULL) Font_Select(oldFont);

	Lang_EncodeSprite(dst, orig, scr, w, h);
	for (y = 0; y < h; y++) memcpy(scr + y * SCREEN_WIDTH, save[y], w);
}

/** True if the pictures with words (BTTN, MENTAT, MISC ...) are of the game's language. */
bool Lang_OwnPictures(void)
{
	return g_config.language == s_pictureLanguage;
}

/** A button's sprite as loaded (layout_amiga.c takes the letters and colours of MENTAT and OPTIONS). */
const uint8 *Lang_OriginalSprite(uint16 sprite)
{
	unsigned i;

	for (i = 0; i < lengthof(s_buttons); i++) {
		if (s_buttons[i].sprite == sprite && s_buttonOrig[i] != NULL) return s_buttonOrig[i];
	}
	return g_sprites[sprite];
}

/**
 * The buttons in the language of the game (after Sprites_Init and after a
 * language change): the loaded pictures if they are of that language, else
 * the English ones lettered anew.
 */
void Lang_LetterButtons(void)
{
	unsigned i;

	if (s_buttonOrig[0] == NULL) {
		/* the first time: the loaded sprites kept, room for the rebuilt ones */
		const char *name = String_GenerateGfxFilename("BTTN", false);
		const char *dot = strrchr(name, '.');

		s_pictureLanguage = LANGUAGE_ENGLISH;
		for (i = 0; dot != NULL && i < LANGUAGE_MAX; i++) {
			if (strcasecmp(dot + 1, g_languageSuffixes[i]) == 0) s_pictureLanguage = (uint8)i;
		}
		for (i = 0; i < lengthof(s_buttons); i++) {
			if (g_sprites[s_buttons[i].sprite] == NULL) return;
		}
		for (i = 0; i < lengthof(s_buttons); i++) {
			uint8 *orig = g_sprites[s_buttons[i].sprite];
			uint8 *room = (uint8 *)malloc(Lang_SpriteRoom(orig));

			if (room == NULL) return;
			memcpy(room, orig, Lang_SpriteBytes(orig));
			s_buttonOrig[i] = orig;
			g_sprites[s_buttons[i].sprite] = room;
		}
		s_buttonsLanguage = s_pictureLanguage;
	}
	if (s_buttonOrig[lengthof(s_buttons) - 1] == NULL) return;	/* (no memory) */
	if (s_buttonsLanguage == g_config.language) return;

	for (i = 0; i < lengthof(s_buttons); i++) {
		const uint8 *orig = s_buttonOrig[i];
		uint8 *dst = g_sprites[s_buttons[i].sprite];

		if (g_config.language == s_pictureLanguage) {
			memcpy(dst, orig, Lang_SpriteBytes(orig));
		} else {
			Lang_LetterButton(&s_buttons[i], orig, dst);
		}
	}
	s_buttonsLanguage = g_config.language;
}

/**
 * The language chosen in Game controls becomes the game's: the texts and the
 * fonts loaded again (the main menu, before it is drawn).
 *
 * @return True if the language changed.
 */
bool Lang_ApplyChosen(void)
{
	int font;

	if (s_chosen == g_config.language) return false;

	/* the font in use, by kind (the old ones are freed) */
	font = (g_fontCurrent == g_fontNew6p) ? 1 : (g_fontCurrent != NULL && g_fontCurrent == g_fontIntro) ? 2 : 0;

	g_config.language = s_chosen;
	String_Uninit();
	String_Init();
	Font_Uninit();
	Font_Init();
	Font_Select(font == 1 ? g_fontNew6p : (font == 2 && g_fontIntro != NULL) ? g_fontIntro : g_fontNew8p);
	Lang_LetterButtons();
	return true;
}
