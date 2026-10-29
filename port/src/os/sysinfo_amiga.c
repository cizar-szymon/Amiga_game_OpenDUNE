/* The Amiga the game runs on (see sysinfo_amiga.h; from the Settlers port). */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <libraries/Picasso96.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/Picasso96.h>

#include <stdio.h>
#include <string.h>

#include "sysinfo_amiga.h"

extern struct ExecBase *SysBase;
extern struct Library *P96Base;   /* opened by video/video_amiga.c */

/* MOVEC PCR,D0 (68060 only), in supervisor mode by Supervisor(): the
	 processor's identity in bits 31-16 (0x0430 MC68060, 0x0431 MC68LC060 /
	 EC060) and its revision in bits 15-8. */
ULONG pcr_trap(void);
__asm__(".text\n"
        "\t.even\n"
        "_pcr_trap:\n"
        "\t.short 0x4e7a,0x0808\n"
        "\trte\n");

static const char kWarpTool[] = "SYS:C/WarpTool";
static const char kDiagFile[] = "RAM:OpenDUNE-warp.txt";

/* Runs a command with no input and its output to a file (or NIL:). */
static LONG run(const char *command, const char *out)
{
	BPTR in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
	BPTR o = Open((CONST_STRPTR)out, MODE_NEWFILE);
	LONG rc = -1;
	if (in != 0 && o != 0) {
		rc = SystemTags((CONST_STRPTR)command, SYS_Input, (ULONG)in,
										SYS_Output, (ULONG)o, TAG_DONE);
	}
	if (o != 0) Close(o);
	if (in != 0) Close(in);
	return rc;
}

/* The value of a "Name...... : value" line of WarpTool Diag, without its
	 colour codes (ESC [ ... m) and spaces at the ends. */
static void diag_value(const char *line, char *value, size_t size)
{
	const char *p = strstr(line, ": ");
	size_t n = 0;
	value[0] = 0;
	if (p == NULL) return;
	for (p += 2; *p != 0 && *p != '\n' && n + 1 < size; p++) {
		if (*p == 0x1b) {
			while (*p != 0 && *p != 'm') p++;
			if (*p == 0) break;
			continue;
		}
		if (*p == ' ' && n == 0) continue;
		value[n++] = *p;
	}
	while (n > 0 && value[n - 1] == ' ') n--;
	value[n] = 0;
}

static void read_warp(AmigaInfo *info)
{
	char line[160], value[64];
	BPTR lock = Lock((CONST_STRPTR)kWarpTool, SHARED_LOCK);
	FILE *f;
	if (lock == 0) return;
	UnLock(lock);
	if (run("SYS:C/WarpTool Diag", kDiagFile) != 0) return;
	f = fopen(kDiagFile, "r");
	if (f == NULL) return;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, "Model name") != NULL) {
			diag_value(line, info->warp_model, sizeof(info->warp_model));
			info->warp = 1;
		} else if (strstr(line, "Firmware version") != NULL) {
			/* WarpTool gives the version without its dot: 3054 is 3.054. */
			size_t n, i;
			diag_value(line, value, sizeof(value));
			n = strlen(value);
			for (i = 0; i < n && value[i] >= '0' && value[i] <= '9'; i++) {}
			if (n >= 2 && i == n && n + 2 <= sizeof(info->warp_firmware)) {
				info->warp_firmware[0] = value[0];
				info->warp_firmware[1] = '.';
				strcpy(info->warp_firmware + 2, value + 1);
			} else {
				strncpy(info->warp_firmware, value, sizeof(info->warp_firmware) - 1);
			}
		} else if (strstr(line, "clk freq") != NULL) {
			int mhz = 0;
			diag_value(line, value, sizeof(value));
			if (sscanf(value, "%d", &mhz) == 1) info->warp_mhz = mhz;
		}
	}
	fclose(f);
	DeleteFile((CONST_STRPTR)kDiagFile);
}

void amiga_info_read(AmigaInfo *info)
{
	UWORD attn = SysBase->AttnFlags;
	memset(info, 0, sizeof(*info));
	info->cpu_rev = -1;

	if (attn & AFF_68060) {
		ULONG pcr = Supervisor((ULONG (*)())pcr_trap);
		info->is_68060 = 1;
		info->cpu_rev = (int)((pcr >> 8) & 0xff);
		strcpy(info->cpu, (pcr >> 16) == 0x0431 ? "MC68LC060" : "MC68060");
	} else if (attn & AFF_68040) {
		strcpy(info->cpu, "MC68040");
	} else if (attn & AFF_68030) {
		strcpy(info->cpu, "MC68030");
	} else if (attn & AFF_68020) {
		strcpy(info->cpu, "MC68020");
	} else {
		strcpy(info->cpu, "MC68000");
	}
	info->fpu = (attn & (AFF_68881 | AFF_68882 | AFF_FPU40)) != 0;
	info->kick_version = SysBase->LibNode.lib_Version;
	info->kick_revision = SysBase->LibNode.lib_Revision;
	info->chip_kb = AvailMem(MEMF_CHIP | MEMF_TOTAL) / 1024;
	info->fast_kb = AvailMem(MEMF_FAST | MEMF_TOTAL) / 1024;

	if (P96Base != NULL) {
		STRPTR name = NULL;
		ULONG mem = 0;
		p96GetBoardDataTags(0, P96BD_BoardName, (ULONG)&name,
												P96BD_TotalMemory, (ULONG)&mem, TAG_DONE);
		if (name != NULL) {
			strncpy(info->board, (const char *)name, sizeof(info->board) - 1);
		}
		info->vram_kb = mem / 1024;
	}

	read_warp(info);
}

int amiga_warp_set_mhz(int mhz)
{
	char command[64];
	if (mhz < 20 || mhz > 120) return -1;
	snprintf(command, sizeof(command), "%s CPUFreqHz=%d000000", kWarpTool, mhz);
	return run(command, "NIL:") == 0 ? 0 : -1;
}
