/** @file src/string.c String routines. */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "types.h"
#include "os/common.h"
#include "os/strings.h"
#include "os/endian.h"
#include "os/error.h"

#include "string.h"

#include "config.h"
#include "file.h"
#include "table/strings.h"
#if defined(__amigaos__)
#include "lang_amiga.h"
#endif

/* 941 strings and 38356 chars in English,
   833 and 39582 in French,
   899 and 42868 in German,
   941 and about 50000 in Polish (the questions of the copy protection in English) */
#define MAX_STRING_COUNT 950U
#define MAX_CHARACTER_COUNT 60*1024U
static uint16 * s_strings = NULL;
static char * s_stringsBuffer = NULL;
static uint16 s_stringsCount = 0;

const char * const g_languageSuffixes[LANGUAGE_MAX] = { "ENG", "FRE", "GER", "ITA", "SPA", "POL" };

/**
 * Decompress a string and
 * Translates 0x1B 0xXX occurences into extended ASCII values (0x7F + 0xXX).
 *
 * Characters values >= 0x80 (1AAAABBB) are unpacked to 2 characters
 * from the table. AAAA gives the 1st characted.
 * BBB the 2nd one (from a sub-table depending on AAAA)
 *
 * @param source The compressed string.
 * @param dest The decompressed and translated string.
 * @return The length of decompressed string.
 */
uint16 String_DecompressAndTranslate(const char *s, char *dest, uint16 destLen)
{
	static const char couples[] =
		" etainosrlhcdupm"	/* 1st char */
		"tasio wb"	/* <SPACE>? */
		" rnsdalm"	/* e? */
		"h ieoras"	/* t? */
		"nrtlc sy"	/* a? */
		"nstcloer"	/* i? */
		" dtgesio"	/* n? */
		"nr ufmsw"	/* o? */
		" tep.ica"	/* s? */
		"e oiadur"	/* r? */
		" laeiyod"	/* l? */
		"eia otru"	/* h? */
		"etoakhlr"	/* c? */
		" eiu,.oa"	/* d? */
		"nsrctlai"	/* u? */
		"leoiratp"	/* p? */
		"eaoip bm";	/* m? */
	uint16 count;

	for (count = 0; *s != '\0'; s++) {
		uint8 c = *s;
		if ((c & 0x80) != 0) {
			c &= 0x7F;
			dest[count++] = couples[c >> 3];	/* 1st char */
			c = couples[c + 16];	/* 2nd char */
		} else if (c == 0x1B) {
			c = 0x7F + *(++s);
		}
		dest[count++] = c;
		if (count >= destLen - 1) {
			Warning("String_Decompress() : truncating output !\n");
			break;
		}
	}
	dest[count] = '\0';
	return count;
}

/**
 * Appends ".(ENG|FRE|...)" to the given string.
 *
 * @param name The string to append extension to.
 * @return The new string.
 */
const char *String_GenerateFilename(const char *name)
{
	static char filename[14];

	assert(g_config.language < lengthof(g_languageSuffixes));

	snprintf(filename, sizeof(filename), "%s.%s", name, g_languageSuffixes[g_config.language]);
	/* a file the language does not have (the Polish translation keeps the
	 * copy protection's questions in English, an incomplete set of German
	 * files): the English one */
	if (g_config.language != LANGUAGE_ENGLISH && !File_Exists(filename)) {
		snprintf(filename, sizeof(filename), "%s.%s", name, g_languageSuffixes[LANGUAGE_ENGLISH]);
	}
	return filename;
}

/**
 * Appends ".(ENG|FRE|...)" to the name of a picture with text in it (a CPS
 * image or a SHP file of buttons): the language's own only when it is in the
 * PC format. The pictures of the Amiga Dune II (.GER) are planar and big
 * endian, the PC code cannot draw them; Polish has no pictures of its own.
 *
 * @param name The name of the picture without extension.
 * @param image True for a CPS image, false for a SHP file.
 * @return The file name.
 */
const char *String_GenerateGfxFilename(const char *name, bool image)
{
	static char filename[14];
	uint8 head[8];
	uint8 index;
	bool pc = false;

	snprintf(filename, sizeof(filename), "%s.%s", name, g_languageSuffixes[g_config.language]);
	if (g_config.language != LANGUAGE_ENGLISH && g_config.language != LANGUAGE_POLISH && File_Exists(filename)) {
		index = File_Open(filename, FILE_MODE_READ);
		if (index != FILE_INVALID) {
			/* CPS: the size of the picture, 320x200 bytes on the PC (Amiga: 40000,
			 * five bitplanes); SHP: the count of shapes, little endian */
			if (File_Read(index, head, 8) == 8) {
				pc = image ? READ_LE_UINT32(head + 4) == 64000 : (READ_LE_UINT16(head) != 0 && READ_LE_UINT16(head) < 0x100);
			}
			File_Close(index);
		}
	}
	if (!pc) snprintf(filename, sizeof(filename), "%s.%s", name, g_languageSuffixes[LANGUAGE_ENGLISH]);
	return filename;
}

/**
 * Whether the texts of a language are there: the original's files of the
 * player (DUNE.GER ...) or the port's Polish translation.
 *
 * @param language The language.
 * @return True if the game can show its texts.
 */
bool String_LanguageAvailable(uint8 language)
{
	char filename[14];

	if (language >= LANGUAGE_MAX) return false;
	if (language == LANGUAGE_ENGLISH) return true;
	snprintf(filename, sizeof(filename), "DUNE.%s", g_languageSuffixes[language]);
	return File_Exists(filename);
}

/**
 * Returns a pointer to the string at given index in current string file.
 *
 * @param stringID The index of the string.
 * @return The pointer to the string.
 */
char *String_Get_ByIndex(uint16 stringID)
{
#if defined(__amigaos__)
	/* Options: the button goes back to the main menu (widget_click.c) */
	if (stringID == STR_QUIT_PLAYING) return (char *)Lang_T("Main menu");
#endif
	return s_stringsBuffer + s_strings[stringID];
}

/* The offsets of the strings: little endian on the PC, big endian in the
 * files of the Amiga Dune II (the German .GER files of a player's Amiga). */
static bool s_stringsBigEndian;

static uint16 String_Offset(const uint8 *buf, uint16 i)
{
	return s_stringsBigEndian ? READ_BE_UINT16(buf + i * 2) : READ_LE_UINT16(buf + i * 2);
}

/* Little endian unless the first offset (the size of the table) only makes
 * sense big endian: even, inside the file, the last offset inside too. */
static bool String_IsBigEndian(const uint8 *buf, uint32 size)
{
	uint16 le = READ_LE_UINT16(buf);
	uint16 be = READ_BE_UINT16(buf);

	if (le >= 2 && (le & 1) == 0 && le <= size && READ_LE_UINT16(buf + le - 2) < size) return false;
	return be >= 2 && (be & 1) == 0 && be <= size && READ_BE_UINT16(buf + be - 2) < size;
}

static void String_Load(const char *filename, bool compressed, uint16 start, uint16 end)
{
	uint8 *buf;
	uint16 count;
	uint16 i;
	uint32 size = 0;
	char buffer[1024];

	filename = String_GenerateFilename(filename);
	File_Exists_GetSize(filename, &size);
	buf = File_ReadWholeFile(filename);
	s_stringsBigEndian = String_IsBigEndian(buf, size);
	count = String_Offset(buf, 0) / 2;

	if (end == 0) end = start + count - 1;

	for (i = 0; i < count && s_stringsCount <= end; i++) {
		uint16 len;
		const char *src = (const char *)buf + String_Offset(buf, i);

		if (compressed) {
			len = String_DecompressAndTranslate(src, buffer, (uint16)sizeof(buffer));
		} else {
			strcpy(buffer, src);
		}
		String_Trim(buffer);

		len = strlen(buffer);
		/* a translation longer than the buffer: the rest empty, not overwritten memory */
		if ((uint32)s_strings[s_stringsCount] + len + 1 > MAX_CHARACTER_COUNT || s_stringsCount + 1 >= MAX_STRING_COUNT) {
			Warning("String_Load(%s): no room for string %hu\n", filename, s_stringsCount);
			break;
		}
		if (len > 0 || s_strings[s_stringsCount] == 0) {
			memcpy(s_stringsBuffer + s_strings[s_stringsCount++], buffer, len + 1);
			s_strings[s_stringsCount] = s_strings[s_stringsCount - 1] + len + 1;
			/* s_strings[s_stringsCount] must point to the available space in buffer */
		}
	}
	free(buf);

	if (s_stringsCount == 335) {
		Warning("DUNE V1.0 message file detected\n");
		memmove(s_strings + 283, s_strings + 281, sizeof(s_strings[0]) * (335 - 281 + 1));
		s_stringsCount += 2;
		s_strings[s_stringsCount + 2] = s_strings[s_stringsCount];	/* available space */
		s_strings[s_stringsCount++] = s_strings[STR_NULL];
		s_strings[s_stringsCount++] = s_strings[STR_OLD_SAVE_GAME_FILE_IS_INCOMPATABLE_WITH_LATEST_VERSION];
	}

	/* EU version has one more string in DUNE langfile. */
	if (s_stringsCount == STR_LOAD_GAME) {
		s_strings[s_stringsCount + 1] = s_strings[s_stringsCount];	/* available space */
		s_strings[s_stringsCount++] = s_strings[STR_LOAD_A_GAME];
	}

	while (s_stringsCount <= end) {
		Warning("String_Load(%s) filling %hu 0x%hx\n", filename, s_stringsCount, s_strings[s_stringsCount]);
		s_stringsCount++;
		s_strings[s_stringsCount] = s_strings[s_stringsCount - 1];	/* available space */
	}

	Debug("String_Load(%s) done. str count = %hu, byte count = %hu\n",
	      filename, s_stringsCount, s_strings[s_stringsCount]);
}

/**
 * Loads the language files in the memory, which is used after that with String_GetXXX_ByIndex().
 */
void String_Init(void)
{
	s_stringsCount = 0;
	s_strings = malloc(sizeof(uint16) * MAX_STRING_COUNT);
	s_strings[s_stringsCount] = 0;	/* points to available space in buffer */
	s_stringsBuffer = malloc(MAX_CHARACTER_COUNT);
	String_Load("DUNE",     false,   1, 339);
	String_Load("MESSAGE",  false, 340, 367);
	String_Load("INTRO",    false, 368, 404);
	String_Load("TEXTH",    true,  405, 444);
	String_Load("TEXTA",    true,  445, 484);
	String_Load("TEXTO",    true,  485, 524);
	String_Load("PROTECT",  true,  525,   0);
}

/**
 * Unloads the language files in the memory.
 */
void String_Uninit(void)
{
	free(s_stringsBuffer);
	s_stringsBuffer = NULL;
	free(s_strings);
	s_strings = NULL;
}

/**
 * Go to the next string.
 * @param ptr Pointer to the current string.
 * @return Pointer to the next string.
 */
uint8 *String_NextString(uint8* ptr)
{
	ptr += *ptr;
	while (*ptr == 0) ptr++;
	return ptr;
}

/**
 * Go to the previous string.
 * @param ptr Pointer to the current string.
 * @return Pointer to the previous string.
 */
uint8 *String_PrevString(uint8 *ptr)
{
	do {
		ptr--;
	} while (*ptr == 0);
	ptr -= *ptr - 1;
	return ptr;
}

void String_Trim(char *string)
{
	char *s = string + strlen(string) - 1;
	while (s >= string && isspace((uint8)*s)) {
		*s = '\0';
		s--;
	}
}
