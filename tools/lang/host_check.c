/*
 * The game's own code for the languages, built for the PC (check_lang.py
 * builds and runs it; no Amiga needed): port/src/string.c (String_Load with
 * the big endian offsets of the Amiga .GER files, the IDs of the strings),
 * port/src/gui/font.c and font_polish.c (the fonts of 256 chars, the Polish
 * letters). A file layer of a few lines reads the files from a directory.
 *
 *   host_check DIR LANGUAGE OUT    (LANGUAGE: the number of string.h)
 *
 * OUT: the strings (ID, length, bytes) and the Polish letters of NEW6P and
 * NEW8P (code, width, unused and used lines, pixels) - compared there with
 * the Python model.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "types.h"
#include "config.h"
#include "file.h"

DuneCfg g_config;
static const char *s_dir;

void Warning(const char *format, ...) { (void)format; }
void Error(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

static FILE *Host_Open(const char *filename)
{
	char path[1024];
	snprintf(path, sizeof(path), "%s/%s", s_dir, filename);
	return fopen(path, "rb");
}

/* the file layer of file.c, enough for string.c and font.c */
bool File_Exists_Ex(enum SearchDirectory dir, const char *filename, uint32 *fileSize)
{
	FILE *f = Host_Open(filename);
	(void)dir;
	if (f == NULL) return false;
	if (fileSize != NULL) {
		fseek(f, 0, SEEK_END);
		*fileSize = (uint32)ftell(f);
	}
	fclose(f);
	return true;
}

static FILE *s_open[8];

uint8 File_Open_Ex(enum SearchDirectory dir, const char *filename, uint8 mode)
{
	uint8 i;
	(void)dir; (void)mode;
	for (i = 0; i < 8; i++) {
		if (s_open[i] == NULL) {
			s_open[i] = Host_Open(filename);
			return s_open[i] != NULL ? i : FILE_INVALID;
		}
	}
	return FILE_INVALID;
}

uint32 File_Read(uint8 index, void *buffer, uint32 length)
{
	return (uint32)fread(buffer, 1, length, s_open[index]);
}

void File_Close(uint8 index)
{
	fclose(s_open[index]);
	s_open[index] = NULL;
}

void *File_ReadWholeFile(const char *filename)
{
	FILE *f = Host_Open(filename);
	long n;
	char *buf;
	if (f == NULL) return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc(n + 1);
	if (fread(buf, 1, n, f) != (size_t)n) n = 0;
	buf[n] = '\0';
	fclose(f);
	return buf;
}

#include "string.c"
#include "gui/font.c"
#include "gui/font_polish.c"

static void Dump_Font(FILE *out, const char *name, Font *f)
{
	static const uint8 codes[18] = { 0xB1, 0xE6, 0xEA, 0xB3, 0xF1, 0xF3, 0xB6, 0xBC, 0xBF, 0xA1, 0xC6, 0xCA, 0xA3, 0xD1, 0xD3, 0xA6, 0xAC, 0xAF };
	int i, n;
	if (f == NULL) return;
	fprintf(out, "font %s %u %u\n", name, f->height, f->count);
	for (i = 0; i < 18; i++) {
		FontChar *fc = &f->chars[codes[i]];
		fprintf(out, "char %02X %u %u %u ", codes[i], fc->width, fc->unusedLines, fc->usedLines);
		for (n = 0; fc->data != NULL && n < fc->usedLines * fc->width; n++) fprintf(out, "%u", fc->data[n]);
		fprintf(out, "\n");
	}
}

int main(int argc, char **argv)
{
	FILE *out;
	uint16 i;

	if (argc < 4) {
		fprintf(stderr, "host_check DIR LANGUAGE OUT\n");
		return 2;
	}
	s_dir = argv[1];
	g_config.language = (uint8)atoi(argv[2]);
	out = fopen(argv[3], "wb");
	if (out == NULL) return 2;

	String_Init();
	fprintf(out, "strings %u\n", s_stringsCount);
	for (i = 0; i < s_stringsCount; i++) {
		const char *s = String_Get_ByIndex(i);
		fprintf(out, "%u %u ", i, (unsigned)strlen(s));
		fwrite(s, 1, strlen(s), out);
		fprintf(out, "\n");
	}
	/* (one a call: the name is in a static buffer) */
	fprintf(out, "gfx %s", String_GenerateGfxFilename("BTTN", false));
	fprintf(out, " %s", String_GenerateGfxFilename("CHOAM", false));
	fprintf(out, " %s", String_GenerateGfxFilename("TITLE", true));
	fprintf(out, " %s\n", String_GenerateGfxFilename("HERALD", true));
	fprintf(out, "mentat %s\n", String_GenerateFilename("MENTATA"));

	if (Font_Init()) {
		Dump_Font(out, "6p", g_fontNew6p);
		Dump_Font(out, "8p", g_fontNew8p);
		Font_Uninit();
	}
	String_Uninit();
	fclose(out);
	return 0;
}
