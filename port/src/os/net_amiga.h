/** @file src/os/net_amiga.h The network of a LAN game on the Amiga (after Settlers' amiga_net). */

/*
 * All the socket work happens in a process of its own ("OpenDUNE LAN",
 * priority 1) that sleeps in WaitSelect: it costs nothing while nothing
 * comes and never calls the game. The game talks to it through two rings in
 * memory: commands go in with the Net_* calls below, events come out with
 * Net_Event(). None of them waits.
 *
 * Up to three players: the server (player 0) and two that joined it (peers
 * 0 and 1 of the server, players 1 and 2). A player that joined has one peer,
 * the server (peer 0). The players do not talk to each other, the server
 * passes everything on.
 *
 * On the wire (TCP port 6820, TCP_NODELAY) a frame is
 *   length (1 byte: type + data) | type (1 byte) | data
 * HELLO, BYE, PING and PONG are handled by the process; the frames of the
 * game come out as NEV_MSG. A server with room announces itself on UDP port
 * 6820 once a second: "OPENDUNE-LAN <protocol> <version> <players> <name>".
 */

#ifndef OS_NET_AMIGA_H
#define OS_NET_AMIGA_H

#define NET_PORT      6820
#define NET_PROTOCOL  1
#define NET_MAX_PEERS 2
#define NET_MAX_DATA  250
#define NET_ALL       -1	/* Net_Send: to every peer */

typedef enum NetEventKind {
	NEV_NONE,
	NEV_ADDRESS,		/* text1: the address of this Amiga */
	NEV_ERROR,		/* text1: what failed; value 1: what was asked for is off */
	NEV_SERVING,		/* RUN SERVER worked */
	NEV_SERVER,		/* found by a search: text1 name, text2 address, text3 version, value players */
	NEV_SEARCH_DONE,
	NEV_PEER,		/* a player is there (HELLO): peer, text1 name, text2 address, text3 version */
	NEV_LOST,		/* peer is gone: text1 why */
	NEV_PING,		/* peer, value: the time of a PING there and back, ms */
	NEV_MSG,		/* peer, value: type, data / length: a frame of the game */
	NEV_RELAY_GAME,		/* a game waiting on the game server (Net_Relay 'L'): text1 its name,
				 * text2 its player, text3 its code; value: seconds waiting (bits 0-19),
				 * players there (20-21), the most (22-23), bit 30: a password is
				 * needed; the end of the list: NEV_SEARCH_DONE */
	NEV_RELAY_CODE		/* the game made on the game server: text1 its code, value the most players */
} NetEventKind;

typedef struct NetEvent {
	int kind;
	int peer;
	long value;
	char text1[32];
	char text2[32];
	char text3[16];
	unsigned char data[NET_MAX_DATA];
	int length;
} NetEvent;

/* Starts the process (bsdsocket.library is opened there). 0: no TCP/IP stack. */
extern int Net_Start(const char *version);
/* Ends the process and closes everything. */
extern void Net_Quit(void);

/* Each replaces what the process did before: */
extern void Net_Serve(const char *name);	/* RUN SERVER: up to NET_MAX_PEERS players */
extern void Net_Search(int ms);			/* listen for servers for ms */
extern void Net_Connect(const char *address, const char *name);
/* A game over the internet through the game server (Settlers/server) at host
 * (a name or an address) and port. op 'L': the games waiting there
 * (NEV_RELAY_GAME...); 'C': make one for up to players (2 or 3), arg its name,
 * a tab and a password or nothing (NEV_RELAY_CODE, then NEV_PEER for each
 * player that comes; NEV_ERROR with value 1 when the game is over there);
 * 'J': join one, arg its code, a tab and the password (NEV_PEER). version
 * tells the games and their versions apart ("OpenDUNE-0.52"). After NEV_PEER
 * all goes as with Net_Serve / Net_Connect. */
extern void Net_Relay(const char *host, int port, int op, int players, const char *version, const char *name, const char *arg);
extern void Net_Stop(void);			/* back to nothing */
/* The server takes no more players and stops announcing itself (the game started). */
extern void Net_Close(void);
/* The server sends a peer away. */
extern void Net_Drop(int peer, const char *why);

/* A frame of the game (type 0..239) to a peer or NET_ALL. 0: no room (or too long). */
extern int Net_Send(int peer, int type, const void *data, int length);

/* The next event, 0 if none. */
extern int Net_Event(NetEvent *event);

/* Ticks of the E-clock the process ran, frames sent and received. */
extern void Net_Counters(unsigned long *eclocks, unsigned long *framesOut, unsigned long *framesIn);

#endif /* OS_NET_AMIGA_H */
