/*
 * sysinfo_amiga.h - The Amiga the game runs on, for the Amiga info window of
 * the Options (CPU and its revision, Kickstart, memory, graphics card, and a
 * Warp accelerator through SYS:C/WarpTool). From the Settlers port.
 */

#ifndef OS_SYSINFO_AMIGA_H
#define OS_SYSINFO_AMIGA_H

typedef struct AmigaInfo {
	char cpu[24];            /* "MC68060", "MC68LC060", "MC68040", ... */
	int is_68060;
	int cpu_rev;             /* 68060: the revision in its PCR, else -1 */
	int fpu;
	int kick_version, kick_revision;
	unsigned long chip_kb, fast_kb;
	char board[40];          /* the Picasso96 board, "" if none */
	unsigned long vram_kb;
	int warp;                /* SYS:C/WarpTool answered */
	char warp_model[24];
	char warp_firmware[16];
	int warp_mhz;            /* the CPU clock it reports, 0 if unknown */
} AmigaInfo;

/* Fills info; the Warp part runs "WarpTool Diag" (a fraction of a second). */
extern void amiga_info_read(AmigaInfo *info);

/* Sets the CPU clock of the Warp (WarpTool CPUFreqHz, at once), 20 to 120
 * MHz (120: a clock the user set above 100 can be set back). 0: done. */
extern int amiga_warp_set_mhz(int mhz);

#endif /* OS_SYSINFO_AMIGA_H */
