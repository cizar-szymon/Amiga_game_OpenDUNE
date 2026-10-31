/** @file src/os/syscheck_amiga.c What the game needs of the Amiga, said in a
 * requester at the start instead of a crash or a silent quit (in English,
 * Polish and German: the language is not known yet).
 *
 * Compiled for the 68000 (Makefile.amiga): on a 68000 or 68010 the rest of the
 * game (68020 code) must not run, this part has to. Up to the CPU test only
 * system calls, no libnix and no 32-bit multiply or divide. */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <stdio.h>

#include "types.h"

extern struct ExecBase *SysBase;

#define NEED_FAST_KB 8192	/* the game used 6.5 MB in a mission (31.10.2026) */

static LONG Syscheck_Ask(const char *text, const char *gadgets)
{
	struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"OpenDUNE", (UBYTE *)"%s", (UBYTE *)gadgets };
	return EasyRequest(NULL, &es, NULL, (APTR)text);
}

/**
 * The CPU and the memory: a 68000/68010 or too little Fast RAM ends the
 * game; a 68020/68030 is told it may be slow (play on or quit).
 * @return True if the game may start.
 */
bool Amiga_SystemCheck(void)
{
	UWORD attn = SysBase->AttnFlags;
	ULONG fastKb, tenths;
	char text[512];

	if ((attn & AFF_68020) == 0) {
		Syscheck_Ask(
			"OpenDUNE needs a 68020 or better (it is made for a 68040 or 68060).\n\n"
			"OpenDUNE wymaga procesora 68020 lub lepszego (jest robiony na 68040 lub 68060).\n\n"
			"OpenDUNE braucht einen 68020 oder besser (gemacht f\xfcr 68040 oder 68060).",
			"OK");
		return false;
	}

	fastKb = AvailMem(MEMF_FAST) >> 10;
	tenths = (((fastKb & 1023) << 3) + ((fastKb & 1023) << 1)) >> 10;	/* (no multiply: 68000) */
	if (fastKb < NEED_FAST_KB) {
		snprintf(text, sizeof(text),
			"OpenDUNE needs 8 MB of free Fast RAM, there are %lu.%lu MB.\n\n"
			"OpenDUNE wymaga 8 MB wolnej pamieci Fast RAM, jest %lu.%lu MB.\n\n"
			"OpenDUNE braucht 8 MB freies Fast-RAM, frei sind %lu,%lu MB.",
			fastKb >> 10, tenths, fastKb >> 10, tenths,
			fastKb >> 10, tenths);
		Syscheck_Ask(text, "OK");
		return false;
	}

	if ((attn & (AFF_68040 | AFF_68060)) == 0) {
		snprintf(text, sizeof(text),
			"OpenDUNE is made for a 68040 or 68060: on a %s it may run slowly.\n\n"
			"OpenDUNE jest robiony na 68040 lub 68060: na %s moze dzialac wolno.\n\n"
			"OpenDUNE ist f\xfcr 68040 oder 68060 gemacht: auf einem %s l\xe4uft es vielleicht langsam.",
			(attn & AFF_68030) ? "68030" : "68020", (attn & AFF_68030) ? "68030" : "68020",
			(attn & AFF_68030) ? "68030" : "68020");
		if (Syscheck_Ask(text, "Play - Graj - Spielen|Quit - Wyjdz - Beenden") == 0) return false;
	}
	return true;
}

/** No Picasso96, or no 8-bit screen mode of 640x480 or more (Video_Init). */
void Amiga_NoRtg(void)
{
	Syscheck_Ask(
		"OpenDUNE needs a graphics card (RTG) with Picasso96 and an 8-bit screen mode of 640x480 or more.\n\n"
		"OpenDUNE wymaga karty graficznej (RTG) z Picasso96 i trybu 8-bitowego 640x480 lub wiekszego.\n\n"
		"OpenDUNE braucht eine Grafikkarte (RTG) mit Picasso96 und einem 8-Bit-Bildschirmmodus ab 640x480.",
		"OK");
}
