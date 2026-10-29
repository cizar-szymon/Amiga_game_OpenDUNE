/** @file src/gui/rts_amiga.c Group selection and right-click orders (AmigaOS).
 *
 * An addition to the original controls (opendune.ini rts_controls=1, the
 * default; 0 = the original only), on the map screen:
 *  - left button held and dragged on the viewport: a rectangle; on release all
 *    the player's units inside it are selected (the first one is shown in the
 *    sidebar, all get the selection mark). A plain click works as before.
 *  - the pointer at an edge of the whole screen (also over the top bar and
 *    the sidebar) scrolls the map by itself, in a corner diagonally, with an
 *    arrow pointer (RTS_EdgeScroll(), 10 tiles a second).
 *  - P pauses the game (the game timer stops; scrolling, selecting and
 *    orders still work, the orders are carried out after the pause), P again
 *    goes on (RTS_PauseRequest() from the video driver's key handling).
 *  - right button on the viewport with own units selected: they move there,
 *    a group keeping its shape (at most 3 tiles from the point); on an enemy
 *    unit or structure, the units that can attack attack it.
 *
 * The mouse events come from the video driver's timer tick
 * (RTS_FilterButtons() only records them); selections and orders are made in
 * the game loop (RTS_Tick()). The rectangle is drawn by the video driver over
 * the picture (RTS_GetDragRect()).
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "../os/common.h"
#include "../os/math.h"
#include "../gfx.h"
#include "../house.h"
#include "../inifile.h"
#include "../map.h"
#include "../input/mouse.h"
#include "../object.h"
#include "../opendune.h"
#include "../structure.h"
#include "../tile.h"
#include "../timer.h"
#include "../tools.h"
#include "../unit.h"
#include "../audio/sound.h"
#include "../pool/pool.h"
#include "../pool/unit.h"
#include "../audio/driver.h"
#include "../config.h"
#include "../sprites.h"
#include "gui.h"
#include "widget.h"
#include "../string.h"
#include "../table/strings.h"
#include "rts_amiga.h"
#include "../lan_amiga.h"

#if defined(__amigaos__)
/* a LAN game: the interface draws its own random numbers, never the simulation's */
#define Tools_Random_256() (g_lanGame ? Tools_RandomUI_256() : (Tools_Random_256)())
#define Tools_RandomLCG_Range(a, b) (g_lanGame ? Tools_RandomUI_Range(a, b) : (Tools_RandomLCG_Range)(a, b))
#define Tools_RandomLCG_Seed(a) do { if (!g_lanGame) (Tools_RandomLCG_Seed)(a); } while (0)
#endif

#define MAX_GROUP 64
#define DRAG_START 4	/* pixels before a press becomes a rectangle */

int RTS_GroupSize(void);

static int s_enabled = -1;		/* -1 = ini not read yet */
static volatile uint32 s_lastMain;	/* Timer_GetTime() of the last game loop pass */

static uint16 s_group[MAX_GROUP];	/* unit indexes */
static int s_groupCount;
static Unit *s_lastSelected;

/* mouse state, written from the video driver's tick */
static volatile bool s_leftDown, s_rightDown;
static volatile bool s_tracking, s_dragging, s_rightSwallow;
static volatile int16 s_dx0, s_dy0, s_dx1, s_dy1;
static volatile bool s_selectPending, s_orderPending;
static volatile bool s_plainClick;	/* a click on the viewport: the group ends */
static volatile int16 s_sx0, s_sy0, s_sx1, s_sy1;	/* the rectangle to select */
static volatile int16 s_ox, s_oy;				/* the order point */

static bool RTS_InViewport(int x, int y)
{
	return x >= 0 && x < VIEWPORT_TILES_W * 16 && y >= 40 && y < 40 + VIEWPORT_TILES_H * 16;
}

/* Only on the map screen, while the game loop runs (not in a window). */
static bool RTS_Active(void)
{
	if (s_enabled <= 0) return false;
	if (g_gameMode != GM_NORMAL) return false;
	if (g_selectionType != SELECTIONTYPE_UNIT && g_selectionType != SELECTIONTYPE_STRUCTURE) return false;
	return Timer_GetTime() - s_lastMain < 100;
}

static bool RTS_HasOwnSelection(void)
{
	return s_groupCount > 0 || (g_unitSelected != NULL && Unit_GetHouseID(g_unitSelected) == LOCAL_HOUSE_ID);
}

/**
 * Takes the button events that belong to a drag or a right-click order. A
 * left press on the viewport is held back until it is known whether it is a
 * drag; when it is not, *clickFirst asks the caller to give the game the press
 * before the release (a plain click, as in the original).
 */
uint8 RTS_FilterButtons(uint16 x, uint16 y, uint8 buttons, bool *clickFirst)
{
	bool left = (buttons & 1) != 0, right = (buttons & 2) != 0;

	*clickFirst = false;
	if (s_enabled <= 0) return buttons;

	if (left && !s_leftDown) {
		s_leftDown = true;
		s_tracking = RTS_Active() && RTS_InViewport(x, y);
		s_dragging = false;
		s_dx0 = s_dx1 = x;
		s_dy0 = s_dy1 = y;
	} else if (!left && s_leftDown) {
		s_leftDown = false;
		if (s_dragging) {
			s_sx0 = s_dx0; s_sy0 = s_dy0; s_sx1 = s_dx1; s_sy1 = s_dy1;
			s_selectPending = true;
		} else if (s_tracking) {
			*clickFirst = true;
			s_plainClick = true;
		}
		s_tracking = false;
		s_dragging = false;
	}
	if (left && s_tracking) {
		s_dx1 = x;
		s_dy1 = y;
		if (!s_dragging && (abs(s_dx1 - s_dx0) > DRAG_START || abs(s_dy1 - s_dy0) > DRAG_START)) s_dragging = true;
		buttons &= ~1;
	}

	if (right && !s_rightDown) {
		s_rightDown = true;
		if (RTS_Active() && RTS_InViewport(x, y) && RTS_HasOwnSelection()) {
			s_rightSwallow = true;
			s_ox = x;
			s_oy = y;
			s_orderPending = true;
		}
	}
	if (s_rightSwallow) {
		buttons &= ~2;
		if (!right) s_rightSwallow = false;
	}
	if (!right) s_rightDown = false;
	return buttons;
}

bool RTS_GetDragRect(int16 *x0, int16 *y0, int16 *x1, int16 *y1)
{
	int16 a, b;
	if (!s_dragging) return false;
	a = s_dx0; b = s_dx1;
	*x0 = min(a, b); *x1 = max(a, b);
	a = s_dy0; b = s_dy1;
	*y0 = min(a, b); *y1 = max(a, b);
	/* inside the viewport */
	*x1 = min(*x1, (int16)(VIEWPORT_TILES_W * 16 - 1));
	*y0 = max(*y0, (int16)40);
	*y1 = min(*y1, (int16)(40 + VIEWPORT_TILES_H * 16 - 1));
	return true;
}

static Unit *RTS_GroupUnit(int i)
{
	Unit *u = Unit_Get_ByIndex(s_group[i]);
	if (u == NULL || !u->o.flags.s.used || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap) return NULL;
	if (Unit_GetHouseID(u) != LOCAL_HOUSE_ID) return NULL;
	return u;
}

static bool RTS_InGroupAny(Unit *u)
{
	int i;
	for (i = 0; i < s_groupCount; i++) {
		if (s_group[i] == u->o.index) return true;
	}
	return false;
}

/** For the selection mark: a member of a group of two or more. */
bool RTS_IsInGroup(Unit *u)
{
	return s_groupCount > 1 && RTS_InGroupAny(u);
}

static void RTS_ClearGroup(void)
{
	int i;
	for (i = 0; i < s_groupCount; i++) {
		Unit *u = RTS_GroupUnit(i);
		if (u != NULL) Unit_UpdateMap(2, u);	/* redraw without the mark */
	}
	s_groupCount = 0;
}

static bool RTS_Selectable(Unit *u)
{
	if (!u->o.flags.s.used || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap) return false;
	if (Unit_GetHouseID(u) != LOCAL_HOUSE_ID) return false;
	if (u->o.type == UNIT_CARRYALL || u->o.type == UNIT_ORNITHOPTER || u->o.type == UNIT_FRIGATE) return false;
	if (u->o.type == UNIT_SANDWORM) return false;
	if (u->o.type >= UNIT_MISSILE_HOUSE && u->o.type <= UNIT_MISSILE_TROOPER) return false;
	return true;
}

static void RTS_SelectRect(int x0, int y0, int x1, int y1)
{
	PoolFindStruct find;
	Unit *u;
	int n = 0;
	uint16 group[MAX_GROUP];

	if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
	if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
	y0 -= 40;
	y1 -= 40;

	find.houseID = LOCAL_HOUSE_ID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	while ((u = Unit_Find(&find)) != NULL && n < MAX_GROUP) {
		uint16 ux, uy;
		if (!RTS_Selectable(u)) continue;
		if (!Map_IsPositionInViewport(u->o.position, &ux, &uy)) continue;
		if ((int16)ux < x0 || (int16)ux > x1 || (int16)uy < y0 || (int16)uy > y1) continue;
		group[n++] = u->o.index;
	}
	if (n == 0) return;

	RTS_ClearGroup();
	memcpy(s_group, group, n * sizeof(uint16));
	s_groupCount = n;
	if (g_unitSelected != NULL && g_unitSelected->o.index != s_group[0]) Unit_Select(NULL);
	Unit_Select(Unit_Get_ByIndex(s_group[0]));
	s_lastSelected = g_unitSelected;
	for (n = 0; n < s_groupCount; n++) {
		u = RTS_GroupUnit(n);
		if (u != NULL) Unit_UpdateMap(2, u);	/* redraw with the mark */
	}
	if (s_groupCount > 1) {
		GUI_DisplayText("Squad of %d units.", 2, s_groupCount);
		GUI_Widget_ActionPanel_Draw(true);	/* SQUAD in the sidebar */
	}
}

static bool RTS_CanDo(Unit *u, ActionType action)
{
	const uint16 *a = g_table_unitInfo[u->o.type].o.actionsPlayer;
	int i;
	if (action == ACTION_AMBUSH) action = ACTION_ATTACK;		/* Shift + Attack */
	if (action == ACTION_AREA_GUARD) action = ACTION_GUARD;		/* Shift + Guard */
	for (i = 0; i < 4; i++) {
		if (a[i] == action) return true;
	}
	return false;
}

/* The middle of the group in tiles. */
static void RTS_GroupMiddle(int *cx, int *cy)
{
	int i, n = 0;
	*cx = *cy = 0;
	for (i = 0; i < s_groupCount; i++) {
		Unit *u = RTS_GroupUnit(i);
		if (u == NULL) continue;
		*cx += Tile_GetPosX(u->o.position);
		*cy += Tile_GetPosY(u->o.position);
		n++;
	}
	if (n > 0) {
		*cx /= n;
		*cy /= n;
	}
}

/* Where a unit of the group goes when the group is sent to packed: its place
 * in the group kept, at most 3 tiles from the point. */
static uint16 RTS_FormationDest(Unit *u, uint16 packed, int cx, int cy)
{
	int dx = 0, dy = 0;
	uint16 dest;
	if (s_groupCount > 1) {
		dx = Tile_GetPosX(u->o.position) - cx;
		dy = Tile_GetPosY(u->o.position) - cy;
		dx = max(-3, min(3, dx));
		dy = max(-3, min(3, dy));
	}
	dest = Tile_PackXY(Tile_GetPackedX(packed) + dx, Tile_GetPackedY(packed) + dy);
	if (!Map_IsValidPosition(dest)) dest = packed;
	return dest;
}

/**
 * An action button or a target click of the selected unit given to the rest
 * of its group too (the units that have that action). packed: the target tile
 * of Move / Harvest / Attack, 0xFFFF for an action without a target.
 */
void RTS_GroupAction(Unit *done, uint16 action, uint16 packed)
{
	int i, cx, cy;

	if (s_enabled <= 0 || done != g_unitSelected || RTS_GroupSize() < 2) return;
	RTS_GroupMiddle(&cx, &cy);
	for (i = 0; i < s_groupCount; i++) {
		Unit *u = RTS_GroupUnit(i);
		if (u == NULL || u == done || !RTS_CanDo(u, action)) continue;

		if (packed == 0xFFFF) {
			Lan_UnitOrder(u, action, LAN_ORDER_NONE, 0);
		} else if (action == ACTION_MOVE) {
			Lan_UnitOrder(u, action, LAN_ORDER_DESTINATION, Tools_Index_Encode(RTS_FormationDest(u, packed, cx, cy), IT_TILE));
		} else if (action == ACTION_HARVEST) {
			Lan_UnitOrder(u, action, LAN_ORDER_MOVE_TARGET, Tools_Index_Encode(RTS_FormationDest(u, packed, cx, cy), IT_TILE));
		} else {
			Lan_UnitOrder(u, action, LAN_ORDER_TARGET, Tools_Index_Encode(Unit_FindTargetAround(packed), IT_TILE));
		}
	}
}

static void RTS_Order(int x, int y)
{
	uint16 packed, target;
	int i, cx, cy;
	bool enemy = false;
	Unit *first = NULL;

	packed = Tile_PackXY(x / 16 + Tile_GetPackedX(g_minimapPosition), (y - 40) / 16 + Tile_GetPackedY(g_minimapPosition));
	if (!Map_IsValidPosition(packed)) return;

	target = Unit_FindTargetAround(packed);
	{
		Unit *tu = Unit_Get_ByPackedTile(target);
		Structure *ts = Structure_Get_ByPackedTile(target);
		if (g_map[target].overlayTileID != g_veiledTileID && TILE_SHOWN(target)) {
			if (tu != NULL && !House_AreAllied(Unit_GetHouseID(tu), LOCAL_HOUSE_ID)) enemy = true;
			if (ts != NULL && !House_AreAllied(ts->o.houseID, LOCAL_HOUSE_ID)) enemy = true;
		}
	}

	/* the group's middle, for keeping its shape */
	RTS_GroupMiddle(&cx, &cy);

	for (i = 0; i < s_groupCount; i++) {
		Unit *u = RTS_GroupUnit(i);
		ActionType action;
		uint16 encoded;

		if (u == NULL) continue;
		action = (enemy && RTS_CanDo(u, ACTION_ATTACK)) ? ACTION_ATTACK : ACTION_MOVE;
		if (!RTS_CanDo(u, action)) continue;

		if (action == ACTION_ATTACK) {
			Unit *t;
			Lan_UnitOrder(u, action, LAN_ORDER_TARGET, Tools_Index_Encode(target, IT_TILE));
			t = LAN_GAME ? Unit_Get_ByPackedTile(target) : Tools_Index_GetUnit(u->targetAttack);
			if (t != NULL) t->blinkCounter = 8;
		} else {
			encoded = Tools_Index_Encode(RTS_FormationDest(u, packed, cx, cy), IT_TILE);
			Lan_UnitOrder(u, action, LAN_ORDER_DESTINATION, encoded);
		}
		if (first == NULL) first = u;
	}
	if (first == NULL) return;

	if (g_enableVoices == 0) {
		Driver_Sound_Play(36, 0xFF);
	} else if (g_table_unitInfo[first->o.type].movementType == MOVEMENT_FOOT) {
		Sound_StartSound(g_table_actionInfo[enemy ? ACTION_ATTACK : ACTION_MOVE].soundID);
	} else {
		Sound_StartSound((((LAN_GAME ? (uint8)AmigaTimer_GetMs() : Tools_Random_256()) & 0x1) == 0) ? 20 : 17);
	}
}

#define EDGE_TICKS 6	/* g_timerGUI ticks (60 Hz) per tile of scrolling */

static uint32 s_edgeNext;
static uint16 s_edgeSprite = 0xFFFF;	/* the arrow shown, 0xFFFF none */

/* The edge of the screen under the pointer as a Map_MoveDirection()
 * direction (0 up, 1 up right ... 7 up left), 0xFFFF none. */
static uint16 RTS_EdgeDirection(void)
{
	static const uint16 dirs[3][3] = {	/* [vertical][horizontal]: -1, 0, +1 */
		{ 7, 0, 1 },
		{ 6, 0xFFFF, 2 },
		{ 5, 4, 3 }
	};
	int h = 1, v = 1;
	if (g_mouseX <= 1) h = 0;
	if (g_mouseX >= SCREEN_WIDTH - 2) h = 2;
	if (g_mouseY <= 1) v = 0;
	if (g_mouseY >= SCREEN_HEIGHT - 2) v = 2;
	return dirs[v][h];
}

/** The arrow to show at the edge of the screen, or def (GUI_Widget_Viewport_Click). */
uint16 RTS_EdgeSprite(uint16 def)
{
	return s_edgeSprite != 0xFFFF ? s_edgeSprite : def;
}

/* Scrolling at the edges of the whole screen. */
static void RTS_EdgeScroll(void)
{
	/* the arrows of the game: 1 up, 2 right, 3 down, 4 left; corners: up/down */
	static const uint16 sprites[8] = { 1, 1, 2, 3, 3, 3, 4, 1 };
	static const XYPosition hotSpots[5] = {{0, 0}, {5, 0}, {8, 5}, {5, 8}, {0, 5}};
	uint16 dir = 0xFFFF;

	if (g_selectionType == SELECTIONTYPE_UNIT || g_selectionType == SELECTIONTYPE_STRUCTURE ||
	    g_selectionType == SELECTIONTYPE_TARGET || g_selectionType == SELECTIONTYPE_PLACE) {
		if (Timer_GetTime() - s_lastMain < 100) dir = RTS_EdgeDirection();
	}
	if (dir == 0xFFFF) {
		s_edgeSprite = 0xFFFF;
		return;
	}
	s_edgeSprite = sprites[dir];
	if (g_cursorSpriteID != s_edgeSprite) {
		Sprites_SetMouseSprite(hotSpots[s_edgeSprite].x, hotSpots[s_edgeSprite].y, g_sprites[s_edgeSprite]);
		g_cursorSpriteID = s_edgeSprite;
	}
	if ((int32)(g_timerGUI - s_edgeNext) >= 0) {
		Map_MoveDirection(dir);
		s_edgeNext = g_timerGUI + EDGE_TICKS;
	}
}

static volatile bool s_pauseRequest;
static bool s_paused;

/** P was pressed (video driver's tick). */
void RTS_PauseRequest(void)
{
	s_pauseRequest = true;
}

/* The pause: toggled by P on the map screen; while it lasts the game timer
 * is kept off (a window that turns it back on does not end it). */
static void RTS_Pause(void)
{
	bool onMap = g_gameMode == GM_NORMAL && g_selectionType != SELECTIONTYPE_MENTAT;

	if (s_pauseRequest) {
		s_pauseRequest = false;
		if (g_lanGame) return;	/* not in a LAN game */
		if (onMap || s_paused) {
			s_paused = !s_paused;
			Timer_SetTimer(TIMER_GAME, !s_paused);
			if (s_paused) {
				GUI_DisplayText("Game paused - press P to continue.", 3);
			} else {
				GUI_DisplayText("Game continues.", 3);
			}
		}
	}
	if (s_paused) {
		if (!onMap) {
			/* the mission ended or the game was left: no pause any more */
			s_paused = false;
			return;
		}
		Timer_SetTimer(TIMER_GAME, false);
	}
}

void RTS_Tick(void)
{
	if (s_enabled < 0) s_enabled = IniFile_GetInteger("rts_controls", 1) != 0 ? 1 : 0;
	s_lastMain = Timer_GetTime();
	RTS_Pause();		/* also with rts_controls=0 */
	if (s_enabled == 0) return;

	if (g_gameMode != GM_NORMAL) {
		s_groupCount = 0;
		s_selectPending = s_orderPending = false;
		s_edgeSprite = 0xFFFF;
		return;
	}
	RTS_EdgeScroll();

	/* a plain click on the viewport ends a group: from now on only what was
	 * clicked is selected (the click itself is handled by the game later, the
	 * selection below follows it) */
	if (s_plainClick) {
		bool wasGroup = s_groupCount > 1;
		s_plainClick = false;
		RTS_ClearGroup();
		s_lastSelected = NULL;
		if (wasGroup && g_unitSelected != NULL) GUI_Widget_ActionPanel_Draw(true);	/* no SQUAD any more */
	}

	/* the selection changed: the selected unit is the group, none clears it
	 * (a group from a rectangle changes it itself) */
	if (g_unitSelected != s_lastSelected) {
		s_lastSelected = g_unitSelected;
		RTS_ClearGroup();
		if (g_unitSelected != NULL && Unit_GetHouseID(g_unitSelected) == LOCAL_HOUSE_ID) {
			s_group[0] = g_unitSelected->o.index;
			s_groupCount = 1;
		}
	}

	if (s_selectPending) {
		s_selectPending = false;
		if (g_selectionType == SELECTIONTYPE_UNIT || g_selectionType == SELECTIONTYPE_STRUCTURE) {
			RTS_SelectRect(s_sx0, s_sy0, s_sx1, s_sy1);
		}
	}
	if (s_orderPending) {
		s_orderPending = false;
		if (s_groupCount > 0 && (g_selectionType == SELECTIONTYPE_UNIT || g_selectionType == SELECTIONTYPE_STRUCTURE)) {
			RTS_Order(s_ox, s_oy);
		}
	}
}

/** For amiga_cmd.c's state: units in the group. */
int RTS_GroupCount(void)
{
	return s_groupCount;
}

/** For amiga_cmd.c's state: the selected unit's index (-1 none). */
int RTS_SelectedUnit(void)
{
	return g_unitSelected != NULL ? (int)g_unitSelected->o.index : -1;
}

/** For amiga_cmd.c's state: g_selectionType. */
int RTS_SelectionType(void)
{
	return (int)g_selectionType;
}

/** Units in the selected group, 0 when the selected unit is not in one. */
int RTS_GroupSize(void)
{
	int i, n = 0;
	if (s_enabled <= 0 || g_unitSelected == NULL || !RTS_InGroupAny(g_unitSelected)) return 0;
	for (i = 0; i < s_groupCount; i++) {
		if (RTS_GroupUnit(i) != NULL) n++;
	}
	return n;
}

/** The group's average health in percent, for the sidebar's DMG bar. */
uint16 RTS_GroupHealthPercent(void)
{
	uint32 sum = 0;
	int i, n = 0;
	for (i = 0; i < s_groupCount; i++) {
		Unit *u = RTS_GroupUnit(i);
		uint16 max;
		if (u == NULL) continue;
		max = g_table_unitInfo[u->o.type].o.hitpoints;
		if (max == 0) max = 1;
		sum += (uint32)u->o.hitpoints * 100 / max;
		n++;
	}
	return n > 0 ? (uint16)(sum / n) : 0;
}

/* A unit's picture without its 1-pixel frame (30x22) at x, y, through the
 * scratch screen (colour 0 is transparent in the sprites). */
static void RTS_DrawUnitPicture(uint8 type, int16 x, int16 y)
{
	uint8 *dst = GFX_Screen_GetActive();
	uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_3);	/* scratch: credits and intro only */
	int xx, yy;

	for (yy = 0; yy < 24; yy++) memset(src + yy * SCREEN_WIDTH, 0, 32);
	GUI_DrawSprite(SCREEN_3, g_sprites[g_table_unitInfo[type].o.spriteID], 0, 0, 0, 0);
	for (yy = 0; yy < 22; yy++) {
		for (xx = 0; xx < 30; xx++) {
			uint8 c = src[(yy + 1) * SCREEN_WIDTH + xx + 1];
			if (c != 0) dst[(y + yy) * SCREEN_WIDTH + x + xx] = c;
		}
	}
}

/**
 * The action panel of a squad (rts_amiga.c groups): under the name one
 * picture per unit type, two per row, with the number of units of that type
 * and their average health as a line; under them the group's damage bar (the
 * average) across the panel. x is the panel's left, y the top of the list.
 * Returns where the action buttons go.
 */
int16 RTS_DrawSquadList(int16 x, int16 y)
{
	static const uint8 colours[3] = { 4, 5, 8 };	/* green, yellow, red as GUI_DrawProgressbar */
	uint8 types[MAX_GROUP];
	uint16 count[MAX_GROUP], health[MAX_GROUP];	/* units, sum of health in % */
	int i, j, n = 0, rows, shown;
	int16 bottom = g_widgetProperties[6].yBase + g_widgetProperties[6].height;
	int16 barY;

	for (i = 0; i < s_groupCount; i++) {
		Unit *u = RTS_GroupUnit(i);
		uint16 max;
		if (u == NULL) continue;
		max = g_table_unitInfo[u->o.type].o.hitpoints;
		if (max == 0) max = 1;
		for (j = 0; j < n; j++) if (types[j] == u->o.type) break;
		if (j == n) {
			types[n] = u->o.type;
			count[n] = 0;
			health[n] = 0;
			n++;
		}
		count[j]++;
		health[j] += (uint16)((uint32)min(u->o.hitpoints, max) * 100 / max);
	}

	/* rows that fit above the bar (12) and the four buttons (44) */
	rows = (bottom - 2 - 44 - 12 - y) / 24;
	if (rows < 1) rows = 1;
	shown = min(n, rows * 2);

	for (i = 0; i < shown; i++) {
		int16 cx = x + 2 + (i & 1) * 31, cy = y + (i >> 1) * 24;
		uint16 avg = health[i] / count[i];
		uint8 colour = colours[0];

		RTS_DrawUnitPicture(types[i], cx, cy);
		if (count[i] > 1) {
			/* white with a black shadow, readable on any picture */
			GUI_DrawText_Wrapper("%d", cx + 2, cy + 16, 12, 0, 0x11, count[i]);
			GUI_DrawText_Wrapper("%d", cx + 1, cy + 15, 15, 0, 0x11, count[i]);
		}
		if (avg <= 50) colour = colours[1];
		if (avg <= 25) colour = colours[2];
		if (avg * 30 / 100 > 0) GUI_DrawLine(cx, cy + 22, cx + avg * 30 / 100 - 1, cy + 22, colour);
	}
	GFX_Screen_SetDirty(SCREEN_ACTIVE, x, y, x + 64, y + ((shown + 1) / 2) * 24);

	/* the damage bar across the panel */
	barY = y + ((shown + 1) / 2) * 24 + 2;
	g_progressbarX = x + 3;
	g_progressbarY = barY;
	g_progressbarW = 58;
	GUI_DrawProgressbar(RTS_GroupHealthPercent(), 100);
	g_progressbarY = -1;
	return barY + 11;
}

/** The four action buttons under a squad list (y), or at their place (y < 0). */
void RTS_PlaceSquadButtons(int16 y)
{
	const WidgetInfo *info;
	for (info = g_table_gameWidgetInfo; info->index >= 0; info++) {
		Widget *w;
		if (info->index < 8 || info->index > 11) continue;
		w = GUI_Widget_Get_ByIndex(g_widgetLinkedListHead, info->index);
		if (w == NULL) continue;
		w->offsetY = (y < 0) ? info->offsetY + PANEL_DY : y + (info->index - 8) * 11;
	}
}

/**
 * Test command "spawn" (amiga_cmd.c): a unit of the type for the house at the
 * viewport point x, y (game pixels), sent to dx, dy - e.g. an ornithopter
 * across the screen for the leftover check (tools/ghost_check.py).
 */
bool RTS_TestSpawn(uint8 type, uint8 house, int16 x, int16 y, int16 dx, int16 dy)
{
	uint16 from, to;
	Unit *u;

	from = Tile_PackXY(x / 16 + Tile_GetPackedX(g_minimapPosition), (y - 40) / 16 + Tile_GetPackedY(g_minimapPosition));
	to = Tile_PackXY(dx / 16 + Tile_GetPackedX(g_minimapPosition), (dy - 40) / 16 + Tile_GetPackedY(g_minimapPosition));
	g_validateStrictIfZero++;
	u = Unit_Create(UNIT_INDEX_INVALID, type, house, Tile_UnpackTile(from), 0);
	g_validateStrictIfZero--;
	if (u == NULL) return false;
	Unit_SetAction(u, ACTION_MOVE);
	Unit_SetDestination(u, Tools_Index_Encode(to, IT_TILE));
	return true;
}
