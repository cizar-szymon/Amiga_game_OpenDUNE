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
#include <string.h>
#include "types.h"
#include "os/strings.h"

#include "lang_amiga.h"

#include "config.h"
#include "file.h"
#include "string.h"
#include "gui/font.h"

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
	return true;
}
