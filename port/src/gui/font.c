/** @file src/gui/font.c Font routines. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "types.h"

#include "font.h"
#include "../os/endian.h"

#include "../config.h"
#include "../file.h"
#include "../string.h"
#if defined(__amigaos__)
#include "../inifile.h"
#include "font_clean.h"
#endif

Font *g_fontIntro = NULL;
Font *g_fontNew6p = NULL;
Font *g_fontNew8p = NULL;

int8 g_fontCharOffset = -1;
#if defined(__amigaos__)
bool g_fontClean = false;	/* opendune.ini font=clean is on */
#endif

Font *g_fontCurrent = NULL;

/**
 * Get the width of a char in pixels.
 *
 * @param c The char to get the width of.
 * @return The width of the char in pixels.
 */
uint16 Font_GetCharWidth(unsigned char c)
{
	return g_fontCurrent->chars[c].width + g_fontCharOffset;
}

/**
 * Get the width of the string in pixels.
 *
 * @param string The string to get the width of.
 * @return The width of the string in pixels.
 */
uint16 Font_GetStringWidth(const char *string)
{
	uint16 width = 0;

	if (string == NULL) return 0;

	while (*string != '\0') {
		width += Font_GetCharWidth(*string++);
	}

	return width;
}

/**
 * Load a font file.
 *
 * @param filename The name of the font file.
 * @return The pointer of the allocated memory where the file has been read.
 */
static Font *Font_LoadFile(const char *filename)
{
	uint8 *buf;
	Font *f;
	uint16 i;
	uint16 count;
	uint16 start;
	uint16 dataStart;
	uint16 widthList;
	uint16 lineList;

	if (!File_Exists(filename)) return NULL;

	buf = (uint8 *)File_ReadWholeFile(filename);

	if (buf[2] != 0x00 || buf[3] != 0x05) {
		free(buf);
		return NULL;
	}

	f = (Font *)calloc(1, sizeof(Font));
	start = READ_LE_UINT16(buf + 4);
	dataStart = READ_LE_UINT16(buf + 6);
	widthList = READ_LE_UINT16(buf + 8);
	lineList = READ_LE_UINT16(buf + 12);
	f->height = buf[start + 4];
	f->maxWidth = buf[start + 5];
	count = READ_LE_UINT16(buf + 10) - widthList;
	if (count > 256) count = 256;
	/* 256 chars, the ones the file does not have empty: any byte of a text can
	 * be drawn (the Polish letters are ISO-8859-2, above the file's 226) */
	f->count = 256;
	f->chars = (FontChar *)calloc(f->count, sizeof(FontChar));

	for (i = 0; i < count; i++) {
		FontChar *fc = &f->chars[i];
		uint16 dataOffset;
		uint8 x;
		uint8 y;

		fc->width = buf[widthList + i];
		fc->unusedLines = buf[lineList + i * 2];
		fc->usedLines = buf[lineList + i * 2 + 1];

		dataOffset = READ_LE_UINT16(buf + dataStart + i * 2);
		if (dataOffset == 0) continue;

		fc->data = (uint8 *)malloc(fc->usedLines * fc->width);

		for (y = 0; y < fc->usedLines; y++) {
			for (x = 0; x < fc->width; x++) {
				uint8 data = buf[dataOffset + y * ((fc->width + 1) / 2) + x / 2];
				if (x % 2 != 0) data >>= 4;
				fc->data[y * fc->width + x] = data & 0xF;
			}
		}
	}

	free(buf);

	return f;
}

/**
 * Select a font.
 *
 * @param font The pointer of the font to use.
 */
void Font_Select(Font *f)
{
	if (f == NULL) return;

	g_fontCurrent = f;
}

#if defined(__amigaos__)
/**
 * The clean font (opendune.ini font=clean): the letters of the game's 8 pixel
 * font replaced by plain ones of 6x10 (font_clean.h), the face (colour 1 of
 * the font) with a shadow below and to the right (2) and no light edge. As in
 * the game's font the face starts in the second column (the first one is the
 * light edge there) and a capital letter in the second row, on the same line
 * of writing: the texts stand where the game's windows expect them. The
 * letters font_clean.h has not stay the game's.
 *
 * @param f The 8 pixel font.
 * @param polish The Polish letters too (their codes are other letters in the
 *   other languages).
 */
static void Font_MakeClean(Font *f, bool polish)
{
	uint16 i;

	if (f == NULL) return;
	for (i = 0; i < sizeof(s_cleanGlyphs) / sizeof(s_cleanGlyphs[0]); i++) {
		const CleanGlyph *g = &s_cleanGlyphs[i];
		FontChar *fc = &f->chars[g->code];
		uint8 w = CLEAN_WIDTH + 1;	/* the width of a letter is one more than its step (g_fontCharOffset) */
		uint8 *data;
		uint8 x, y;

		if (g->polish && !polish) continue;
		data = (uint8 *)calloc(CLEAN_HEIGHT * w, 1);
		if (data == NULL) continue;
		for (y = 0; y < CLEAN_HEIGHT; y++) {
			for (x = 0; x < CLEAN_WIDTH; x++) {
				if ((g->rows[y] & (1 << (CLEAN_WIDTH - 1 - x))) == 0) continue;
				if (x + 1 >= w) continue;
				data[y * w + x + 1] = 1;
				if (y + 1 < CLEAN_HEIGHT && x + 2 < w && data[(y + 1) * w + x + 2] == 0) data[(y + 1) * w + x + 2] = 2;
			}
		}
		/* (a shadow written before the face of the row below it: the face wins) */
		for (y = 1; y < CLEAN_HEIGHT; y++) {
			for (x = 0; x < CLEAN_WIDTH; x++) {
				if ((g->rows[y] & (1 << (CLEAN_WIDTH - 1 - x))) != 0 && x + 1 < w) data[y * w + x + 1] = 1;
			}
		}
		free(fc->data);
		fc->data = data;
		fc->width = w;
		fc->unusedLines = 0;
		fc->usedLines = CLEAN_HEIGHT;
	}
	if (f->height < CLEAN_HEIGHT) f->height = CLEAN_HEIGHT;
	if (f->maxWidth < CLEAN_WIDTH + 1) f->maxWidth = CLEAN_WIDTH + 1;
}
#endif

bool Font_Init(void)
{
	g_fontIntro = Font_LoadFile("INTRO.FNT");
	if ((g_config.language == LANGUAGE_GERMAN) && File_Exists("new6pg.fnt")) {
		g_fontNew6p = Font_LoadFile("new6pg.fnt");
	} else {
		g_fontNew6p = Font_LoadFile("new6p.fnt");
	}
	g_fontNew8p = Font_LoadFile("new8p.fnt");
#if defined(__amigaos__)
	g_fontClean = false;
#endif

	if (g_config.language == LANGUAGE_POLISH) {
		Font_AddPolish(g_fontIntro);
		Font_AddPolish(g_fontNew6p);
		Font_AddPolish(g_fontNew8p);
	}
#if defined(__amigaos__)
	{
		/* opendune.ini / Tool Type font=clean: plain letters in place of the 8 pixel font's */
		char name[16];
		if (IniFile_GetString("font", "", name, sizeof(name)) != NULL && strcasecmp(name, "clean") == 0) {
			Font_MakeClean(g_fontNew8p, g_config.language == LANGUAGE_POLISH);
			g_fontClean = g_fontNew8p != NULL;
		}
	}
#endif

	return g_fontNew8p != NULL;
}

static void Font_Unload(Font *f) {
	uint16 i;

	if (f == NULL) return;	/* INTRO.FNT is not in every set of data files */
	for (i = 0; i < f->count; i++) free(f->chars[i].data);
	free(f->chars);
	free(f);
}

void Font_Uninit(void)
{
	Font_Unload(g_fontIntro); g_fontIntro = NULL;
	Font_Unload(g_fontNew6p); g_fontNew6p = NULL;
	Font_Unload(g_fontNew8p); g_fontNew8p = NULL;
}
