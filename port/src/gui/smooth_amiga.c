/** @file src/gui/smooth_amiga.c Smooth unit movement on the screen (AmigaOS).
 *
 * The game moves units every 3 game ticks (50 ms, 20 times a second), by
 * several pixels at a time. The logic stays as it is; only the drawing shows
 * a unit between its position before and after the last movement step, by the
 * time since that step (opendune.ini smooth_units=1, the default; 0 = the
 * original). Moving units and the tiles around both positions are marked for
 * redrawing in every frame (Map_Update(), as for a normal move). A jump of
 * more than 2 tiles (a new unit, a drop from a carryall) is not smoothed.
 */

#include <string.h>

#include "types.h"
#include "../os/math.h"
#include "../gfx.h"
#include "../house.h"
#include "../inifile.h"
#include "../map.h"
#include "../tile.h"
#include "../timer.h"
#include "../unit.h"
#include "../pool/pool.h"
#include "../pool/unit.h"
#include "smooth_amiga.h"

#define STEP_MS   50		/* 3 ticks of the 60 Hz game timer */
#define MAX_JUMP  512		/* 2 tiles in tile32 units */
#define MARGIN    24		/* pixels around a moving unit that its drawing can touch */

static int s_enabled = -1;
static tile32 s_prev[UNIT_INDEX_MAX];	/* positions before the last movement step */
static bool s_valid[UNIT_INDEX_MAX];
static uint32 s_stepMs;			/* Timer_GetTime() of the last movement step */
static bool s_settle;			/* one more redraw at the final positions */
static uint32 s_lastMark;		/* Timer_GetTime() of the last marking */
#define FRAME_MS  16		/* one frame at 60 Hz */

static bool Smooth_Enabled(void)
{
	if (s_enabled < 0) s_enabled = IniFile_GetInteger("smooth_units", 1) != 0 ? 1 : 0;
	return s_enabled != 0;
}

/** A movement step starts (GameLoop_Unit): every unit's position before it. */
void Smooth_StepStart(void)
{
	PoolFindStruct find;
	Unit *u;

	s_stepMs = Timer_GetTime();
	memset(s_valid, 0, sizeof(s_valid));
	if (!Smooth_Enabled()) return;
	find.houseID = HOUSE_INVALID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	while ((u = Unit_Find(&find)) != NULL) {
		if (u->o.index >= UNIT_INDEX_MAX) continue;
		s_prev[u->o.index] = u->o.position;
		s_valid[u->o.index] = true;
	}
}

/* 0..256 of the way from the previous to the current position */
static uint16 Smooth_Fraction(void)
{
	uint32 t = Timer_GetTime() - s_stepMs;
	if (t >= STEP_MS) return 256;
	return (uint16)(t * 256 / STEP_MS);
}

/** Where to draw the unit: between its last two positions. */
tile32 Smooth_Position(Unit *u)
{
	tile32 p = u->o.position;
	int32 dx, dy;
	uint16 f;

	if (!Smooth_Enabled() || u->o.index >= UNIT_INDEX_MAX || !s_valid[u->o.index]) return p;
	f = Smooth_Fraction();
	if (f >= 256) return p;
	dx = (int32)p.x - s_prev[u->o.index].x;
	dy = (int32)p.y - s_prev[u->o.index].y;
	if (dx == 0 && dy == 0) return p;
	if (dx > MAX_JUMP || dx < -MAX_JUMP || dy > MAX_JUMP || dy < -MAX_JUMP) return p;
	p.x = (uint16)(s_prev[u->o.index].x + dx * f / 256);
	p.y = (uint16)(s_prev[u->o.index].y + dy * f / 256);
	return p;
}

/**
 * Before the viewport is drawn (GUI_DrawScreen): the units on their way
 * between two positions are drawn again, with the tiles around both.
 */
void Smooth_MarkMoving(void)
{
	PoolFindStruct find;
	Unit *u;
	bool moving = Smooth_Fraction() < 256;

	if (!Smooth_Enabled()) return;
	if (!moving && !s_settle) return;
	/* the screen is sent 60 times a second; the game loop runs 120: one
	 * in-between picture per frame is enough (the last one always) */
	if (moving && Timer_GetTime() - s_lastMark < FRAME_MS) return;
	s_lastMark = Timer_GetTime();
	s_settle = moving;		/* after the last in-between frame one at the end */

	find.houseID = HOUSE_INVALID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	while ((u = Unit_Find(&find)) != NULL) {
		tile32 prev;
		if (u->o.index >= UNIT_INDEX_MAX || !s_valid[u->o.index] || u->o.flags.s.isNotOnMap) continue;
		prev = s_prev[u->o.index];
		if (prev.x == u->o.position.x && prev.y == u->o.position.y) continue;

		{
			/* every tile the sprite can touch anywhere on its way from prev to
			 * the current position: the box around both, 24 pixels more (half
			 * a sprite, the shadow at +1,+3, the blur), each redrawn with
			 * Map_Update(tile, 0) as a move does. Only the tiles around the
			 * ends left pieces of a carryall and dots of an ornithopter's
			 * shadow behind (bug report 1, 28.10). */
			const UnitInfo *ui = &g_table_unitInfo[u->o.type];
			bool winger = ui->movementType == MOVEMENT_WINGER;
			int16 x0 = min(prev.x, u->o.position.x) >> 4, x1 = max(prev.x, u->o.position.x) >> 4;
			int16 y0 = min(prev.y, u->o.position.y) >> 4, y1 = max(prev.y, u->o.position.y) >> 4;
			int16 tx, ty;

			x0 = max(0, (x0 - MARGIN) >> 4);
			y0 = max(0, (y0 - MARGIN) >> 4);
			x1 = min(63, (x1 + MARGIN) >> 4);
			y1 = min(63, (y1 + MARGIN) >> 4);
			for (ty = y0; ty <= y1; ty++) {
				for (tx = x0; tx <= x1; tx++) Map_Update(Tile_PackXY(tx, ty), 0, false);
			}
			u->o.flags.s.isDirty = true;
			if (winger) {
				g_dirtyAirUnitCount++;
			} else {
				g_dirtyUnitCount++;
			}
		}
	}
}

/** For the bug report (os/bugreport_amiga.c). */
void Smooth_Report(void)
{
	extern void AmigaBug_Printf(const char *format, ...);
	AmigaBug_Printf("smooth units: enabled %d, last step %lu ms ago, fraction %u/256, settle %d\n",
	                s_enabled, (unsigned long)(Timer_GetTime() - s_stepMs), (unsigned)Smooth_Fraction(), (int)s_settle);
}
