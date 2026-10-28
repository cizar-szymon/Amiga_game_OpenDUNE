/** @file src/os/amiga_prof.h Time measurement by parts (AmigaOS, debug_stats=1).
 *
 * PROF_BEGIN / PROF_END add the E-clock time of a part to its total;
 * AmigaProf_Tick() (game loop) writes the shares to RAM:OpenDUNE-stats.txt
 * every 5 seconds. Without __amigaos__ the macros are empty. */

#ifndef OS_AMIGA_PROF_H
#define OS_AMIGA_PROF_H

#if defined(__amigaos__)

enum {
	PROF_LOGIC,	/* GameLoop_Team/Unit/Structure/House */
	PROF_DRAW,	/* GUI_DrawScreen: the map into the buffer */
	PROF_UI,	/* widgets, action panel, credits */
	PROF_VIDEO,	/* Video_Tick: changed blocks to the card */
	PROF_SLEEP,	/* waiting for the next 120 Hz tick */
	PROF_COUNT
};

extern uint32 g_profTicks[PROF_COUNT];
extern uint32 g_profCalls[PROF_COUNT];
extern uint32 g_profFrames;	/* frames sent to the card */
extern uint32 g_profLoops;	/* passes of the game loop */
extern uint32 AmigaTimer_EClock(void);
extern void AmigaProf_Tick(void);

#define PROF_BEGIN(id) { uint32 prof_t0_ = AmigaTimer_EClock();
#define PROF_END(id) g_profTicks[id] += AmigaTimer_EClock() - prof_t0_; g_profCalls[id]++; }

#else

#define PROF_BEGIN(id) {
#define PROF_END(id) }

#endif

#endif /* OS_AMIGA_PROF_H */
