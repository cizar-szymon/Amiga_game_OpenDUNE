/** @file src/os/amiga.c AmigaOS: version string, stack, clock and sleep.
 *
 * The game runs cooperatively like the Atari port: SleepAndProcessBackgroundTasks()
 * (timer.c) sleeps until the next 120 Hz tick with AmigaTimer_WaitMs() and then
 * runs the game's timers. */

#include <exec/types.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/dos.h>
#include <stdlib.h>

#include <stdio.h>
#include "types.h"
#include "../inifile.h"
#include "amiga_prof.h"

const char amiga_version[] = "$VER: OpenDUNE 0.01 (28.10.2026)";

/* libnix: a stack of this size is set up before main() (start from an icon
 * gets only the icon's stack otherwise). __stack alone does not do it: the
 * code that swaps the stack (swapstack.o, __stkinit in the init list) is only
 * linked in when something refers to it. Without it, the LAN game (Lan_Run
 * from the windows) overflowed the 16 KB of the icon into g_map (29.10.2026). */
unsigned long __stack = 400000;
extern void __stkinit(void);
void (*const amiga_stackSwap)(void) = __stkinit;

/** The stack of the game (for the bug report): its size and what is free below here. */
void Amiga_StackInfo(unsigned long *size, unsigned long *free)
{
	struct Task *me = FindTask(NULL);
	ULONG here = (ULONG)&me;
	*size = (ULONG)me->tc_SPUpper - (ULONG)me->tc_SPLower;
	*free = here - (ULONG)me->tc_SPLower;
}

/* libnix's I/O calls __chkabort(), which on Ctrl-C calls exit() in the middle
 * of a file operation; Ctrl-C is handled in Video_Tick() instead. */
void __chkabort(void) {}

struct Device *TimerBase;
static struct timerequest s_eclockReq;	/* TimerBase, ReadEClock */
static struct MsgPort *s_waitPort;
static struct timerequest *s_waitReq;	/* UNIT_MICROHZ, sleeping */
static ULONG s_eclockPerMs;

static void AmigaTimer_Close(void)
{
	if (s_waitReq != NULL) {
		CloseDevice((struct IORequest *)s_waitReq);
		DeleteIORequest((struct IORequest *)s_waitReq);
		s_waitReq = NULL;
	}
	if (s_waitPort != NULL) {
		DeleteMsgPort(s_waitPort);
		s_waitPort = NULL;
	}
	if (TimerBase != NULL) {
		CloseDevice((struct IORequest *)&s_eclockReq);
		TimerBase = NULL;
	}
}

static bool AmigaTimer_Open(void)
{
	if (TimerBase != NULL) return true;
	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&s_eclockReq, 0) != 0) return false;
	TimerBase = s_eclockReq.tr_node.io_Device;
	s_waitPort = CreateMsgPort();
	if (s_waitPort != NULL) s_waitReq = (struct timerequest *)CreateIORequest(s_waitPort, sizeof(struct timerequest));
	if (s_waitReq != NULL && OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)s_waitReq, 0) != 0) {
		DeleteIORequest((struct IORequest *)s_waitReq);
		s_waitReq = NULL;
	}
	atexit(AmigaTimer_Close);
	return true;
}

/** Milliseconds from the E-clock (low 32 bits of hi:lo / ticks per ms, no 64-bit division). */
uint32 AmigaTimer_GetMs(void)
{
	struct EClockVal ev;
	ULONG hi, lo, r, q;
	int b;

	if (!AmigaTimer_Open()) return 0;
	s_eclockPerMs = ReadEClock(&ev) / 1000;
	hi = ev.ev_hi;
	lo = ev.ev_lo;
	r = hi % s_eclockPerMs;
	q = 0;
	for (b = 31; b >= 0; b--) {
		r = (r << 1) | ((lo >> b) & 1);
		q <<= 1;
		if (r >= s_eclockPerMs) {
			r -= s_eclockPerMs;
			q |= 1;
		}
	}
	return q;
}

/** Sleeps ms milliseconds; returns early on Ctrl-C (Video_Tick() then quits). */
void AmigaTimer_WaitMs(uint32 ms)
{
	ULONG sig;
	uint32 t0;

	if (!AmigaTimer_Open() || s_waitReq == NULL) {
		Delay((ms + 19) / 20);
		return;
	}
	t0 = AmigaTimer_EClock();
	s_waitReq->tr_node.io_Command = TR_ADDREQUEST;
	s_waitReq->tr_time.tv_secs = ms / 1000;
	s_waitReq->tr_time.tv_micro = (ms % 1000) * 1000;
	SendIO((struct IORequest *)s_waitReq);
	sig = Wait((1UL << s_waitPort->mp_SigBit) | SIGBREAKF_CTRL_C);
	if (sig & SIGBREAKF_CTRL_C) {
		SetSignal(SIGBREAKF_CTRL_C, SIGBREAKF_CTRL_C);	/* keep it for Video_Tick() */
		if (!CheckIO((struct IORequest *)s_waitReq)) AbortIO((struct IORequest *)s_waitReq);
	}
	WaitIO((struct IORequest *)s_waitReq);
	g_profTicks[PROF_SLEEP] += AmigaTimer_EClock() - t0;
	g_profCalls[PROF_SLEEP]++;
}

/* ---- time by parts (os/amiga_prof.h), debug_stats=1 ---- */

uint32 g_profTicks[PROF_COUNT];
uint32 g_profCalls[PROF_COUNT];
uint32 g_profFrames;
uint32 g_profLoops;
uint32 g_profAreas[PROF_AREA_COUNT];
uint32 g_profBlits;
bool g_profStatsOn;
uint32 g_profBytes;

/** Low 32 bits of the E-clock (differences of up to ~70 minutes). */
uint32 AmigaTimer_EClock(void)
{
	struct EClockVal ev;
	if (!AmigaTimer_Open()) return 0;
	ReadEClock(&ev);
	return ev.ev_lo;
}

/**
 * Called once per pass of the game loop: with debug_stats=1 every 5 seconds
 * one line in RAM:OpenDUNE-stats.txt: frames sent and loop passes per second,
 * each part's share of the time and its ms per call.
 */
void AmigaProf_Tick(void)
{
	static int s_enabled = -1;
	static uint32 s_last, s_lastMs, s_t0;
	static const char *names[PROF_COUNT] = { "logic", "draw", "ui", "video", "sleep", "v.events", "v.areas", "v.send", "v.lock" };
	uint32 now, wall, sum = 0;
	FILE *f;
	int i;

	if (s_enabled < 0) {
		s_enabled = IniFile_GetInteger("debug_stats", 0) != 0;
		g_profStatsOn = s_enabled != 0;
		s_last = AmigaTimer_EClock();
		s_t0 = s_lastMs = AmigaTimer_GetMs();
		if (s_enabled) {
			f = fopen("RAM:OpenDUNE-stats.txt", "w");
			if (f != NULL) {
				fprintf(f, "OpenDUNE stats every 5 s: fps = frames sent to the card, loop = game loop passes;\n"
				           "share of the time and ms per call of logic, draw (map), ui, video (upload), sleep\n");
				fclose(f);
			}
		}
	}
	g_profLoops++;
	if (!s_enabled) return;
	if (AmigaTimer_GetMs() - s_lastMs < 5000) return;

	now = AmigaTimer_EClock();
	wall = now - s_last;
	f = fopen("RAM:OpenDUNE-stats.txt", "a");
	if (f != NULL && wall > 0) {
		uint32 ms = AmigaTimer_GetMs() - s_lastMs;
		fprintf(f, "%5lus fps %2lu loop %3lu |", (unsigned long)((AmigaTimer_GetMs() - s_t0) / 1000),
		        (unsigned long)(g_profFrames * 1000 / ms), (unsigned long)(g_profLoops * 1000 / ms));
		for (i = 0; i < PROF_COUNT; i++) {
			/* per mille of the time; ms per call in tenths */
			uint32 pm = g_profTicks[i] / (wall / 1000 + 1);
			uint32 per = g_profCalls[i] ? g_profTicks[i] / g_profCalls[i] * 10 / s_eclockPerMs : 0;
			if (i < PROF_V_EVENTS) sum += g_profTicks[i];
			fprintf(f, " %s %lu.%lu%% %lu.%lums", names[i], (unsigned long)(pm / 10), (unsigned long)(pm % 10),
			        (unsigned long)(per / 10), (unsigned long)(per % 10));
		}
		{
			uint32 pm = (wall > sum) ? (wall - sum) / (wall / 1000 + 1) : 0;
			fprintf(f, " other %lu.%lu%% | blits %lu, %lu KB/frame | frames with changes: top %lu view %lu radar %lu panel %lu\n",
			        (unsigned long)(pm / 10), (unsigned long)(pm % 10), (unsigned long)g_profBlits,
			        (unsigned long)(g_profFrames ? g_profBytes / g_profFrames / 1024 : 0),
			        (unsigned long)g_profAreas[PROF_AREA_TOP], (unsigned long)g_profAreas[PROF_AREA_VIEW],
			        (unsigned long)g_profAreas[PROF_AREA_RADAR], (unsigned long)g_profAreas[PROF_AREA_PANEL]);
		}
		fclose(f);
	}
	for (i = 0; i < PROF_COUNT; i++) g_profTicks[i] = g_profCalls[i] = 0;
	g_profFrames = g_profLoops = 0;
	g_profBlits = g_profBytes = 0;
	for (i = 0; i < PROF_AREA_COUNT; i++) g_profAreas[i] = 0;
	s_last = now;
	s_lastMs = AmigaTimer_GetMs();
}
