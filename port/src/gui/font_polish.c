/** @file src/gui/font_polish.c The Polish letters of the game's fonts. */

/*
 * Dune II has no Polish letters. These are drawn for the port in the style of
 * the original glyphs: '#' is a pixel of the letter's face (colour 1 of the
 * font); the light edge above and to the left (3) and the shadow below and to
 * the right (2) are added the way the original letters have them
 * (Font_Emboss, which gives 47 of the 52 letters A-Z a-z of NEW6P.FNT and
 * NEW8P.FNT exactly).
 *
 * The codes are ISO-8859-2 (as the Polish texts in PROGDIR:lang/POLISH.PAK):
 *   a, A ogonek  B1 A1    c, C acute  E6 C6    e, E ogonek  EA CA
 *   l, L stroke  B3 A3    n, N acute  F1 D1    o, O acute   F3 D3
 *   s, S acute   B6 A6    z, Z acute  BC AC    z, Z dot     BF AF
 *
 * NEW6P.FNT (8 rows, capitals only): accents in the top row and the letter
 * one row lower (as the Ä Ö Ü of Westwood's German NEW6PG.FNT), the ogonek in
 * the two rows under the letter. NEW8P.FNT (11 rows): the accents of small
 * letters in the two rows above them (as é), the capitals one row lower under
 * the accent (as É), the ogonek under the line (as ç). Other fonts (the intro's
 * font) get the plain letter.
 */

#include <stdlib.h>
#include <string.h>
#include "types.h"

#include "font.h"

typedef struct PolishGlyph {
	uint8 code;          /*!< ISO-8859-2. */
	uint8 base;          /*!< The plain letter, for fonts without own drawings. */
	const char *rows[11]; /*!< The face of the letter, one string a row. */
} PolishGlyph;

/* NEW6P.FNT: 8 rows */
static const PolishGlyph s_glyphs6[] = {
	{ 0xA1, 'A', {	/* A ogonek */
		"......",
		"..#...",
		".#.#..",
		".###..",
		".#.#..",
		".#.#..",
		"...#..",
		"....#.",
	} },
	{ 0xC6, 'C', {	/* C acute */
		"...##.",
		"......",
		"..##..",
		".#....",
		".#....",
		"..##..",
		"......",
		"......",
	} },
	{ 0xCA, 'E', {	/* E ogonek */
		"......",
		".###..",
		".#....",
		".##...",
		".#....",
		".###..",
		"...#..",
		"....#.",
	} },
	{ 0xA3, 'L', {	/* L stroke */
		".......",
		"..#....",
		"..#.#..",
		"..##...",
		".##....",
		"..###..",
		".......",
		".......",
	} },
	{ 0xD1, 'N', {	/* N acute */
		"....##.",
		".......",
		".#..#..",
		".##.#..",
		".#.##..",
		".#..#..",
		".......",
		".......",
	} },
	{ 0xD3, 'O', {	/* O acute */
		"...##.",
		"......",
		"..#...",
		".#.#..",
		".#.#..",
		"..#...",
		"......",
		"......",
	} },
	{ 0xA6, 'S', {	/* S acute */
		"...##.",
		"......",
		"..##..",
		".##...",
		"...#..",
		".##...",
		"......",
		"......",
	} },
	{ 0xAC, 'Z', {	/* Z acute */
		"....##..",
		"........",
		".#####..",
		"...#....",
		"..#.....",
		".#####..",
		"........",
		"........",
	} },
	{ 0xAF, 'Z', {	/* Z dot */
		"...#....",
		"........",
		".#####..",
		"...#....",
		"..#.....",
		".#####..",
		"........",
		"........",
	} },
};

/* NEW8P.FNT: 11 rows */
static const PolishGlyph s_glyphs8[] = {
	{ 0xB1, 'a', {	/* a ogonek */
		".......",
		".......",
		".......",
		"..##...",
		"....#..",
		"..###..",
		".#..#..",
		"..###..",
		"....#..",
		".....#.",
		".......",
	} },
	{ 0xE6, 'c', {	/* c acute */
		"...#...",
		"..#....",
		".......",
		"..##...",
		".#..#..",
		".#.....",
		".#..#..",
		"..##...",
		".......",
		".......",
		".......",
	} },
	{ 0xEA, 'e', {	/* e ogonek */
		".......",
		".......",
		".......",
		"..##...",
		".#..#..",
		".####..",
		".#.....",
		"..###..",
		"...#...",
		"....#..",
		".......",
	} },
	{ 0xB3, 'l', {	/* l stroke */
		"......",
		"..#...",
		"..#...",
		"..#.#.",
		"..##..",
		".##...",
		"..#...",
		"..#...",
		"......",
		"......",
		"......",
	} },
	{ 0xF1, 'n', {	/* n acute */
		"...#...",
		"..#....",
		".......",
		".###...",
		".#..#..",
		".#..#..",
		".#..#..",
		".#..#..",
		".......",
		".......",
		".......",
	} },
	{ 0xF3, 'o', {	/* o acute */
		"...#...",
		"..#....",
		".......",
		"..##...",
		".#..#..",
		".#..#..",
		".#..#..",
		"..##...",
		".......",
		".......",
		".......",
	} },
	{ 0xB6, 's', {	/* s acute */
		"....#..",
		"...#...",
		".......",
		"..###..",
		".#.....",
		"..##...",
		"....#..",
		".###...",
		".......",
		".......",
		".......",
	} },
	{ 0xBC, 'z', {	/* z acute */
		"....#...",
		"...#....",
		"........",
		".#####..",
		"....#...",
		"...#....",
		"..#.....",
		".#####..",
		"........",
		"........",
		"........",
	} },
	{ 0xBF, 'z', {	/* z dot */
		"........",
		"...#....",
		"........",
		".#####..",
		"....#...",
		"...#....",
		"..#.....",
		".#####..",
		"........",
		"........",
		"........",
	} },
	{ 0xA1, 'A', {	/* A ogonek */
		".......",
		"..##...",
		".#..#..",
		".#..#..",
		".####..",
		".#..#..",
		".#..#..",
		".#..#..",
		"....#..",
		".....#.",
		".......",
	} },
	{ 0xC6, 'C', {	/* C acute */
		"....#..",
		"...#...",
		"..##...",
		".#..#..",
		".#.....",
		".#.....",
		".#..#..",
		"..##...",
		".......",
		".......",
		".......",
	} },
	{ 0xCA, 'E', {	/* E ogonek */
		".......",
		".####..",
		".#.....",
		".#.....",
		".###...",
		".#.....",
		".#.....",
		".####..",
		"...#...",
		"....#..",
		".......",
	} },
	{ 0xA3, 'L', {	/* L stroke */
		"........",
		"..#.....",
		"..#.....",
		"..#.#...",
		"..##....",
		".##.....",
		"..#.....",
		"..####..",
		"........",
		"........",
		"........",
	} },
	{ 0xD1, 'N', {	/* N acute */
		"....#...",
		"...#....",
		".#...#..",
		".##..#..",
		".#.#.#..",
		".#..##..",
		".#...#..",
		".#...#..",
		"........",
		"........",
		"........",
	} },
	{ 0xD3, 'O', {	/* O acute */
		"....#..",
		"...#...",
		"..##...",
		".#..#..",
		".#..#..",
		".#..#..",
		".#..#..",
		"..##...",
		".......",
		".......",
		".......",
	} },
	{ 0xA6, 'S', {	/* S acute */
		"....#..",
		"...#...",
		"..##...",
		".#..#..",
		"..#....",
		"...#...",
		".#..#..",
		"..##...",
		".......",
		".......",
		".......",
	} },
	{ 0xAC, 'Z', {	/* Z acute */
		"....#...",
		"...#....",
		".#####..",
		"....#...",
		"...#....",
		"..#.....",
		".#......",
		".#####..",
		"........",
		"........",
		"........",
	} },
	{ 0xAF, 'Z', {	/* Z dot */
		"...#....",
		"........",
		".#####..",
		"....#...",
		"...#....",
		"..#.....",
		".#......",
		".#####..",
		"........",
		"........",
		"........",
	} },
};

/* The small letters of NEW6P.FNT are its capitals: ą is drawn as Ą. */
static const uint8 s_smallToCapital[9][2] = {
	{ 0xB1, 0xA1 }, { 0xE6, 0xC6 }, { 0xEA, 0xCA }, { 0xB3, 0xA3 }, { 0xF1, 0xD1 },
	{ 0xF3, 0xD3 }, { 0xB6, 0xA6 }, { 0xBC, 0xAC }, { 0xBF, 0xAF }
};

static bool Font_Face(const PolishGlyph *g, int x, int y, int width, int height)
{
	if (x < 0 || y < 0 || x >= width || y >= height) return false;
	return g->rows[y][x] == '#';
}

/* A glyph of the font from a drawing: the face 1, the shadow 2 next to the
 * face on the right and below, the light edge 3 on the left and above. */
static void Font_Emboss(FontChar *fc, const PolishGlyph *g, uint8 height)
{
	int width = (int)strlen(g->rows[0]);
	int first = height, last = -1;
	int x, y;
	uint8 cell[11][8];

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			uint8 v = 0;
			if (Font_Face(g, x, y, width, height)) v = 1;
			else if (Font_Face(g, x - 1, y, width, height) || Font_Face(g, x, y - 1, width, height) || Font_Face(g, x - 1, y - 1, width, height)) v = 2;
			else if (Font_Face(g, x + 1, y, width, height) || Font_Face(g, x, y + 1, width, height) || Font_Face(g, x + 1, y + 1, width, height)
			      || Font_Face(g, x - 1, y + 1, width, height) || Font_Face(g, x + 1, y - 1, width, height)) v = 3;
			cell[y][x] = v;
			if (v != 0) {
				if (y < first) first = y;
				last = y;
			}
		}
	}

	free(fc->data);
	fc->width = (uint8)width;
	fc->unusedLines = (uint8)first;
	fc->usedLines = (uint8)(last + 1 - first);
	fc->data = (uint8 *)malloc(fc->usedLines * fc->width);
	for (y = 0; y < fc->usedLines; y++) {
		memcpy(fc->data + y * fc->width, cell[first + y], fc->width);
	}
}

/* A copy of another glyph of the font (the plain letter). */
static void Font_CopyChar(Font *f, uint8 to, uint8 from)
{
	FontChar *fc = &f->chars[to];
	const FontChar *src = &f->chars[from];

	free(fc->data);
	*fc = *src;
	fc->data = NULL;
	if (src->data != NULL) {
		fc->data = (uint8 *)malloc(src->usedLines * src->width);
		memcpy(fc->data, src->data, src->usedLines * src->width);
	}
}

/**
 * Add the Polish letters to a font of 256 chars: drawn for NEW6P.FNT (8 rows)
 * and NEW8P.FNT (11 rows), the plain letters for other fonts.
 *
 * @param f The font.
 */
void Font_AddPolish(Font *f)
{
	const PolishGlyph *glyphs = NULL;
	unsigned int count = 0;
	unsigned int i;

	if (f == NULL || f->count < 256) return;

	if (f->height == 8) {
		glyphs = s_glyphs6;
		count = sizeof(s_glyphs6) / sizeof(s_glyphs6[0]);
	} else if (f->height == 11) {
		glyphs = s_glyphs8;
		count = sizeof(s_glyphs8) / sizeof(s_glyphs8[0]);
	}

	for (i = 0; i < count; i++) Font_Emboss(&f->chars[glyphs[i].code], &glyphs[i], f->height);

	if (f->height == 8) {
		for (i = 0; i < 9; i++) Font_CopyChar(f, s_smallToCapital[i][0], s_smallToCapital[i][1]);
	} else if (f->height != 11) {
		/* a font without drawings: the plain letters */
		for (i = 0; i < sizeof(s_glyphs8) / sizeof(s_glyphs8[0]); i++) Font_CopyChar(f, s_glyphs8[i].code, s_glyphs8[i].base);
	}
}
