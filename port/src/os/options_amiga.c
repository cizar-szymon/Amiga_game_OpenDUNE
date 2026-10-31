/** @file src/os/options_amiga.c The settings of opendune.ini also as Tool
 * Types of the game's icon and as Shell arguments (KEY=value, or KEY alone
 * for 1); they win over opendune.ini (IniFile_GetString/GetInteger). Tool
 * Types in brackets, "(SHOW_FPS=1)", are hints and are skipped. */

#include <exec/types.h>
#include <dos/dos.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "types.h"

#define MAX_OPTIONS 32

extern struct WBStartup *_WBenchMsg;

static char *s_keys[MAX_OPTIONS];
static char *s_values[MAX_OPTIONS];
static int s_count;

static void Options_Add(const char *text)
{
	const char *eq;
	size_t n;

	while (*text == ' ') text++;
	if (*text == '\0' || *text == '(' || *text == ';' || s_count == MAX_OPTIONS) return;
	eq = strchr(text, '=');
	n = (eq != NULL) ? (size_t)(eq - text) : strlen(text);
	while (n > 0 && text[n - 1] == ' ') n--;
	if (n == 0) return;
	s_keys[s_count] = malloc(n + 1);
	if (s_keys[s_count] == NULL) return;
	memcpy(s_keys[s_count], text, n);
	s_keys[s_count][n] = '\0';
	if (eq != NULL) {
		eq++;
		while (*eq == ' ') eq++;
	}
	s_values[s_count] = strdup(eq != NULL ? eq : "1");
	if (s_values[s_count] == NULL) {
		free(s_keys[s_count]);
		return;
	}
	s_count++;
}

/** The Tool Types of the icon the game was started from, or the arguments
 * of the Shell. Before Load_IniFile(). */
void Amiga_OptionsRead(int argc, char **argv)
{
	int i;

	if (_WBenchMsg != NULL && _WBenchMsg->sm_NumArgs > 0) {
		struct Library *IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 36);
		if (IconBase != NULL) {
			struct WBArg *arg = &_WBenchMsg->sm_ArgList[0];
			BPTR old = CurrentDir(arg->wa_Lock);
			struct DiskObject *dobj = GetDiskObject(arg->wa_Name);
			CurrentDir(old);
			if (dobj != NULL) {
				char **tt = (char **)dobj->do_ToolTypes;
				for (i = 0; tt != NULL && tt[i] != NULL; i++) Options_Add(tt[i]);
				FreeDiskObject(dobj);
			}
			CloseLibrary(IconBase);
		}
		return;
	}
	for (i = 1; i < argc; i++) Options_Add(argv[i]);
}

/** The value of an option from the icon or the Shell (key in any case), NULL
 * if it is not there (opendune.ini then). */
const char *Amiga_Option(const char *key)
{
	int i;
	for (i = 0; i < s_count; i++) {
		if (strcasecmp(s_keys[i], key) == 0) return s_values[i];
	}
	return NULL;
}
