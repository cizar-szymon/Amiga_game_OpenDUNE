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

#include "types.h"

const char amiga_version[] = "$VER: OpenDUNE 0.01 (28.10.2026)";

/* libnix: a stack of this size is set up before main() (start from an icon
 * gets 4 KB otherwise) */
unsigned long __stack = 400000;

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

	if (!AmigaTimer_Open() || s_waitReq == NULL) {
		Delay((ms + 19) / 20);
		return;
	}
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
}
