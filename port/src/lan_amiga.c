/** @file src/lan_amiga.c A LAN game of up to three players (Amiga, after the Settlers port). */

/*
 * Lockstep, as in the Settlers port: every Amiga runs the same simulation
 * and the players' orders go over the network instead of the game's state.
 *
 * - The players: the server (player 0) and up to two that joined it; each
 *   plays one of Atreides, Harkonnen and Ordos, no house of the computer.
 * - The simulation is the same on each Amiga: g_playerHouseID is the server's
 *   house everywhere (the original game's "player" in the simulation: fog,
 *   counting units), each Amiga sees and controls its own house
 *   (g_lanLocalHouse, LOCAL_HOUSE_ID); House_IsHuman() tells a person's
 *   house from the computer's. No fog of war in a LAN game.
 * - Time: the game timer (g_timerGame) only moves in Lan_Step(), 60 steps a
 *   second; a turn is LAN_TURN_STEPS steps. An order given in turn T is
 *   carried out at the start of turn T + delay on every Amiga; the delay
 *   (turns) comes from the measured ping.
 * - The joiners send their orders of a turn to the server (TURN); the server,
 *   once it has every player's orders of turn T, sends them all together in
 *   the same order (TURNALL) and carries them out; a joiner carries out turn T
 *   when its TURNALL has come. A turn that has not come yet stops the game
 *   (WAITING); the picture and the mouse go on.
 * - Every 5 seconds each Amiga sends a sum of the game's state (units,
 *   structures, houses, random generators); a different sum: OUT OF STEP.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "os/math.h"
#include "os/net_amiga.h"
#include "os/amiga_prof.h"

#include "lan_amiga.h"
#include "animation.h"
#include "explosion.h"
#include "gui/gui.h"
#include "gui/widget.h"
#include "gui/font.h"
#include "gui/layout_amiga.h"
#include "house.h"
#include "map.h"
#include "opendune.h"
#include "pool/house.h"
#include "pool/pool.h"
#include "pool/structure.h"
#include "pool/unit.h"
#include "scenario.h"
#include "save.h"
#include "load.h"
#include "file.h"
#include "opendune.h"
#include "sprites.h"
#include "string.h"
#include "structure.h"
#include "audio/sound.h"
#include "table/strings.h"
#include "team.h"
#include "tile.h"
#include "timer.h"
#include "tools.h"
#include "unit.h"
#include "script/script.h"
#include "config.h"
#include "inifile.h"

#include "lan_maps.h"
#define LAN_LEVELS ((int)(sizeof(s_lanLevels) / sizeof(s_lanLevels[0])))
/* level 1 (mission 1) has no barracks or factory: nobody could win a fight */
#define LAN_LEVEL_MIN 2

extern uint32 AmigaTimer_GetMs(void);
extern uint8 g_validateHouseID;
extern void GameLoop_Team(void);
extern void GameLoop_Unit(void);
extern void GameLoop_Structure(void);
extern void GameLoop_House(void);
extern void Unit_ResetTicks(void);
extern void Structure_ResetTicks(void);
extern void House_ResetTicks(void);
extern void Team_ResetTicks(void);
extern void Explosion_ResetTicks(void);
extern void Animation_ResetTicks(void);

bool g_lanGame = false;
bool g_lanSim = false;
int g_lanUI = 0;
int g_lanModal = 0;
uint8 g_lanLocalHouse = HOUSE_INVALID;
uint16 g_lanSimViewPos = 0;

/* ---- the players (lobby and game) */

LanPlayer g_lanPlayers[LAN_MAX_PLAYERS];
int g_lanMe = 0;		/* my slot: 0 server */
int g_lanMap = 0;
int g_lanLevel = LAN_LEVEL_MIN;	/* LAN_LEVEL_MIN..LAN_LEVELS: the start and the knowledge of that mission */
bool g_lanServer = false;

static const uint8 s_lanHouses[3] = { HOUSE_ATREIDES, HOUSE_HARKONNEN, HOUSE_ORDOS };

int Lan_LevelCount(void) { return LAN_LEVELS; }
int Lan_LevelMin(void) { return LAN_LEVEL_MIN; }

int Lan_MapCount(void) { return (int)(sizeof(s_lanMaps) / sizeof(s_lanMaps[0])); }

/** How many players a map has places for (2 or 3). */
int Lan_MapPlayers(int i)
{
	return i >= 0 && i < Lan_MapCount() ? s_lanMaps[i].players : 0;
}

const char *Lan_MapName(int i)
{
	static char name[32];
	if (i < 0 || i >= Lan_MapCount()) return "";
	/* "SCENA020" -> "Arrakis 3 (A020) 3P" by its place in the list */
	snprintf(name, sizeof(name), "Arrakis %d (%s) %dP", i + 1, s_lanMaps[i].name + 4, s_lanMaps[i].players);
	return name;
}

/* ---- commands: everything a player does to the game goes through these */

enum {
	CMD_UNIT_ORDER = 1,	/* a unit, action, mode, target */
	CMD_DEVIATE,		/* a unit: deviation - 5 (its action button) */
	CMD_BUILD,		/* a factory starts building an object type */
	CMD_UPGRADE,		/* a factory starts upgrading */
	CMD_REPAIR_UPGRADE,	/* the Repair / Upgrade button (toggle) */
	CMD_RESUME,		/* "On hold" clicked: building goes on */
	CMD_HOLD,		/* "Done" clicked while building: on hold */
	CMD_PLACE		/* the Construction Yard's finished structure placed at a tile */
};

#define CMD_SIZE 10		/* type, house, a, b, c, d (big endian) */
#define LAN_TURN_STEPS 6	/* 100 ms */
#define LAN_SUM_STEPS 300	/* a sum every 5 s */
#define LAN_RING 16		/* turns kept */
#define LAN_MAX_CMDS 64		/* per player and turn */

typedef struct TurnBuf {
	int32 turn;		/* -1: none */
	bool complete;
	int count;		/* commands */
	uint8 data[LAN_MAX_CMDS * LAN_MAX_PLAYERS * CMD_SIZE];
} TurnBuf;

static uint8 s_pending[LAN_MAX_CMDS * CMD_SIZE];
static int s_pendingCount;
/* the server: each joiner's turns; a joiner: the turns of all (TURNALL) */
static TurnBuf s_turns[LAN_MAX_PLAYERS][LAN_RING];
/* the server: its own turns (kept until carried out) */
static TurnBuf s_ownTurns[LAN_RING];

static uint32 s_steps;		/* steps done */
static uint32 s_stepBase;	/* ms of step 0 */
static int s_delay = 3;		/* turns */
static int32 s_turnBase;	/* the first turn of this game (a loaded game: not 0) */
static uint8 s_seen[4096 / 8];	/* this player's fog (below) */
static bool s_waiting;
static uint32 s_waitSince;
static bool s_inStep;
static bool s_outOfStep;
static uint32 s_outOfStepAt;			/* the step where the sums first differed */
static uint32 s_lastSame[LAN_MAX_PLAYERS];	/* the last step the sums agreed, by player */
static bool s_lanInfo;				/* ini lan_info=1: the LAN line always */
static uint32 s_sums[8], s_sumSteps[8];	/* own sums of the last checks */
static uint16 s_noSilo[HOUSE_MAX];
static uint8 s_savedSpeed, s_savedHints;
static bool s_ended;
static bool s_leave;		/* the player quit (Options) */
static bool s_trace;		/* opendune.ini debug_lan_trace=1: the random state of each first step */

enum { MT_LOBBY = 1, MT_PICK, MT_START, MT_TURN, MT_TURNALL, MT_SUM, MT_QUIT, MT_GONE, MT_MAP, MT_CHAT, MT_PAUSE,
       MT_SEENREQ, MT_SEEN, MT_FILE, MT_FILEEND, MT_FILEOK };

static bool s_logNew = true;

/* RAM:OpenDUNE-lan.txt, opened for each line (another program can read it meanwhile) */
static void Lan_Log(const char *fmt, ...)
{
	va_list ap;
	FILE *f = fopen("RAM:OpenDUNE-lan.txt", s_logNew ? "w" : "a");
	if (f == NULL) return;
	s_logNew = false;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fclose(f);
}

static void Put16(uint8 *p, uint16 v) { p[0] = (uint8)(v >> 8); p[1] = (uint8)v; }
static uint16 Get16(const uint8 *p) { return (uint16)((p[0] << 8) | p[1]); }
static void Put32(uint8 *p, uint32 v) { p[0] = (uint8)(v >> 24); p[1] = (uint8)(v >> 16); p[2] = (uint8)(v >> 8); p[3] = (uint8)v; }
static uint32 Get32(const uint8 *p) { return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3]; }

static void Lan_Queue(uint8 type, uint16 a, uint16 b, uint16 c, uint16 d)
{
	uint8 *p;
	if (s_pendingCount >= LAN_MAX_CMDS) return;
	p = s_pending + s_pendingCount * CMD_SIZE;
	p[0] = type;
	p[1] = g_lanLocalHouse;
	Put16(p + 2, a);
	Put16(p + 4, b);
	Put16(p + 6, c);
	Put16(p + 8, d);
	s_pendingCount++;
}

/* ---- carrying out the commands (the same on every Amiga) */

static void Lan_DoUnitOrder(Unit *u, uint16 action, uint8 mode, uint16 encoded)
{
	Object_Script_Variable4_Clear(&u->o);
	u->targetAttack = 0;
	u->targetMove = 0;
	u->route[0] = 0xFF;
	Unit_SetAction(u, (ActionType)action);
	switch (mode) {
		case LAN_ORDER_DESTINATION: Unit_SetDestination(u, encoded); break;
		case LAN_ORDER_MOVE_TARGET: u->targetMove = encoded; break;
		case LAN_ORDER_TARGET: Unit_SetTarget(u, encoded); break;
		default: break;
	}
}

static Unit *Lan_OwnUnit(uint16 index, uint8 house)
{
	Unit *u;
	if (index >= UNIT_INDEX_MAX) return NULL;
	u = Unit_Get_ByIndex(index);
	if (!u->o.flags.s.used || !u->o.flags.s.allocated) return NULL;
	if (Unit_GetHouseID(u) != house) return NULL;
	return u;
}

static Structure *Lan_OwnStructure(uint16 index, uint8 house)
{
	Structure *s;
	if (index >= STRUCTURE_INDEX_MAX_SOFT) return NULL;
	s = Structure_Get_ByIndex(index);
	if (!s->o.flags.s.used || s->o.houseID != house) return NULL;
	return s;
}

static void Lan_Place(Structure *cy, uint16 position)
{
	Structure *ns;
	House *h = House_Get_ByIndex(cy->o.houseID);
	bool local = cy->o.houseID == g_lanLocalHouse;

	if (cy->o.type != STRUCTURE_CONSTRUCTION_YARD || cy->o.linkedID == 0xFF) return;
	ns = Structure_Get_ByIndex(cy->o.linkedID);
	cy->o.linkedID = STRUCTURE_INVALID;

	if (!Structure_Place(ns, position)) {
		cy->o.linkedID = ns->o.index & 0xFF;	/* back: place it again */
		if (local) GUI_DisplayText(String_Get_ByIndex(STR_CAN_NOT_PLACE_S_HERE), 2, String_Get_ByIndex(g_table_structureInfo[ns->o.type].o.stringID_abbrev));
		return;
	}
	if (local) Voice_Play(20);
	if (ns->o.type == STRUCTURE_PALACE) h->palacePosition = ns->o.position;
	if (ns->o.type == STRUCTURE_REFINERY) {
		Unit *u;
		g_validateStrictIfZero++;
		u = Unit_CreateWrapper(cy->o.houseID, UNIT_HARVESTER, Tools_Index_Encode(ns->o.index, IT_STRUCTURE));
		g_validateStrictIfZero--;
		if (u == NULL) {
			h->harvestersIncoming++;
		} else {
			u->originEncoded = Tools_Index_Encode(ns->o.index, IT_STRUCTURE);
		}
	}
	if ((Structure_GetBuildable(cy) & (1 << cy->objectType)) == 0) Structure_BuildObject(cy, 0xFFFE);
	if (local) {
		House_UpdateRadarState(h);
		if (h->powerProduction < h->powerUsage && (h->structuresBuilt & (1 << STRUCTURE_OUTPOST)) != 0) {
			GUI_DisplayText(String_Get_ByIndex(STR_NOT_ENOUGH_POWER_FOR_RADAR_BUILD_WINDTRAPS), 3);
		}
	}
}

static void Lan_Apply(const uint8 *p)
{
	uint8 type = p[0], house = p[1];
	uint16 a = Get16(p + 2), b = Get16(p + 4), c = Get16(p + 6), d = Get16(p + 8);
	Unit *u;
	Structure *s;

	switch (type) {
		case CMD_UNIT_ORDER:
			u = Lan_OwnUnit(a, house);
			if (u != NULL) Lan_DoUnitOrder(u, b, (uint8)c, d);
			break;
		case CMD_DEVIATE:
			u = Lan_OwnUnit(a, house);
			if (u != NULL && u->deviated != 0) Unit_Deviation_Decrease(u, 5);
			break;
		case CMD_BUILD:
			s = Lan_OwnStructure(a, house);
			if (s != NULL) Structure_BuildObject(s, b);
			break;
		case CMD_UPGRADE:
			s = Lan_OwnStructure(a, house);
			if (s != NULL) {
				Structure_SetRepairingState(s, 0, NULL);
				Structure_SetUpgradingState(s, 1, NULL);
			}
			break;
		case CMD_REPAIR_UPGRADE:
			s = Lan_OwnStructure(a, house);
			if (s != NULL && !Structure_SetRepairingState(s, -1, NULL)) Structure_SetUpgradingState(s, -1, NULL);
			break;
		case CMD_RESUME:
			s = Lan_OwnStructure(a, house);
			if (s != NULL) {
				s->o.flags.s.repairing = false;
				s->o.flags.s.onHold = false;
				s->o.flags.s.upgrading = false;
			}
			break;
		case CMD_HOLD:
			s = Lan_OwnStructure(a, house);
			if (s != NULL) s->o.flags.s.onHold = true;
			break;
		case CMD_PLACE:
			s = Lan_OwnStructure(a, house);
			if (s != NULL) Lan_Place(s, b);
			break;
		default: break;
	}
	(void)d;
}

/* ---- the game's calls (each applies at once outside a LAN game) */

void Lan_UnitOrder(Unit *u, uint16 action, uint8 mode, uint16 encoded)
{
	if (u == NULL) return;
	if (!g_lanGame) {
		Lan_DoUnitOrder(u, action, mode, encoded);
		return;
	}
	if (Unit_GetHouseID(u) != g_lanLocalHouse) return;
	Lan_Queue(CMD_UNIT_ORDER, u->o.index, action, mode, encoded);
}

void Lan_UnitDeviate(Unit *u)
{
	if (u == NULL) return;
	if (!g_lanGame) {
		Unit_Deviation_Decrease(u, 5);
		return;
	}
	Lan_Queue(CMD_DEVIATE, u->o.index, 0, 0, 0);
}

void Lan_StructureCommand(int command, Structure *s, uint16 arg)
{
	if (s == NULL || s->o.houseID != g_lanLocalHouse) return;
	switch (command) {
		case LAN_STRUCTURE_BUILD: Lan_Queue(CMD_BUILD, s->o.index, arg, 0, 0); break;
		case LAN_STRUCTURE_UPGRADE: Lan_Queue(CMD_UPGRADE, s->o.index, 0, 0, 0); break;
		case LAN_STRUCTURE_REPAIR_UPGRADE: Lan_Queue(CMD_REPAIR_UPGRADE, s->o.index, 0, 0, 0); break;
		case LAN_STRUCTURE_RESUME: Lan_Queue(CMD_RESUME, s->o.index, 0, 0, 0); break;
		case LAN_STRUCTURE_HOLD: Lan_Queue(CMD_HOLD, s->o.index, 0, 0, 0); break;
		case LAN_STRUCTURE_PLACE: Lan_Queue(CMD_PLACE, s->o.index, arg, 0, 0); break;
		default: break;
	}
}

/* ---- credits: each person's house as the original player's */

void Lan_HouseCredits(House *h)
{
	uint16 maxCredits;
	if (!h->flags.human) {
		if (h->creditsStorage < h->credits) h->credits = h->creditsStorage;
		return;
	}
	maxCredits = max(h->creditsStorage, s_noSilo[h->index]);
	if (h->credits > maxCredits) {
		h->credits = maxCredits;
		if (h->index == g_lanLocalHouse) GUI_DisplayText(String_Get_ByIndex(STR_INSUFFICIENT_SPICE_STORAGE_AVAILABLE_SPICE_IS_LOST), 1);
	}
	if (h->creditsStorage > s_noSilo[h->index]) s_noSilo[h->index] = 0;
	if (h->index == g_lanLocalHouse && s_noSilo[h->index] == 0 && h->credits != 0 && h->creditsStorage != 0 &&
	    (h->credits * 256 / h->creditsStorage) > 200) {
		GUI_DisplayText(String_Get_ByIndex(STR_SPICE_STORAGE_CAPACITY_LOW_BUILD_SILOS), 0);
	}
}

/* tests: who draws the simulation's random numbers (the caller's address) */
void Lan_TraceRandom(void *caller)
{
	Lan_Log("rnd t%lu %08lx\n", (unsigned long)g_timerGame, (unsigned long)caller);
}

/* tests: an event with the caller, in the trace window (or always for "-ui" ones) */
void Lan_TraceTag(const char *tag, uint32 value, void *caller)
{
	extern uint32 g_lanTraceFrom, g_lanTraceTo;
	if (!g_lanGame) return;
	if (strstr(tag, "-ui") == NULL && (g_timerGame < g_lanTraceFrom || g_timerGame > g_lanTraceTo)) return;
	Lan_Log("%s t%lu %lu %08lx sim%d ui%d modal%d step%d\n", tag, (unsigned long)g_timerGame, (unsigned long)value, (unsigned long)caller,
	        (int)g_lanSim, g_lanUI, g_lanModal, (int)s_inStep);
}

/* ---- the scenario of a LAN game: a map's landscape, a start for each player */

static char s_scenario[2048];

static const LanLevel *Lan_Level(uint8 house)
{
	int level = g_lanLevel < LAN_LEVEL_MIN ? LAN_LEVEL_MIN : g_lanLevel > LAN_LEVELS ? LAN_LEVELS : g_lanLevel;
	return &s_lanLevels[level - 1][house < 3 ? house : 0];
}

/* Can a unit of this type start on the tile? Not on a structure or another
 * starting unit, and on ground it drives (walks) on; rock first. */
static bool Lan_StartTileFree(uint16 packed, uint8 type, const uint16 *used, int usedCount, bool rockOnly)
{
	uint16 lst;
	int i;
	if (!Map_IsValidPosition(packed) || g_map[packed].hasStructure) return false;
	for (i = 0; i < usedCount; i++) if (used[i] == packed) return false;
	lst = Map_GetLandscapeType(packed);
	if (g_table_landscapeInfo[lst].movementSpeed[g_table_unitInfo[type].movementType] == 0) return false;
	if (rockOnly && g_table_landscapeInfo[lst].isSand) return false;
	return true;
}

/**
 * The starting units of every player (the level's), around its Construction
 * Yard: ring after ring (from the second ring, the first stays free for
 * building), rock before sand. In the simulation, the same on every Amiga.
 */
static void Lan_PlaceStartUnits(void)
{
	const LanMap *m = &s_lanMaps[g_lanMap];
	int slot;
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		const LanLevel *lv;
		uint16 used[LAN_LEVEL_UNITS];
		int k, n = 0;
		int x0, y0;
		if (!g_lanPlayers[slot].used) continue;
		lv = Lan_Level(g_lanPlayers[slot].house);
		x0 = Tile_GetPackedX(m->cy[slot]);
		y0 = Tile_GetPackedY(m->cy[slot]);
		for (k = 0; k < lv->count; k++) {
			uint8 type = lv->units[k];
			uint16 found = 0xFFFF;
			int pass, r, x, y;
			for (pass = 0; pass < 2 && found == 0xFFFF; pass++) {
				/* the yard is x0..x0+1, y0..y0+1; ring r around it: -r..1+r */
				for (r = 2; r <= (pass == 0 ? 6 : 12) && found == 0xFFFF; r++) {
					for (y = -r; y <= 1 + r && found == 0xFFFF; y++) {
						for (x = -r; x <= 1 + r && found == 0xFFFF; x++) {
							if (x != -r && x != 1 + r && y != -r && y != 1 + r) continue;
							if (x0 + x < 0 || x0 + x >= 64 || y0 + y < 0 || y0 + y >= 64) continue;
							if (Lan_StartTileFree(Tile_PackXY(x0 + x, y0 + y), type, used, n, pass == 0)) found = Tile_PackXY(x0 + x, y0 + y);
						}
					}
				}
			}
			if (found == 0xFFFF) continue;
			used[n++] = found;
			{
				char line[64];
				snprintf(line, sizeof(line), "%s,%s,256,%u,64,Guard", g_table_houseInfo[g_lanPlayers[slot].house].name,
				         g_table_unitInfo[type].o.name, found);
				Scenario_AddUnit(line);
			}
		}
	}
}

const char *Lan_ScenarioText(void)
{
	const LanMap *m = &s_lanMaps[g_lanMap];
	char *p = s_scenario;
	char *end = s_scenario + sizeof(s_scenario);
	int slot;
	int serverStart = 0;

	p += snprintf(p, end - p, "[BASIC]\nLosePicture=LOSTBILD.WSA\nWinPicture=WIN1.WSA\nBriefPicture=HARVEST.WSA\n"
	              "TimeOut=0\nMapScale=0\nLoseFlags=1\nWinFlags=3\n");
	p += snprintf(p, end - p, "CursorPos=%u\nTacticalPos=%u\n", m->cy[serverStart], (unsigned)(m->cy[serverStart] - 64 * 5 - 8));
	p += snprintf(p, end - p, "[MAP]\nSeed=%u\n", m->seed);
	if (m->field[0] != '\0') p += snprintf(p, end - p, "Field=%s\n", m->field);
	if (m->bloom[0] != '\0') p += snprintf(p, end - p, "Bloom=%s\n", m->bloom);
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		if (!g_lanPlayers[slot].used) continue;
		p += snprintf(p, end - p, "[%s]\nQuota=0\nCredits=%u\nBrain=Human\nMaxUnit=30\n", g_table_houseInfo[g_lanPlayers[slot].house].name,
		              (unsigned)Lan_Level(g_lanPlayers[slot].house)->credits);
	}
	p += snprintf(p, end - p, "[STRUCTURES]\n");
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		if (!g_lanPlayers[slot].used) continue;
		p += snprintf(p, end - p, "ID%03d=%s,Const Yard,256,%u\n", slot, g_table_houseInfo[g_lanPlayers[slot].house].name, m->cy[slot]);
	}
	/* the units: Lan_PlaceStartUnits(), when the landscape is there */
	return s_scenario;
}

/* ---- the checksum of the game's state (FNV-1a) */

static uint32 Lan_Fnv(uint32 h, uint32 v)
{
	int i;
	for (i = 0; i < 4; i++) {
		h ^= (v >> (i * 8)) & 0xFF;
		h *= 16777619UL;
	}
	return h;
}

extern uint32 Tools_Random_State(void);

/* the state the sum is made of, in the log (to compare the Amigas' logs) */
static void Lan_DumpState(void)
{
	FILE *f = fopen("RAM:OpenDUNE-lan.txt", "a");
	PoolFindStruct find;
	int i;
	if (f == NULL) return;
	fprintf(f, "T %lu R %08lx\n", (unsigned long)g_timerGame, (unsigned long)Tools_Random_State());
	for (i = 0; i < UNIT_INDEX_MAX; i++) {
		Unit *u = Unit_Get_ByIndex(i);
		if (!u->o.flags.s.used) continue;
		fprintf(f, "U %d t%d h%d %u,%u hp%u a%d o%d m%u ta%u\n", i, u->o.type, u->o.houseID, u->o.position.x, u->o.position.y,
		        u->o.hitpoints, u->actionID, u->orientation[0].current, u->targetMove, u->targetAttack);
	}
	for (i = 0; i < STRUCTURE_INDEX_MAX_HARD; i++) {
		Structure *s = Structure_Get_ByIndex(i);
		if (!s->o.flags.s.used) continue;
		fprintf(f, "S %d t%d h%d hp%u st%d cd%u ot%u\n", i, s->o.type, s->o.houseID, s->o.hitpoints, s->state, s->countDown, s->objectType);
	}
	find.houseID = HOUSE_INVALID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	for (;;) {
		House *h = House_Find(&find);
		if (h == NULL) break;
		fprintf(f, "H %d cr%u\n", h->index, h->credits);
	}
	fclose(f);
}

static uint32 Lan_Checksum(void)
{
	uint32 h = 2166136261UL;
	PoolFindStruct find;
	Unit *u;
	Structure *s;
	int i;

	h = Lan_Fnv(h, g_timerGame);
	h = Lan_Fnv(h, Tools_Random_State());
	for (i = 0; i < UNIT_INDEX_MAX; i++) {
		u = Unit_Get_ByIndex(i);
		if (!u->o.flags.s.used) continue;
		h = Lan_Fnv(h, ((uint32)i << 16) | (u->o.type << 8) | u->o.houseID);
		h = Lan_Fnv(h, ((uint32)u->o.position.x << 16) | u->o.position.y);
		h = Lan_Fnv(h, ((uint32)u->o.hitpoints << 16) | ((uint32)u->actionID << 8) | u->orientation[0].current);
		h = Lan_Fnv(h, ((uint32)u->targetMove << 16) | u->targetAttack);
	}
	for (i = 0; i < STRUCTURE_INDEX_MAX_HARD; i++) {
		s = Structure_Get_ByIndex(i);
		if (!s->o.flags.s.used) continue;
		h = Lan_Fnv(h, ((uint32)i << 16) | (s->o.type << 8) | s->o.houseID);
		h = Lan_Fnv(h, ((uint32)s->o.hitpoints << 16) | s->state);
		h = Lan_Fnv(h, ((uint32)s->countDown << 16) | s->objectType);
	}
	find.houseID = HOUSE_INVALID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	for (;;) {
		House *hs = House_Find(&find);
		if (hs == NULL) break;
		h = Lan_Fnv(h, ((uint32)hs->index << 16) | hs->credits);
	}
	return h;
}

/* ---- network: sending turns and sums */

static void Lan_SendCmds(int peer, int type, int32 turn, const uint8 *data, int count)
{
	uint8 buf[NET_MAX_DATA];
	const int per = (NET_MAX_DATA - 5) / CMD_SIZE;
	int done = 0;
	do {
		int n = count - done;
		if (n > per) n = per;
		Put32(buf, (uint32)turn);
		buf[4] = (uint8)(done + n < count);	/* more frames of this turn follow */
		memcpy(buf + 5, data + done * CMD_SIZE, n * CMD_SIZE);
		{
			int tries = 0;
			while (!Net_Send(peer, type, buf, 5 + n * CMD_SIZE) && ++tries < 200) AmigaTimer_WaitMs(5);
		}
		done += n;
	} while (done < count);
}

static void Lan_TakeTurnFrame(TurnBuf *ring, const uint8 *d, int len)
{
	int32 turn;
	TurnBuf *t;
	int n;
	if (len < 5) return;
	turn = (int32)Get32(d);
	t = &ring[turn % LAN_RING];
	if (t->turn != turn) {
		t->turn = turn;
		t->complete = false;
		t->count = 0;
	}
	n = (len - 5) / CMD_SIZE;
	if (t->count + n > (int)(sizeof(t->data) / CMD_SIZE)) n = (int)(sizeof(t->data) / CMD_SIZE) - t->count;
	memcpy(t->data + t->count * CMD_SIZE, d + 5, n * CMD_SIZE);
	t->count += n;
	if (d[4] == 0) t->complete = true;
}

/* the others' sums until ours of the same step is there */
static uint32 s_otherSums[LAN_MAX_PLAYERS][8], s_otherSteps[LAN_MAX_PLAYERS][8];

static void Lan_CompareSum(int slot, uint32 step, uint32 mine, uint32 sum)
{
	if (mine != sum) {
		if (s_outOfStep) {
			Lan_Log("step %lu: %s different %08lx\n", (unsigned long)step, g_lanPlayers[slot].name, (unsigned long)sum);
			return;
		}
		s_outOfStep = true;
		s_outOfStepAt = step;
		Lan_Log("OUT OF STEP at %lu: mine %08lx, %s %08lx\n", (unsigned long)step, (unsigned long)mine, g_lanPlayers[slot].name, (unsigned long)sum);
		GUI_DisplayText("LAN: OUT OF STEP with %s (step %lu)", 1, g_lanPlayers[slot].name, (unsigned long)step);
	} else {
		Lan_Log("step %lu: %s same %08lx\n", (unsigned long)step, g_lanPlayers[slot].name, (unsigned long)sum);
		if (slot < LAN_MAX_PLAYERS) s_lastSame[slot] = step;
	}
}

/* a sum from another player */
static void Lan_CheckSum(int slot, uint32 step, uint32 sum)
{
	int i;
	if (slot >= LAN_MAX_PLAYERS) return;
	for (i = 0; i < 8; i++) {
		if (s_sumSteps[i] != step) continue;
		Lan_CompareSum(slot, step, s_sums[i], sum);
		return;
	}
	i = (int)((step / LAN_SUM_STEPS) % 8);
	s_otherSums[slot][i] = sum;
	s_otherSteps[slot][i] = step;
}

/* our sum of a step: the others' that came before it */
static void Lan_OwnSum(uint32 step, uint32 sum)
{
	int slot, i = (int)((step / LAN_SUM_STEPS) % 8);
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		if (s_otherSteps[slot][i] != step) continue;
		Lan_CompareSum(slot, step, sum, s_otherSums[slot][i]);
		s_otherSteps[slot][i] = 0;
	}
}

static void Lan_PlayerGone(int slot, const char *why)
{
	if (!g_lanPlayers[slot].used || g_lanPlayers[slot].gone) return;
	g_lanPlayers[slot].gone = true;
	Lan_Log("%s gone: %s\n", g_lanPlayers[slot].name, why);
	if (g_lanGame) GUI_DisplayText("%s left the game.", 1, g_lanPlayers[slot].name);
	if (g_lanServer) {
		uint8 b = (uint8)slot;
		Net_Send(NET_ALL, MT_GONE, &b, 1);
	}
}

/* Network events during the game. */
static void Lan_SetPause(bool on, int slot);
static void Lan_SendSeen(void);
static bool Lan_StartSaved(void);
static void Lan_TakeSeen(int slot, const uint8 *d, int len);
static void Lan_ShowPause(bool on, int slot);
static bool s_paused;

static void Lan_Receive(void)
{
	NetEvent ev;
	while (Net_Event(&ev)) {
		int slot = g_lanServer ? ev.peer + 1 : 0;
		switch (ev.kind) {
			case NEV_PING:
				if (slot < LAN_MAX_PLAYERS) g_lanPlayers[slot].ping = (int)ev.value;
				break;
			case NEV_LOST:
				if (g_lanServer) {
					Lan_PlayerGone(slot, ev.text1);
				} else {
					Lan_PlayerGone(0, ev.text1);
					s_ended = true;
				}
				break;
			case NEV_MSG:
				switch (ev.value) {
					case MT_TURN:
						if (g_lanServer && slot < LAN_MAX_PLAYERS) Lan_TakeTurnFrame(s_turns[slot], ev.data, ev.length);
						break;
					case MT_TURNALL:
						if (!g_lanServer) Lan_TakeTurnFrame(s_turns[0], ev.data, ev.length);
						break;
					case MT_SUM:
						if (ev.length >= 8) Lan_CheckSum(slot, Get32(ev.data), Get32(ev.data + 4));
						break;
					case MT_QUIT:
						if (g_lanServer) {
							Lan_PlayerGone(slot, "quit");
						} else {
							Lan_PlayerGone(0, "quit");
							s_ended = true;
						}
						break;
					case MT_GONE:
						if (!g_lanServer && ev.length >= 1 && ev.data[0] < LAN_MAX_PLAYERS) Lan_PlayerGone(ev.data[0], "gone");
						break;
					case MT_PAUSE:
						/* a joiner asks the server to (un)pause; the server tells all [slot][on] */
						if (g_lanServer) {
							if (slot < LAN_MAX_PLAYERS) Lan_SetPause(!s_paused, slot);
						} else if (ev.length >= 2 && ev.data[0] < LAN_MAX_PLAYERS) {
							Lan_ShowPause(ev.data[1] != 0, ev.data[0]);
						}
						break;
					case MT_SEENREQ:
						/* the server saves: my fog to it, in three parts */
						if (!g_lanServer) Lan_SendSeen();
						break;
					case MT_SEEN:
						if (g_lanServer && slot < LAN_MAX_PLAYERS && ev.length >= 2) Lan_TakeSeen(slot, ev.data, ev.length);
						break;
					case MT_CHAT:
						/* [slot][text]: the server sets the sender and passes it on */
						if (ev.length >= 2 && slot < LAN_MAX_PLAYERS) {
							int from = g_lanServer ? slot : ev.data[0];
							if (from >= LAN_MAX_PLAYERS || !g_lanPlayers[from].used) break;
							if (g_lanServer) {
								int peer;
								ev.data[0] = (uint8)from;
								for (peer = 0; peer < NET_MAX_PEERS; peer++) {
									if (peer + 1 != from && g_lanPlayers[peer + 1].used && !g_lanPlayers[peer + 1].gone) Net_Send(peer, MT_CHAT, ev.data, ev.length);
								}
							}
							Lan_ChatReceived(from, ev.data + 1, ev.length - 1);
						}
						break;
					default: break;
				}
				break;
			default: break;
		}
	}
}

/* Turn T can begin: its orders of every player are here (applied then). */
static bool Lan_BeginTurn(int32 turn)
{
	int slot;

	if (g_lanServer) {
		static uint8 all[LAN_MAX_CMDS * LAN_MAX_PLAYERS * CMD_SIZE];
		int count = 0;
		TurnBuf *own = &s_ownTurns[turn % LAN_RING];

		if (turn >= s_turnBase + s_delay) {
			for (slot = 1; slot < LAN_MAX_PLAYERS; slot++) {
				TurnBuf *t = &s_turns[slot][turn % LAN_RING];
				if (!g_lanPlayers[slot].used || g_lanPlayers[slot].gone) continue;
				if (t->turn != turn || !t->complete) return false;
			}
		}
		/* everyone's orders, the server's first */
		if (own->turn == turn) {
			memcpy(all, own->data, own->count * CMD_SIZE);
			count = own->count;
		}
		for (slot = 1; slot < LAN_MAX_PLAYERS; slot++) {
			TurnBuf *t = &s_turns[slot][turn % LAN_RING];
			if (t->turn != turn) continue;
			memcpy(all + count * CMD_SIZE, t->data, t->count * CMD_SIZE);
			count += t->count;
		}
		Lan_SendCmds(NET_ALL, MT_TURNALL, turn, all, count);
		g_lanSim = true;
		for (slot = 0; slot < count; slot++) Lan_Apply(all + slot * CMD_SIZE);
		g_lanSim = false;

		/* my orders since the last turn: carried out in turn + delay */
		own = &s_ownTurns[(turn + s_delay) % LAN_RING];
		own->turn = turn + s_delay;
		own->count = s_pendingCount;
		own->complete = true;
		memcpy(own->data, s_pending, s_pendingCount * CMD_SIZE);
	} else {
		TurnBuf *t = &s_turns[0][turn % LAN_RING];
		if (turn >= s_turnBase + s_delay) {
			if (t->turn != turn || !t->complete) return false;
			g_lanSim = true;
			for (slot = 0; slot < t->count; slot++) Lan_Apply(t->data + slot * CMD_SIZE);
			g_lanSim = false;
		}
		Lan_SendCmds(0, MT_TURN, turn + s_delay, s_pending, s_pendingCount);
	}
	s_pendingCount = 0;
	return true;
}

static void Lan_UpdateSeen(bool all);
static void Lan_ResetSeen(void);

static void Lan_Step(void)
{
	s_inStep = true;
	g_lanSim = true;
	g_timerGame++;
	GameLoop_Team();
	GameLoop_Unit();
	GameLoop_Structure();
	GameLoop_House();
	Explosion_Tick();
	Animation_Tick();
	Unit_Sort();
	s_steps++;
	if (s_trace) {
		int i, loaded = 0;
		uint32 delays = 0;
		for (i = 0; i < UNIT_INDEX_MAX; i++) {
			Unit *u = Unit_Get_ByIndex(i);
			if (!u->o.flags.s.used) continue;
			delays += u->o.script.delay;
			if (Script_IsLoaded(&u->o.script)) loaded++;
		}
		Lan_Log("r %lu %08lx t%lu d%lu l%d p%d m%d\n", (unsigned long)s_steps, (unsigned long)Tools_Random_State(), (unsigned long)g_timerGame,
		        (unsigned long)delays, loaded, (int)g_playerHouseID, g_lanModal);
	}
	if (s_steps % LAN_SUM_STEPS == 0) {
		uint8 b[8];
		int i = (int)((s_steps / LAN_SUM_STEPS) % 8);
		s_sums[i] = Lan_Checksum();
		s_sumSteps[i] = s_steps;
		Lan_OwnSum(s_steps, s_sums[i]);
		Lan_Log("sum %lu %08lx\n", (unsigned long)s_steps, (unsigned long)s_sums[i]);
		Lan_DumpState();
		Put32(b, s_steps);
		Put32(b + 4, s_sums[i]);
		Net_Send(g_lanServer ? NET_ALL : 0, MT_SUM, b, 8);
	}
	g_lanSim = false;
	s_inStep = false;
}

/* Is the local house still in the game (a structure or a unit)? */
static bool Lan_HouseAlive(uint8 houseID)
{
	PoolFindStruct find;
	Structure *s;
	find.houseID = houseID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	while ((s = Structure_Find(&find)) != NULL) {
		if (s->o.type == STRUCTURE_SLAB_1x1 || s->o.type == STRUCTURE_SLAB_2x2 || s->o.type == STRUCTURE_WALL) continue;
		return true;
	}
	find.houseID = houseID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	return Unit_Find(&find) != NULL;
}

/** The game's steps that are due (in the main loop instead of GameLoop_Team..House). */
static bool s_waitShown;	/* "LAN: WAITING for ..." is on the message line */

/* ---- the pause: the server stops beginning turns, the others then wait for
 * its turns; all stop at the same turn, the game is not changed by it
 * (s_paused above). */

static void Lan_ShowPause(bool on, int slot)
{
	if (on == s_paused) return;
	s_paused = on;
	if (on) {
		GUI_DisplayText("Game paused by %s - press P to continue.", 3, slot < LAN_MAX_PLAYERS ? g_lanPlayers[slot].name : "?");
	} else {
		/* the time of the pause is not caught up */
		s_stepBase = AmigaTimer_GetMs() - (s_steps * 1000) / 60;
		s_waiting = false;
		GUI_DisplayText("Game continues.", 3);
	}
}

/* The server: the pause on or off, all are told. */
static void Lan_SetPause(bool on, int slot)
{
	uint8 b[2];
	b[0] = (uint8)slot;
	b[1] = on ? 1 : 0;
	Net_Send(NET_ALL, MT_PAUSE, b, 2);
	Lan_ShowPause(on, slot);
}

/** P on the map screen of a LAN game (RTS_Pause): pause or go on, for all. */
void Lan_PauseRequest(void)
{
	uint8 b = (uint8)g_lanMe;
	if (!g_lanGame) return;
	if (g_lanServer) {
		Lan_SetPause(!s_paused, g_lanMe);
	} else {
		Net_Send(0, MT_PAUSE, &b, 1);
	}
}

void Lan_Run(void)
{
	uint32 now, due;
	int n = 0;
	int savedUI;

	if (!g_lanGame || s_inStep) return;
	savedUI = g_lanUI;
	g_lanUI = 0;	/* (called in the interface's code: the steps are the simulation) */
	Lan_Receive();
	now = AmigaTimer_GetMs();
	due = (now - s_stepBase) * 60 / 1000;
	if (due > s_steps + 30) {
		/* far behind (waited for a turn): do not race to catch up */
		s_stepBase = now - (s_steps * 1000) / 60;
		due = s_steps + 1;
	}
	if (s_paused) {
		/* no time runs on; the server begins no turn (below) */
		s_stepBase = now - (s_steps * 1000) / 60;
	}
	while (s_steps < due && n < 8) {
		if (s_steps % LAN_TURN_STEPS == 0) {
			if (g_lanServer && s_paused) break;
			if (!Lan_BeginTurn((int32)(s_steps / LAN_TURN_STEPS))) {
				if (!s_waiting) {
					s_waiting = true;
					s_waitSince = now;
				}
				break;
			}
			if (s_waiting && now - s_waitSince > 300) s_waitShown = true;	/* its text off the screen (below) */
			s_waiting = false;
		}
		Lan_Step();
		n++;
	}
	if (n > 0) Lan_UpdateSeen(false);
	if (s_waitShown && !s_waiting && g_lanModal == 0) {
		/* the game goes on: "LAN: WAITING for ..." off the message line (not
		 * under a window: there when it is closed) */
		GUI_ClearTextLine();
		s_waitShown = false;
	}
	if (s_waiting && now - s_waitSince > 300 && !s_paused) {
		static uint32 shown;
		if (now - shown > 1000) {
			int slot;
			s_waitShown = true;
			for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
				if (slot != g_lanMe && g_lanPlayers[slot].used && !g_lanPlayers[slot].gone) break;
			}
			GUI_DisplayText("LAN: WAITING for %s", 1, slot < LAN_MAX_PLAYERS ? g_lanPlayers[slot].name : "the others");
			shown = now;
		}
	}
	g_lanUI = savedUI;
}

/** The game's end for this player: lost, won, the server gone. 0 none, 1 lost, 2 won, 3 server gone. */
int Lan_CheckEnd(void)
{
	static uint32 next;
	int slot, others = 0;
	uint32 now;

	if (!g_lanGame) return 0;
	if (s_leave) return 4;
	if (s_ended) return 3;
	now = AmigaTimer_GetMs();
	if (now - next < 1000 && next != 0) return 0;
	next = now;
	if (s_steps < 120) return 0;
	if (!Lan_HouseAlive(g_lanLocalHouse)) return 1;
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		if (slot == g_lanMe || !g_lanPlayers[slot].used || g_lanPlayers[slot].gone) continue;
		if (Lan_HouseAlive(g_lanPlayers[slot].house)) others++;
	}
	return others == 0 ? 2 : 0;
}

/* ---- start and end */

static void Lan_ResetTurns(void)
{
	int i, j;
	for (i = 0; i < LAN_MAX_PLAYERS; i++) for (j = 0; j < LAN_RING; j++) s_turns[i][j].turn = -1;
	for (j = 0; j < LAN_RING; j++) s_ownTurns[j].turn = -1;
	s_pendingCount = 0;
	s_steps = 0;
	s_turnBase = 0;
	s_waiting = false;
	s_outOfStep = false;
	memset(s_lastSame, 0, sizeof(s_lastSame));
	s_ended = false;
	s_leave = false;
	s_paused = false;
	memset(s_sumSteps, 0, sizeof(s_sumSteps));
	memset(s_otherSteps, 0, sizeof(s_otherSteps));
}

/**
 * Starts the game of the lobby on this Amiga (the server after sending START,
 * a joiner when it came): the same scenario, seed and delay everywhere.
 */
void Lan_StartGame(void)
{
	int slot;
	PoolFindStruct find;
	uint32 seed;
	int delay;

	Lan_StartValues(&seed, &delay);
	s_delay = delay;
	Lan_ResetTurns();
	s_trace = IniFile_GetInteger("debug_lan_trace", 0) != 0;
	s_lanInfo = IniFile_GetInteger("lan_info", 0) != 0;
	{
		extern uint32 g_lanTraceFrom, g_lanTraceTo;
		g_lanTraceFrom = (uint32)IniFile_GetInteger("debug_lan_trace_from", -1);
		g_lanTraceTo = (uint32)IniFile_GetInteger("debug_lan_trace_to", 0);
	}
	g_lanGame = true;
	g_lanLocalHouse = g_lanPlayers[g_lanMe].house;
	s_logNew = true;
	Lan_Log("base Lan_TraceRandom %08lx\n", (unsigned long)&Lan_TraceRandom);
	Lan_Log("level %d\n", g_lanLevel);
	Lan_Log("start: map %d seed %lu delay %d me %d house %d\n", g_lanMap, (unsigned long)seed, delay, g_lanMe, g_lanLocalHouse);

	s_savedSpeed = g_gameConfig.gameSpeed;
	s_savedHints = g_gameConfig.hints;
	g_gameConfig.gameSpeed = 2;
	g_gameConfig.hints = 0;

	g_scenarioID = 22;
	if (g_lanLevel < LAN_LEVEL_MIN || g_lanLevel > LAN_LEVELS) g_lanLevel = LAN_LEVEL_MIN;
	g_campaignID = (uint16)(g_lanLevel - 1);	/* what the houses know: as in mission <level> */
	g_strategicRegionBits = 0;
	if (g_lanFromSave && Lan_StartSaved()) return;
	Tools_RandomLCG_Seed((uint16)seed);
	Tools_Random_Seed(seed);
	/* the game's time and its loops' next times: the same on every Amiga */
	g_timerGame = 1000;
	Unit_ResetTicks();
	Structure_ResetTicks();
	House_ResetTicks();
	Team_ResetTicks();
	Explosion_ResetTicks();
	Animation_ResetTicks();

	Sprites_LoadTiles();
	g_lanSim = true;
	Game_LoadScenario(g_lanPlayers[0].house, 22);
	Lan_PlaceStartUnits();
	g_lanSim = false;
	Lan_Log("loaded %08lx\n", (unsigned long)Tools_Random_State());

	/* the simulation's "player" is the server's house on each Amiga */
	g_playerHouseID = (HouseType)g_lanPlayers[0].house;
	g_playerHouse = House_Get_ByIndex(g_playerHouseID);
	g_lanSimViewPos = Tile_PackXY(Tile_GetPackedX(s_lanMaps[g_lanMap].cy[0]) - 7, Tile_GetPackedY(s_lanMaps[g_lanMap].cy[0]) - 5);

	for (slot = 0; slot < HOUSE_MAX; slot++) s_noSilo[slot] = 0;
	find.houseID = HOUSE_INVALID;
	find.index = 0xFFFF;
	find.type = 0xFFFF;
	for (;;) {
		House *h = House_Find(&find);
		if (h == NULL) break;
		s_noSilo[h->index] = h->credits;
	}

	/* my view: my base */
	GUI_Palette_CreateRemap(g_lanLocalHouse);
	Voice_LoadVoices(g_lanLocalHouse);
	{
		uint16 cy = s_lanMaps[g_lanMap].cy[g_lanMe];
		Map_SetViewportPosition(cy);
		g_selectionRectanglePosition = cy;
		g_selectionPosition = cy;
		Map_SetSelection(cy);
	}
	Lan_ResetSeen();
	s_stepBase = AmigaTimer_GetMs();
}

/* ---- a saved LAN game (the server saves; the lobby sends the file to all)
 *
 * The usual save and the chunk 'LAN ': what the save has not and every Amiga
 * needs to go on the same: the step, the game's time, both random
 * generators, the credits without silos, the players and each one's fog. */

#define LAN_SAVE_VERSION 1
#define LAN_SAVE_SIZE (36 + LAN_MAX_PLAYERS * 18 + LAN_MAX_PLAYERS * 512)

static uint8 s_otherSeen[LAN_MAX_PLAYERS][4096 / 8];
static uint8 s_seenParts[LAN_MAX_PLAYERS];	/* bits: the three parts came */

/* a joiner: its fog to the server (MT_SEEN [part][data]) */
static void Lan_SendSeen(void)
{
	int part;
	for (part = 0; part < 3; part++) {
		uint8 b[1 + 200];
		int off = part * 200, n = min(200, (int)sizeof(s_seen) - off);
		b[0] = (uint8)part;
		memcpy(b + 1, s_seen + off, n);
		Net_Send(0, MT_SEEN, b, 1 + n);
	}
}

static void Lan_TakeSeen(int slot, const uint8 *d, int len)
{
	int part = d[0], off = part * 200, n = len - 1;
	if (part > 2 || off + n > (int)sizeof(s_seen)) return;
	memcpy(s_otherSeen[slot] + off, d + 1, n);
	s_seenParts[slot] |= 1 << part;
}

/** The server, before writing a save: the others' fog (up to 3 s). */
void Lan_CollectSeen(void)
{
	uint32 end;
	int slot;
	if (!g_lanGame || !g_lanServer) return;
	memset(s_seenParts, 0, sizeof(s_seenParts));
	memset(s_otherSeen, 0, sizeof(s_otherSeen));
	memcpy(s_otherSeen[g_lanMe], s_seen, sizeof(s_seen));
	s_seenParts[g_lanMe] = 7;
	Net_Send(NET_ALL, MT_SEENREQ, "s", 1);
	end = AmigaTimer_GetMs() + 3000;
	while ((int32)(end - AmigaTimer_GetMs()) > 0) {
		bool all = true;
		for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
			if (g_lanPlayers[slot].used && !g_lanPlayers[slot].gone && s_seenParts[slot] != 7) all = false;
		}
		if (all) break;
		Lan_Receive();
		AmigaTimer_WaitMs(20);
	}
}

static void Lan_Put16(uint8 *b, uint16 v) { b[0] = (uint8)(v >> 8); b[1] = (uint8)v; }

/** The chunk 'LAN ' of a save (save.c). */
bool Lan_SaveChunk(FILE *fp)
{
	static uint8 b[LAN_SAVE_SIZE];
	int i, n = 0;
	memset(b, 0, sizeof(b));
	b[n++] = LAN_SAVE_VERSION;
	b[n++] = (uint8)g_lanMap;
	b[n++] = (uint8)g_lanLevel;
	b[n++] = (uint8)g_playerHouseID;
	Put32(b + n, s_steps); n += 4;
	Put32(b + n, g_timerGame); n += 4;
	Tools_Random_GetFull(b + n); n += 8;
	for (i = 0; i < 8; i++) { Lan_Put16(b + n, i < HOUSE_MAX ? s_noSilo[i] : 0); n += 2; }
	for (i = 0; i < LAN_MAX_PLAYERS; i++) {
		b[n++] = (uint8)(g_lanPlayers[i].used && !g_lanPlayers[i].gone);
		b[n++] = g_lanPlayers[i].house;
		memcpy(b + n, g_lanPlayers[i].name, 16); n += 16;
	}
	for (i = 0; i < LAN_MAX_PLAYERS; i++) {
		memcpy(b + n, s_otherSeen[i], 512); n += 512;
	}
	return fwrite(b, n, 1, fp) == 1;
}

/**
 * What a LAN save is (the lobby's list, the start): its name (NAME), the
 * chunk 'LAN '; seen: the fog of the player of that house (512 bytes), if
 * not NULL. False when it is no LAN save.
 */
bool Lan_ReadSaveInfo(const char *file, LanSaveInfo *info, uint8 *seen, uint8 seenHouse)
{
	FILE *fp = fopendatadir(SEARCHDIR_PERSONAL_DATA_DIR, file, "rb");
	uint8 h[8];
	bool found = false;
	memset(info, 0, sizeof(*info));
	if (fp == NULL) return false;
	if (fread(h, 4, 1, fp) != 1 || memcmp(h, "FORM", 4) != 0 || fread(h, 8, 1, fp) != 1 || memcmp(h + 4, "SCEN", 4) != 0) {
		fclose(fp);
		return false;
	}
	while (fread(h, 8, 1, fp) == 1) {
		uint32 len = Get32(h + 4);
		if (memcmp(h, "NAME", 4) == 0) {
			char name[64];
			uint32 n = min(len, (uint32)sizeof(name) - 1);
			if (fread(name, n, 1, fp) != 1) break;
			name[n] = 0;
			strncpy(info->desc, name, sizeof(info->desc) - 1);
			fseek(fp, (long)(len - n + (len & 1)), SEEK_CUR);
			continue;
		}
		if (memcmp(h, "LAN ", 4) == 0 && len >= LAN_SAVE_SIZE) {
			static uint8 b[LAN_SAVE_SIZE];
			int i, n = 0;
			if (fread(b, LAN_SAVE_SIZE, 1, fp) != 1 || b[0] != LAN_SAVE_VERSION) break;
			n = 1;
			info->map = b[n++];
			info->level = b[n++];
			info->simHouse = b[n++];
			info->steps = Get32(b + n); n += 4;
			info->timer = Get32(b + n); n += 4;
			memcpy(info->rng, b + n, 8); n += 8;
			for (i = 0; i < 8; i++) { info->noSilo[i] = (uint16)((b[n] << 8) | b[n + 1]); n += 2; }
			for (i = 0; i < LAN_MAX_PLAYERS; i++) {
				info->used[i] = b[n++];
				info->house[i] = b[n++];
				memcpy(info->name[i], b + n, 16); n += 16;
				info->name[i][15] = 0;
				if (info->used[i] && info->house[i] < 8) info->mask |= 1 << info->house[i];
			}
			for (i = 0; i < LAN_MAX_PLAYERS; i++) {
				if (seen != NULL && info->used[i] && info->house[i] == seenHouse) memcpy(seen, b + n, 512);
				n += 512;
			}
			found = true;
			break;
		}
		fseek(fp, (long)(len + (len & 1)), SEEK_CUR);
	}
	fclose(fp);
	info->valid = found;
	return found;
}

/* Lan_StartGame of a saved game: the same on every Amiga from the same file. */
static bool Lan_StartSaved(void)
{
	static uint8 seen[512];
	LanSaveInfo info;
	PoolFindStruct find;
	uint16 view = 0xFFFF;
	int i;

	memset(seen, 0, sizeof(seen));
	if (!Lan_ReadSaveInfo(g_lanSaveFile, &info, seen, g_lanLocalHouse)) {
		Lan_Log("saved game %s: no LAN save\n", g_lanSaveFile);
		return false;
	}
	Lan_Log("saved game %s: step %lu\n", g_lanSaveFile, (unsigned long)info.steps);
	g_lanMap = info.map;
	g_lanLevel = info.level;
	g_timerGame = info.timer;
	Unit_ResetTicks();
	Structure_ResetTicks();
	House_ResetTicks();
	Team_ResetTicks();
	Explosion_ResetTicks();
	Animation_ResetTicks();

	g_lanSim = true;
	if (!SaveGame_LoadFile(g_lanSaveFile)) {
		g_lanSim = false;
		return false;
	}
	g_lanSim = false;
	g_timerGame = info.timer;
	Tools_Random_SetFull(info.rng);
	s_steps = info.steps;
	s_turnBase = (int32)((s_steps + LAN_TURN_STEPS - 1) / LAN_TURN_STEPS);
	Lan_Log("loaded %08lx\n", (unsigned long)Tools_Random_State());

	g_playerHouseID = (HouseType)info.simHouse;
	g_playerHouse = House_Get_ByIndex(g_playerHouseID);
	g_lanSimViewPos = Tile_PackXY(Tile_GetPackedX(s_lanMaps[g_lanMap].cy[0]) - 7, Tile_GetPackedY(s_lanMaps[g_lanMap].cy[0]) - 5);
	for (i = 0; i < HOUSE_MAX; i++) s_noSilo[i] = info.noSilo[i];

	/* my view: my Construction Yard, else any of mine */
	find.houseID = g_lanLocalHouse;
	find.index = 0xFFFF;
	find.type = STRUCTURE_CONSTRUCTION_YARD;
	{
		Structure *st = Structure_Find(&find);
		if (st != NULL) view = Tile_PackTile(st->o.position);
	}
	if (view == 0xFFFF) {
		Unit *u;
		find.houseID = g_lanLocalHouse;
		find.index = 0xFFFF;
		find.type = 0xFFFF;
		u = Unit_Find(&find);
		if (u != NULL) view = Tile_PackTile(u->o.position);
	}
	GUI_Palette_CreateRemap(g_lanLocalHouse);
	Voice_LoadVoices(g_lanLocalHouse);
	Unit_Select(NULL);
	if (view != 0xFFFF) {
		Map_SetViewportPosition(view);
		g_selectionRectanglePosition = view;
		g_selectionPosition = view;
		Map_SetSelection(view);
	}
	Lan_ResetSeen();
	for (i = 0; i < (int)sizeof(s_seen); i++) s_seen[i] |= seen[i];
	g_viewport_forceRedraw = true;
	s_stepBase = AmigaTimer_GetMs() - (s_steps * 1000) / 60;
	return true;
}

/* ---- fog of war, for each player its own
 *
 * The simulation sees the whole map (g_map[].isUnveiled everywhere, the same
 * on every Amiga: explosions, animations, walls depend on it). What a player
 * sees is this Amiga's own: s_seen, the tiles around its house's units and
 * structures, never hidden again (as in Dune II). Only the drawing uses it
 * (viewport, radar, selection); the game is not changed by it. */

static uint16 s_unitTile[UNIT_INDEX_MAX];
static uint16 s_structureTile[STRUCTURE_INDEX_MAX_SOFT];

bool Lan_TileShown(uint16 packed)
{
	return (s_seen[(packed & 0xFFF) >> 3] & (1 << (packed & 7))) != 0;
}

/** The fog over a seen tile: 0, or the border towards unseen neighbours; g_veiledTileID unseen. */
uint16 Lan_FogOverlay(uint16 packed)
{
	uint16 tileID = 0;
	int i;
	if (!Lan_TileShown(packed)) return g_veiledTileID;
	for (i = 0; i < 4; i++) {
		uint16 neighbour = packed + g_table_mapDiff[i];
		if (Tile_IsOutOfMap(neighbour) || !Lan_TileShown(neighbour)) tileID |= 1 << i;
	}
	return tileID == 0 ? 0 : g_iconMap[g_iconMap[ICM_ICONGROUP_FOG_OF_WAR] + tileID];
}

static void Lan_SeeAround(tile32 position, uint16 radius)
{
	int x, y, i, j, k;
	uint16 packed = Tile_PackTile(position);
	tile32 centre;

	if (radius == 0 || !Map_IsValidPosition(packed)) return;
	x = Tile_GetPackedX(packed);
	y = Tile_GetPackedY(packed);
	Tile_MakeXY(centre, x, y);
	for (i = -(int)radius; i <= (int)radius; i++) {
		for (j = -(int)radius; j <= (int)radius; j++) {
			tile32 t;
			uint16 p;
			if (x + i < 0 || x + i >= 64 || y + j < 0 || y + j >= 64) continue;
			p = Tile_PackXY(x + i, y + j);
			if (Lan_TileShown(p)) continue;
			Tile_MakeXY(t, x + i, y + j);
			if (Tile_GetDistanceRoundedUp(centre, t) > radius) continue;
			s_seen[p >> 3] |= 1 << (p & 7);
			/* redraw it and its four neighbours (their fog borders change;
			 * Map_Update redraws only the tile itself), on the radar too */
			Map_Update(p, 0, true);
			for (k = 0; k < 4; k++) {
				uint16 q = p + g_table_mapDiff[k];
				if (!Tile_IsOutOfMap(q)) Map_Update(q, 0, true);
			}
			Map_MarkTileDirty(p);
		}
	}
}

/** What the local house sees now (once a turn; all: every unit and structure again). */
static void Lan_UpdateSeen(bool all)
{
	int i;
	for (i = 0; i < UNIT_INDEX_MAX; i++) {
		Unit *u = Unit_Get_ByIndex(i);
		uint16 packed;
		if (!u->o.flags.s.used || u->o.flags.s.isNotOnMap || Unit_GetHouseID(u) != g_lanLocalHouse) continue;
		if ((u->o.position.x == 0xFFFF && u->o.position.y == 0xFFFF) || (u->o.position.x == 0 && u->o.position.y == 0)) continue;
		packed = Tile_PackTile(u->o.position);
		if (!all && s_unitTile[i] == packed) continue;
		s_unitTile[i] = packed;
		Lan_SeeAround(u->o.position, g_table_unitInfo[u->o.type].o.fogUncoverRadius);
	}
	for (i = 0; i < STRUCTURE_INDEX_MAX_SOFT; i++) {
		Structure *s = Structure_Get_ByIndex(i);
		const StructureInfo *si;
		tile32 position;
		uint16 packed;
		if (!s->o.flags.s.used || s->o.houseID != g_lanLocalHouse) continue;
		packed = Tile_PackTile(s->o.position);
		if (!all && s_structureTile[i] == packed) continue;
		s_structureTile[i] = packed;
		si = &g_table_structureInfo[s->o.type];
		position = s->o.position;
		if (g_dune2_enhanced) {
			position.x += 256 * (g_table_structure_layoutSize[si->layout].width  - 1) / 2;
			position.y += 256 * (g_table_structure_layoutSize[si->layout].height - 1) / 2;
		}
		Lan_SeeAround(position, si->o.fogUncoverRadius);
	}
}

static void Lan_ResetSeen(void)
{
	memset(s_seen, 0, sizeof(s_seen));
	memset(s_unitTile, 0xFF, sizeof(s_unitTile));
	memset(s_structureTile, 0xFF, sizeof(s_structureTile));
	Lan_UpdateSeen(true);
}

/** A chat line ([slot][text]) to the others (gui/lan_chat_amiga.c); false: no connection. */
bool Lan_SendChat(const uint8 *data, int length)
{
	int slot, n = 0;
	if (!g_lanGame) return false;
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		if (slot != g_lanMe && g_lanPlayers[slot].used && !g_lanPlayers[slot].gone) n++;
	}
	if (n == 0) return false;
	return Net_Send(g_lanServer ? NET_ALL : 0, MT_CHAT, data, length) != 0;
}

/**
 * ini lan_info=1 (as LANINFO of the Settlers port): a line over the FPS
 * counter, after each frame of the map screen - the others, their response
 * time and the last step the games agreed ("same"), or OUT OF STEP, or
 * WAITING. Without it these show only on the message line when they happen.
 */
void Lan_InfoDraw(void)
{
	char text[96];
	int slot, n, w;
	Screen old;

	if (!g_lanGame || !s_lanInfo || g_lanModal > 0 || !Layout_IsBig()) return;
	if (g_gameMode != GM_NORMAL || g_selectionType == SELECTIONTYPE_MENTAT) return;
	n = snprintf(text, sizeof(text), "LAN");
	for (slot = 0; slot < LAN_MAX_PLAYERS && n < (int)sizeof(text) - 32; slot++) {
		LanPlayer *p = &g_lanPlayers[slot];
		if (slot == g_lanMe || !p->used) continue;
		if (!g_lanServer && slot != 0) {
			n += snprintf(text + n, sizeof(text) - n, " %s", p->name);	/* (only the server is measured) */
			continue;
		}
		if (p->gone) {
			n += snprintf(text + n, sizeof(text) - n, " %s gone", p->name);
		} else {
			n += snprintf(text + n, sizeof(text) - n, " %s %d ms", p->name, p->ping < 0 ? 0 : p->ping);
			if (!s_outOfStep && s_lastSame[slot] != 0) n += snprintf(text + n, sizeof(text) - n, " same %lu", (unsigned long)s_lastSame[slot]);
		}
	}
	if (s_paused) n += snprintf(text + n, sizeof(text) - n, " PAUSE");
	else if (s_waiting) n += snprintf(text + n, sizeof(text) - n, " WAITING");
	if (s_outOfStep) snprintf(text + n, sizeof(text) - n, " OUT OF STEP %lu", (unsigned long)s_outOfStepAt);

	old = GFX_Screen_SetActive(SCREEN_0);
	GUI_DrawText_Wrapper(NULL, 0, 0, 0, 0, 0x22);
	w = Font_GetStringWidth(text) + 4;
	GUI_Mouse_Hide_Safe();
	GUI_DrawFilledRectangle(0, (int16)(SCREEN_HEIGHT - 26), (int16)(w - 1), (int16)(SCREEN_HEIGHT - 17), 12);
	GUI_DrawText_Wrapper("%s", 2, (int16)(SCREEN_HEIGHT - 25), (uint8)(s_outOfStep ? g_table_houseInfo[HOUSE_HARKONNEN].minimapColor
	                     : g_table_houseInfo[HOUSE_ORDOS].minimapColor), 0, 0x22, text);
	GUI_Mouse_Show_Safe();
	GFX_Screen_SetActive(old);
}

/** The player quits the LAN game (Options); the game loop ends it (Lan_CheckEnd). */
void Lan_LeaveGame(void)
{
	s_leave = true;
}

/** Leaves the LAN game (the game ends or the player quits it). */
void Lan_EndGame(void)
{
	if (!g_lanGame) return;
	Net_Send(g_lanServer ? NET_ALL : 0, MT_QUIT, "q", 1);
	AmigaTimer_WaitMs(100);
	Net_Stop();
	g_lanGame = false;
	g_playerHouseID = (HouseType)g_lanLocalHouse;
	g_gameConfig.gameSpeed = s_savedSpeed;
	g_gameConfig.hints = s_savedHints;
	Lan_Log("end\n");
}


/* ---- the lobby's network part (used by gui/lan_menu_amiga.c) */

/* the server: the lobby to every joiner (each its own slot as "you") */
void Lan_SendLobby(void)
{
	int peer;
	for (peer = 0; peer < NET_MAX_PEERS; peer++) {
		uint8 b[NET_MAX_DATA];
		int n = 0, slot;
		if (!g_lanPlayers[peer + 1].used) continue;
		b[n++] = (uint8)(peer + 1);
		b[n++] = (uint8)g_lanMap;
		b[n++] = (uint8)g_lanLevel;
		for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
			int len = (int)strlen(g_lanPlayers[slot].name);
			b[n++] = (uint8)g_lanPlayers[slot].used;
			b[n++] = g_lanPlayers[slot].house;
			memcpy(b + n, g_lanPlayers[slot].name, len + 1);
			n += len + 1;
		}
		{
			int len = (int)strlen(g_lanSaveDesc);
			b[n++] = g_lanSaveMask;
			memcpy(b + n, g_lanSaveDesc, len + 1);
			n += len + 1;
		}
		Net_Send(peer, MT_LOBBY, b, n);
	}
}

/* a joiner: the lobby from the server */
void Lan_TakeLobby(const uint8 *d, int len)
{
	int n = 0, slot;
	if (len < 3) return;
	g_lanMe = d[n++];
	g_lanMap = d[n++];
	if (g_lanMap >= Lan_MapCount()) g_lanMap = 0;
	g_lanLevel = d[n++];
	if (g_lanLevel < LAN_LEVEL_MIN || g_lanLevel > LAN_LEVELS) g_lanLevel = LAN_LEVEL_MIN;
	for (slot = 0; slot < LAN_MAX_PLAYERS && n + 2 <= len; slot++) {
		int k = 0;
		g_lanPlayers[slot].used = d[n++] != 0;
		g_lanPlayers[slot].house = d[n++];
		while (n < len && d[n] != 0) {
			if (k < (int)sizeof(g_lanPlayers[slot].name) - 1) g_lanPlayers[slot].name[k++] = (char)d[n];
			n++;
		}
		g_lanPlayers[slot].name[k] = 0;
		n++;
	}
	g_lanSaveMask = 0;
	g_lanSaveDesc[0] = 0;
	if (n < len) {
		int k = 0;
		g_lanSaveMask = d[n++];
		while (n < len && d[n] != 0 && k < (int)sizeof(g_lanSaveDesc) - 1) g_lanSaveDesc[k++] = (char)d[n++];
		g_lanSaveDesc[k] = 0;
	}
}

uint8 g_lanSaveMask;	/* a saved game: its houses (bits); 0 a new game */
char g_lanSaveDesc[24];	/* its name, for the lobby */
bool g_lanFromSave;
char g_lanSaveFile[16];

bool Lan_HouseFree(uint8 house)
{
	int slot;
	if (g_lanSaveMask != 0 && (g_lanSaveMask & (1 << house)) == 0) return false;
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		if (g_lanPlayers[slot].used && g_lanPlayers[slot].house == house) return false;
	}
	return true;
}

/* the server: the houses nobody chose to those without one */
void Lan_GiveHouses(void)
{
	int slot, i;
	for (slot = 0; slot < LAN_MAX_PLAYERS; slot++) {
		if (!g_lanPlayers[slot].used || g_lanPlayers[slot].house != HOUSE_INVALID) continue;
		for (i = 0; i < 3; i++) {
			if (Lan_HouseFree(s_lanHouses[i])) {
				g_lanPlayers[slot].house = s_lanHouses[i];
				break;
			}
		}
	}
}

/* the server: START to every joiner, then the game here */
void Lan_SendStart(uint32 seed, int delay)
{
	int peer;
	for (peer = 0; peer < NET_MAX_PEERS; peer++) {
		uint8 b[8];
		if (!g_lanPlayers[peer + 1].used) continue;
		Put32(b, seed);
		b[4] = (uint8)delay;
		b[5] = (uint8)g_lanMap;
		b[6] = (uint8)g_lanLevel;
		b[7] = g_lanFromSave ? 1 : 0;
		Net_Send(peer, MT_START, b, 8);
	}
}

int Lan_DelayForPing(int ms)
{
	int d = 1 + (ms + 50) / 100;
	if (d < 2) d = 2;
	if (d > 8) d = 8;
	return d;
}

const uint8 *Lan_Houses(void) { return s_lanHouses; }

int Lan_MsgType(int which)
{
	switch (which) {
		case 0: return MT_LOBBY;
		case 1: return MT_PICK;
		case 2: return MT_START;
		case 3: return MT_FILE;
		case 4: return MT_FILEEND;
		case 5: return MT_FILEOK;
		default: return MT_QUIT;
	}
}
