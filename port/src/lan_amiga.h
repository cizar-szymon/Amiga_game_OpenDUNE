/** @file src/lan_amiga.h A LAN game of up to three players (Amiga, lan_amiga.c). */

#ifndef LAN_AMIGA_H
#define LAN_AMIGA_H

#include "house.h"

struct Unit;
struct Structure;
struct House;

#define LAN_MAX_PLAYERS 3
#define LAN_VERSION "0.02"

extern uint32 AmigaTimer_GetMs(void);

typedef struct LanPlayer {
	bool used;
	bool gone;		/* left the game */
	uint8 house;		/* HOUSE_INVALID: not chosen yet */
	int ping;		/* ms, -1 unknown */
	char name[16];
	char address[16];
} LanPlayer;

extern LanPlayer g_lanPlayers[LAN_MAX_PLAYERS];
extern int g_lanMe;		/* this Amiga's slot (0 the server) */
extern int g_lanMap;
extern int g_lanLevel;	/* Lan_LevelMin()..Lan_LevelCount() */
extern bool g_lanServer;

/* how a unit's order uses its target (Lan_UnitOrder) */
enum { LAN_ORDER_NONE = 0, LAN_ORDER_DESTINATION, LAN_ORDER_MOVE_TARGET, LAN_ORDER_TARGET };
/* Lan_StructureCommand */
enum { LAN_STRUCTURE_BUILD = 1, LAN_STRUCTURE_UPGRADE, LAN_STRUCTURE_REPAIR_UPGRADE, LAN_STRUCTURE_RESUME, LAN_STRUCTURE_HOLD, LAN_STRUCTURE_PLACE };

/* the orders of the player (applied at once outside a LAN game) */
extern void Lan_UnitOrder(struct Unit *u, uint16 action, uint8 mode, uint16 encoded);
extern void Lan_UnitDeviate(struct Unit *u);
extern void Lan_StructureCommand(int command, struct Structure *s, uint16 arg);

/* the game */
extern void Lan_Run(void);
extern int Lan_CheckEnd(void);
extern void Lan_StartGame(void);
extern void Lan_StartValues(uint32 *seed, int *delay);
extern void AmigaTimer_WaitMs(uint32 ms);
extern void Lan_EndGame(void);
extern void Lan_LeaveGame(void);
extern bool g_lanStarting;
extern void Lan_HouseCredits(struct House *h);
extern const char *Lan_ScenarioText(void);

/* the lobby */
extern bool Lan_Menu(void);		/* gui/lan_menu_amiga.c: true = a game was started */
extern int Lan_MapCount(void);
extern const char *Lan_MapName(int i);
extern int Lan_MapPlayers(int i);
extern int Lan_LevelCount(void);
extern int Lan_LevelMin(void);
extern void Lan_SendLobby(void);
extern void Lan_TakeLobby(const uint8 *d, int len);
extern bool Lan_HouseFree(uint8 house);
extern void Lan_GiveHouses(void);
extern void Lan_SendStart(uint32 seed, int delay);
extern int Lan_DelayForPing(int ms);
extern const uint8 *Lan_Houses(void);
extern int Lan_MsgType(int which);

#endif /* LAN_AMIGA_H */
