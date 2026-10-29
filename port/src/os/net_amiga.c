/** @file src/os/net_amiga.c The network of a LAN game: a process of its own (see net_amiga.h). */

#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>

#include "types.h"
#include "net_amiga.h"
#include "amiga_prof.h"

extern void Amiga_ExitLog(const char *text);

/* After Net_Quit's signal: the steps of the process into the exit log (a hang
 * on Amiga 2, 29.10.2026), at most some dozens of lines. */
static volatile int s_quitTrace;
/* Net_Quit asks the process to end. Not only by Ctrl-C: Roadshow on Amiga 2
 * (OS 3.2, 29.10.2026) returned from WaitSelect with no signals while the
 * Ctrl-C was sent - the process went on, the game waited for it for ever and
 * the Amiga stood (the process, priority 1, kept the CPU). */
static volatile int s_quitRequest;
#define NET_QT(text) do { if (s_quitTrace > 0 && s_quitTrace < 60) { s_quitTrace++; Amiga_ExitLog(text); } } while (0)

extern uint32 AmigaTimer_GetMs(void);

struct Library *SocketBase = NULL;

/* The first type of a frame of the game on the wire; smaller ones are the link's. */
#define MSG_GAME 16

/* ---- Rings: one writer, one reader, no locks. A frame is
 * length (2 bytes) | kind (1 byte) | data. The writer moves head after the
 * data, the reader tail after reading it. */

#define RING_SIZE 8192

typedef struct Ring {
	volatile ULONG head, tail;
	UBYTE buf[RING_SIZE];
} Ring;

static Ring s_toNet, s_fromNet;

static ULONG Ring_Free(const Ring *r)
{
	return RING_SIZE - 1 - ((r->head - r->tail) & (RING_SIZE - 1));
}

static void Ring_Write(Ring *r, ULONG *pos, const void *src, int n)
{
	const UBYTE *s = (const UBYTE *)src;
	while (n-- > 0) {
		r->buf[*pos] = *s++;
		*pos = (*pos + 1) & (RING_SIZE - 1);
	}
}

static int Ring_Put(Ring *r, int kind, const void *a, int alen, const void *b, int blen)
{
	ULONG pos = r->head;
	int len = 1 + alen + blen;
	UBYTE hdr[3];
	if ((ULONG)(len + 2) > Ring_Free(r)) return 0;
	hdr[0] = (UBYTE)(len >> 8);
	hdr[1] = (UBYTE)len;
	hdr[2] = (UBYTE)kind;
	Ring_Write(r, &pos, hdr, 3);
	Ring_Write(r, &pos, a, alen);
	Ring_Write(r, &pos, b, blen);
	r->head = pos;
	return 1;
}

/* The kind and the length of the next frame, without taking it. */
static int Ring_Peek(const Ring *r, int *kind, int *length)
{
	ULONG pos = r->tail;
	int len;
	if (pos == r->head) return 0;
	len = r->buf[pos] << 8;
	len |= r->buf[(pos + 1) & (RING_SIZE - 1)];
	*kind = r->buf[(pos + 2) & (RING_SIZE - 1)];
	*length = len - 1;
	return 1;
}

/* data gets at most size bytes; the rest of a longer frame is skipped. */
static int Ring_Get(Ring *r, int *kind, UBYTE *data, int size, int *length)
{
	ULONG pos = r->tail;
	int len, i;
	if (pos == r->head) return 0;
	len = r->buf[pos] << 8;
	pos = (pos + 1) & (RING_SIZE - 1);
	len |= r->buf[pos];
	pos = (pos + 1) & (RING_SIZE - 1);
	*kind = r->buf[pos];
	pos = (pos + 1) & (RING_SIZE - 1);
	len--;
	for (i = 0; i < len; i++) {
		if (i < size) data[i] = r->buf[pos];
		pos = (pos + 1) & (RING_SIZE - 1);
	}
	*length = len < size ? len : size;
	r->tail = pos;
	return 1;
}

/* ---- The process and the game side */

enum { C_SERVE = 1, C_SEARCH, C_CONNECT, C_STOP, C_SEND, C_CLOSE, C_DROP };

static struct Task *volatile s_netTask = NULL;
static volatile ULONG s_wakeMask = 0;
static struct Task *s_parentTask = NULL;
static BYTE s_parentSignal = -1;
static volatile int s_startFailed = 0;
static char s_version[16];

static volatile ULONG s_countEClocks, s_countFramesOut, s_countFramesIn;
/* The process stopped taking frames: the ring to the game is full. */
static volatile int s_rxBlocked = 0;

static void Net_Command(int kind, const void *a, int alen, const void *b, int blen)
{
	if (s_netTask == NULL) return;
	if (Ring_Put(&s_toNet, kind, a, alen, b, blen)) Signal((struct Task *)s_netTask, s_wakeMask);
}

static void PutBE32(UBYTE *p, ULONG v)
{
	p[0] = (UBYTE)(v >> 24); p[1] = (UBYTE)(v >> 16);
	p[2] = (UBYTE)(v >> 8); p[3] = (UBYTE)v;
}

static ULONG GetBE32(const UBYTE *p)
{
	return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | p[3];
}

/* An event: value, peer, then the texts one after another with their zeros. */
static int s_serving;
static void Event(int kind, int peer, long value, const char *t1, const char *t2, const char *t3)
{
	UBYTE buf[5 + 32 + 32 + 16];
	int n = 5;
	const char *t[3];
	int i;
	t[0] = t1; t[1] = t2; t[2] = t3;
	if (kind == NEV_ERROR) value = s_serving ? 0 : 1;	/* 1: all is off */
	PutBE32(buf, (ULONG)value);
	buf[4] = (UBYTE)peer;
	for (i = 0; i < 3; i++) {
		const char *s = t[i] != NULL ? t[i] : "";
		int max = i == 2 ? 15 : 31;
		int len = (int)strlen(s);
		if (len > max) len = max;
		memcpy(buf + n, s, len);
		buf[n + len] = 0;
		n += len + 1;
	}
	Ring_Put(&s_fromNet, kind, buf, n, NULL, 0);
}

static void CopyText(char *dst, int size, const UBYTE **p, const UBYTE *end)
{
	int n = 0;
	while (*p < end && **p != 0) {
		if (n < size - 1) dst[n++] = (char)**p;
		(*p)++;
	}
	if (*p < end) (*p)++;
	dst[n] = 0;
}

int Net_Event(NetEvent *ev)
{
	UBYTE buf[5 + 1 + NET_MAX_DATA + 8];
	int kind, len;
	const UBYTE *p, *end;
	if (!Ring_Get(&s_fromNet, &kind, buf, sizeof(buf), &len)) return 0;
	if (s_rxBlocked && s_netTask != NULL && Ring_Free(&s_fromNet) >= RING_SIZE / 2) {
		s_rxBlocked = 0;
		Signal((struct Task *)s_netTask, s_wakeMask);
	}
	memset(ev, 0, sizeof(*ev) - sizeof(ev->data));
	ev->kind = kind;
	if (len < 5) return 1;
	ev->value = (long)GetBE32(buf);
	ev->peer = buf[4];
	if (kind == NEV_MSG) {
		ev->length = len - 5;
		if (ev->length > NET_MAX_DATA) ev->length = NET_MAX_DATA;
		memcpy(ev->data, buf + 5, ev->length);
		return 1;
	}
	p = buf + 5;
	end = buf + len;
	CopyText(ev->text1, sizeof(ev->text1), &p, end);
	CopyText(ev->text2, sizeof(ev->text2), &p, end);
	CopyText(ev->text3, sizeof(ev->text3), &p, end);
	return 1;
}

void Net_Serve(const char *name)
{
	Net_Command(C_SERVE, name, (int)strlen(name), NULL, 0);
}

void Net_Search(int ms)
{
	UBYTE b[4];
	PutBE32(b, (ULONG)ms);
	Net_Command(C_SEARCH, b, 4, NULL, 0);
}

void Net_Connect(const char *address, const char *name)
{
	Net_Command(C_CONNECT, address, (int)strlen(address) + 1, name, (int)strlen(name) + 1);
}

void Net_Stop(void)
{
	Net_Command(C_STOP, NULL, 0, NULL, 0);
}

void Net_Close(void)
{
	Net_Command(C_CLOSE, NULL, 0, NULL, 0);
}

void Net_Drop(int peer, const char *why)
{
	UBYTE p = (UBYTE)peer;
	Net_Command(C_DROP, &p, 1, why, (int)strlen(why));
}

int Net_Send(int peer, int type, const void *data, int length)
{
	UBYTE h[2];
	if (s_netTask == NULL || length > NET_MAX_DATA || type < 0 || type > 239) return 0;
	h[0] = (UBYTE)(peer < 0 ? 0xFF : peer);
	h[1] = (UBYTE)(type + MSG_GAME);
	if (!Ring_Put(&s_toNet, C_SEND, h, 2, data, length)) return 0;
	Signal((struct Task *)s_netTask, s_wakeMask);
	return 1;
}

void Net_Counters(unsigned long *eclocks, unsigned long *framesOut, unsigned long *framesIn)
{
	*eclocks = s_countEClocks;
	*framesOut = s_countFramesOut;
	*framesIn = s_countFramesIn;
}

/* ---- Inside the process */

enum { F_HELLO = 1, F_BYE, F_PING, F_PONG };

typedef struct Peer {
	long fd;
	int connecting, ok;
	ULONG connectEnd, nextPing;
	char ip[16];
	UBYTE in[4096];
	int inLen;
	UBYTE out[8192];
	int outLen;
} Peer;

static Peer s_peers[NET_MAX_PEERS];
static long s_listenFd = -1, s_udpFd = -1;
static int s_searching, s_closed;
static ULONG s_nextAnnounce, s_searchEnd;
static char s_myName[32];

static void Net_CloseFd(long *fd)
{
	if (*fd >= 0) CloseSocket(*fd);
	*fd = -1;
}

static void Net_NonBlocking(long fd)
{
	long one = 1;
	IoctlSocket(fd, FIONBIO, (char *)&one);
}

static void Net_NoDelay(long fd)
{
	long on = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

static void Net_AddressText(ULONG addr, char *text, int size)
{
	const char *a = (const char *)Inet_NtoA(addr);
	strncpy(text, a != NULL ? a : "", size - 1);
	text[size - 1] = 0;
}

/* The address the way out of this Amiga has: a UDP socket "connected" to
 * an address outside the network (nothing is sent) knows its own one.
 * Without a route there, the address of the host name. */
static void Net_LocalAddress(char *text, int size)
{
	struct sockaddr_in to, me;
	socklen_t len = sizeof(me);
	long fd = socket(AF_INET, SOCK_DGRAM, 0);
	text[0] = 0;
	if (fd >= 0) {
		memset(&to, 0, sizeof(to));
		to.sin_family = AF_INET;
		to.sin_port = htons(9);
		to.sin_addr.s_addr = htonl(0xc0000201UL);	/* 192.0.2.1, TEST-NET */
		memset(&me, 0, sizeof(me));
		if (connect(fd, (struct sockaddr *)&to, sizeof(to)) == 0 &&
		    getsockname(fd, (struct sockaddr *)&me, &len) == 0 && me.sin_addr.s_addr != 0) {
			Net_AddressText(me.sin_addr.s_addr, text, size);
		}
		CloseSocket(fd);
	}
	if (text[0] == 0) {
		char host[64];
		if (gethostname(host, sizeof(host)) == 0) {
			struct hostent *he = gethostbyname((STRPTR)host);
			if (he != NULL && he->h_addr_list[0] != NULL) {
				struct in_addr a;
				memcpy(&a, he->h_addr_list[0], sizeof(a));
				Net_AddressText(a.s_addr, text, size);
			}
		}
	}
}

static void Net_Fail(const char *what)
{
	char text[32];
	snprintf(text, sizeof(text), "%s failed (%ld).", what, (long)Errno());
	Event(NEV_ERROR, 0, 0, text, NULL, NULL);
}

static int Net_PeerCount(void)
{
	int i, n = 0;
	for (i = 0; i < NET_MAX_PEERS; i++) if (s_peers[i].fd >= 0) n++;
	return n;
}

/* A frame to a peer, through its out buffer (sent in the loop). */
static void Net_FrameOut(Peer *p, int type, const void *data, int length)
{
	if (p->fd < 0 || p->outLen + 2 + length > (int)sizeof(p->out)) return;
	p->out[p->outLen] = (UBYTE)(1 + length);
	p->out[p->outLen + 1] = (UBYTE)type;
	if (length > 0) memcpy(p->out + p->outLen + 2, data, length);
	p->outLen += 2 + length;
	s_countFramesOut++;
}

static void Net_FlushOut(Peer *p)
{
	long n;
	if (p->fd < 0 || p->outLen == 0 || p->connecting) return;
	n = send(p->fd, p->out, p->outLen, 0);
	if (n > 0) {
		memmove(p->out, p->out + n, p->outLen - n);
		p->outLen -= (int)n;
	}
}

static void Net_SendHello(Peer *p)
{
	UBYTE b[1 + 16 + 32];
	int n = 1, len;
	b[0] = NET_PROTOCOL;
	len = (int)strlen(s_version);
	memcpy(b + n, s_version, len + 1);
	n += len + 1;
	len = (int)strlen(s_myName);
	memcpy(b + n, s_myName, len + 1);
	n += len + 1;
	Net_FrameOut(p, F_HELLO, b, n);
}

static void Net_DropPeer(int i, const char *why)
{
	Peer *p = &s_peers[i];
	if (p->fd >= 0 && why != NULL && (p->ok || p->connecting)) Event(NEV_LOST, i, 0, why, NULL, NULL);
	Net_CloseFd(&p->fd);
	p->ok = 0;
	p->connecting = 0;
	p->inLen = 0;
	p->outLen = 0;
}

static void Net_Bye(Peer *p, const char *why)
{
	Net_FrameOut(p, F_BYE, why, (int)strlen(why));
	Net_FlushOut(p);
}

static void Net_StopAll(void)
{
	int i;
	for (i = 0; i < NET_MAX_PEERS; i++) Net_DropPeer(i, NULL);
	Net_CloseFd(&s_listenFd);
	Net_CloseFd(&s_udpFd);
	s_serving = s_searching = s_closed = 0;
}

static void Net_StartServing(const UBYTE *name, int len)
{
	struct sockaddr_in addr;
	long one = 1;
	Net_StopAll();
	if (len > 31) len = 31;
	memcpy(s_myName, name, len);
	s_myName[len] = 0;

	s_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (s_listenFd < 0) { Net_Fail("TCP"); return; }
	setsockopt(s_listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(NET_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s_listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(s_listenFd, 3) < 0) {
		Net_Fail("TCP port 6820");
		Net_CloseFd(&s_listenFd);
		return;
	}
	Net_NonBlocking(s_listenFd);

	s_udpFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (s_udpFd < 0 || setsockopt(s_udpFd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0) {
		Net_Fail("UDP broadcast");
		Net_StopAll();
		return;
	}
	Net_NonBlocking(s_udpFd);
	s_serving = 1;
	s_nextAnnounce = AmigaTimer_GetMs();
	Event(NEV_SERVING, 0, 0, NULL, NULL, NULL);
}

static void Net_Announce(void)
{
	char text[96];
	struct sockaddr_in to;
	snprintf(text, sizeof(text), "OPENDUNE-LAN %d %s %d %s", NET_PROTOCOL, s_version, 1 + Net_PeerCount(), s_myName);
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons(NET_PORT);
	to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	sendto(s_udpFd, text, strlen(text), 0, (struct sockaddr *)&to, sizeof(to));
}

static void Net_StartSearch(ULONG ms)
{
	struct sockaddr_in addr;
	long one = 1;
	Net_StopAll();
	s_udpFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (s_udpFd < 0) { Net_Fail("UDP"); return; }
	setsockopt(s_udpFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(NET_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s_udpFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		Net_Fail("UDP port 6820");
		Net_CloseFd(&s_udpFd);
		return;
	}
	Net_NonBlocking(s_udpFd);
	s_searching = 1;
	s_searchEnd = AmigaTimer_GetMs() + ms;
}

/* "OPENDUNE-LAN <protocol> <version> <players> <name>" from a server. */
static void Net_ReadAnnouncement(void)
{
	char text[100], ip[16], version[16];
	struct sockaddr_in from;
	socklen_t len = sizeof(from);
	int proto = 0, players = 0, pos = 0;
	long n = recvfrom(s_udpFd, text, sizeof(text) - 1, 0, (struct sockaddr *)&from, &len);
	if (n <= 0) return;
	text[n] = 0;
	if (sscanf(text, "OPENDUNE-LAN %d %15s %d %n", &proto, version, &players, &pos) < 3 || pos == 0) return;
	Net_AddressText(from.sin_addr.s_addr, ip, sizeof(ip));
	Event(NEV_SERVER, 0, proto * 16 + players, text + pos, ip, version);
}

static void Net_StartConnect(const UBYTE *data, int len)
{
	struct sockaddr_in to;
	const UBYTE *p = data, *end = data + len;
	Peer *peer = &s_peers[0];
	char ip[32];
	long r;
	Net_StopAll();
	CopyText(ip, sizeof(ip), &p, end);
	CopyText(s_myName, sizeof(s_myName), &p, end);
	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = htons(NET_PORT);
	to.sin_addr.s_addr = inet_addr((STRPTR)ip);
	if (to.sin_addr.s_addr == INADDR_NONE) {
		Event(NEV_ERROR, 0, 0, "Not an address.", NULL, NULL);
		return;
	}
	strncpy(peer->ip, ip, sizeof(peer->ip) - 1);
	peer->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (peer->fd < 0) { Net_Fail("TCP"); return; }
	Net_NonBlocking(peer->fd);
	Net_NoDelay(peer->fd);
	r = connect(peer->fd, (struct sockaddr *)&to, sizeof(to));
	if (r < 0 && Errno() != EINPROGRESS) {
		Net_Fail("Connecting");
		Net_CloseFd(&peer->fd);
		return;
	}
	peer->connecting = 1;
	peer->connectEnd = AmigaTimer_GetMs() + 5000;
	if (r == 0) {
		peer->connecting = 0;
		Net_SendHello(peer);
	}
}

static void Net_AcceptPeer(void)
{
	struct sockaddr_in from;
	socklen_t len = sizeof(from);
	long fd = accept(s_listenFd, (struct sockaddr *)&from, &len);
	int i;
	Peer *p;
	if (fd < 0) return;
	for (i = 0; i < NET_MAX_PEERS && s_peers[i].fd >= 0; i++) {}
	if (i == NET_MAX_PEERS || s_closed) {
		UBYTE f[6] = { 5, F_BYE, 'b', 'u', 's', 'y' };
		send(fd, f, sizeof(f), 0);
		CloseSocket(fd);
		return;
	}
	p = &s_peers[i];
	p->fd = fd;
	Net_NonBlocking(fd);
	Net_NoDelay(fd);
	Net_AddressText(from.sin_addr.s_addr, p->ip, sizeof(p->ip));
	p->inLen = p->outLen = 0;
	p->ok = p->connecting = 0;
	Net_SendHello(p);
}

static void Net_FrameIn(int i, int type, const UBYTE *d, int len)
{
	Peer *p = &s_peers[i];
	s_countFramesIn++;
	switch (type) {
		case F_HELLO: {
			char version[16], name[32];
			const UBYTE *q = d + 1, *end = d + len;
			if (len < 1) return;
			CopyText(version, sizeof(version), &q, end);
			CopyText(name, sizeof(name), &q, end);
			if (d[0] != NET_PROTOCOL || strcmp(version, s_version) != 0) {
				char why[32];
				snprintf(why, sizeof(why), "Different version: %s", version);
				Net_Bye(p, "Different version");
				if (!s_serving) Event(NEV_ERROR, i, 0, why, NULL, NULL);
				Net_DropPeer(i, NULL);
				return;
			}
			p->ok = 1;
			p->nextPing = AmigaTimer_GetMs();
			Event(NEV_PEER, i, 0, name, p->ip, version);
			break;
		}
		case F_BYE: {
			char why[32];
			int n = len < 31 ? len : 31;
			memcpy(why, d, n);
			why[n] = 0;
			if (strcmp(why, "busy") == 0) strcpy(why, "The server is full.");
			if (!p->ok) Event(NEV_ERROR, i, 0, why, NULL, NULL);
			Net_DropPeer(i, p->ok ? why : NULL);
			break;
		}
		case F_PING:
			Net_FrameOut(p, F_PONG, d, len);
			break;
		case F_PONG:
			if (len == 4) Event(NEV_PING, i, (long)(AmigaTimer_GetMs() - GetBE32(d)), NULL, NULL, NULL);
			break;
		default:
			if (type >= MSG_GAME && p->ok) {
				UBYTE hdr[5];
				PutBE32(hdr, (ULONG)(type - MSG_GAME));
				hdr[4] = (UBYTE)i;
				Ring_Put(&s_fromNet, NEV_MSG, hdr, 5, d, len);
			}
			break;
	}
}

/* The whole frames in a peer's in buffer to the game. A frame of the game
 * waits there while the ring to the game is full; s_rxBlocked then, and the
 * game wakes the process when it has taken events (Net_Event). */
static void Net_TakeFrames(int i)
{
	Peer *p = &s_peers[i];
	int pos = 0;
	while (p->inLen - pos >= 2 && p->inLen - pos >= 1 + p->in[pos]) {
		int len = p->in[pos];
		if (len == 0) { Net_DropPeer(i, "Bad data."); return; }
		if (p->in[pos + 1] >= MSG_GAME && Ring_Free(&s_fromNet) < (ULONG)(len + 10)) {
			s_rxBlocked = 1;
			break;
		}
		Net_FrameIn(i, p->in[pos + 1], p->in + pos + 2, len - 1);
		if (p->fd < 0) return;
		pos += 1 + len;
	}
	memmove(p->in, p->in + pos, p->inLen - pos);
	p->inLen -= pos;
}

static void Net_ReadPeer(int i)
{
	Peer *p = &s_peers[i];
	long n = recv(p->fd, p->in + p->inLen, sizeof(p->in) - p->inLen, 0);
	if (n <= 0) {
		if (n < 0 && Errno() == EWOULDBLOCK) return;
		Net_DropPeer(i, "Disconnected.");
		return;
	}
	p->inLen += (int)n;
	Net_TakeFrames(i);
}

static void Net_DoCommand(int kind, const UBYTE *d, int len)
{
	int i;
	switch (kind) {
		case C_SERVE: Net_StartServing(d, len); break;
		case C_SEARCH: Net_StartSearch(len >= 4 ? GetBE32(d) : 3000); break;
		case C_CONNECT: Net_StartConnect(d, len); break;
		case C_STOP:
			for (i = 0; i < NET_MAX_PEERS; i++) {
				if (s_peers[i].fd >= 0 && !s_peers[i].connecting) Net_Bye(&s_peers[i], "Disconnected.");
			}
			Net_StopAll();
			break;
		case C_CLOSE:
			s_closed = 1;
			Net_CloseFd(&s_listenFd);
			break;
		case C_DROP:
			if (len >= 1 && d[0] < NET_MAX_PEERS && s_peers[d[0]].fd >= 0) {
				char why[32];
				int n = len - 1 < 31 ? len - 1 : 31;
				memcpy(why, d + 1, n);
				why[n] = 0;
				Net_Bye(&s_peers[d[0]], why);
				Net_DropPeer(d[0], NULL);
			}
			break;
		case C_SEND:
			if (len >= 2) {
				for (i = 0; i < NET_MAX_PEERS; i++) {
					if ((d[0] == 0xFF || d[0] == i) && s_peers[i].ok) Net_FrameOut(&s_peers[i], d[1], d + 2, len - 2);
				}
			}
			break;
	}
}

/* Room for a frame of len bytes in the out buffers it goes to. */
static int Net_RoomFor(const UBYTE *d, int len)
{
	int i;
	for (i = 0; i < NET_MAX_PEERS; i++) {
		if ((d[0] == 0xFF || d[0] == i) && s_peers[i].ok && s_peers[i].outLen + 2 + (len - 2) > (int)sizeof(s_peers[i].out)) return 0;
	}
	return 1;
}

static void Net_Main(void)
{
	struct Task *me = FindTask(NULL);
	BYTE wake = AllocSignal(-1);
	int quit = 0, i;

	SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
	if (SocketBase == NULL || wake < 0) {
		s_startFailed = 1;
		if (SocketBase != NULL) CloseLibrary(SocketBase);
		SocketBase = NULL;
		Forbid();
		Signal(s_parentTask, 1UL << s_parentSignal);
		return;
	}
	for (i = 0; i < NET_MAX_PEERS; i++) s_peers[i].fd = -1;
	s_wakeMask = 1UL << wake;
	s_netTask = me;
	Signal(s_parentTask, 1UL << s_parentSignal);

	{
		char address[32];
		Net_LocalAddress(address, sizeof(address));
		Event(NEV_ADDRESS, 0, 0, address, NULL, NULL);
	}

	while (!quit) {
		fd_set rd, wr;
		long top = -1;
		struct timeval tv;
		ULONG sigs = s_wakeMask | SIGBREAKF_CTRL_C;
		ULONG start;
		int timed = s_serving || s_searching || Net_PeerCount() > 0;
		UBYTE data[2 + NET_MAX_DATA + 8];
		int kind, len;

		FD_ZERO(&rd);
		FD_ZERO(&wr);
		if (s_listenFd >= 0) { FD_SET(s_listenFd, &rd); top = s_listenFd; }
		if (s_udpFd >= 0 && s_searching) {
			FD_SET(s_udpFd, &rd);
			if (s_udpFd > top) top = s_udpFd;
		}
		for (i = 0; i < NET_MAX_PEERS; i++) {
			Peer *p = &s_peers[i];
			if (p->fd < 0) continue;
			if (p->connecting || p->outLen > 0) FD_SET(p->fd, &wr);
			if (!p->connecting && !s_rxBlocked && p->inLen < (int)sizeof(p->in)) FD_SET(p->fd, &rd);
			if (p->fd > top) top = p->fd;
		}
		tv.tv_secs = 0;
		tv.tv_micro = 200000;
		NET_QT("net: WaitSelect");
		WaitSelect(top + 1, &rd, &wr, NULL, timed ? &tv : NULL, &sigs);
		if (s_quitTrace > 0 && s_quitTrace < 60) {
			char t[64];
			snprintf(t, sizeof(t), "net: woke, signals %08lx, top %ld", (unsigned long)sigs, top);
			NET_QT(t);
		}

		start = AmigaTimer_EClock();
		if ((sigs & SIGBREAKF_CTRL_C) || s_quitRequest) quit = 1;

		for (;;) {
			/* A frame of the game waits in the ring until it fits into the
			 * out buffers (sent below and as the sockets take it). */
			if (Ring_Peek(&s_toNet, &kind, &len) && kind == C_SEND) {
				UBYTE h[2];
				h[0] = s_toNet.buf[(s_toNet.tail + 3) & (RING_SIZE - 1)];
				h[1] = 0;
				if (!Net_RoomFor(h, len)) break;
			}
			if (!Ring_Get(&s_toNet, &kind, data, sizeof(data), &len)) break;
			Net_DoCommand(kind, data, len);
		}

		NET_QT("net: commands done");
		if (s_listenFd >= 0 && FD_ISSET(s_listenFd, &rd)) Net_AcceptPeer();
		if (s_searching && s_udpFd >= 0 && FD_ISSET(s_udpFd, &rd)) Net_ReadAnnouncement();
		for (i = 0; i < NET_MAX_PEERS; i++) {
			Peer *p = &s_peers[i];
			if (p->fd >= 0 && p->connecting && FD_ISSET(p->fd, &wr)) {
				long err = 0;
				socklen_t elen = sizeof(err);
				getsockopt(p->fd, SOL_SOCKET, SO_ERROR, &err, &elen);
				if (err != 0) {
					char text[32];
					snprintf(text, sizeof(text), "No server there (%ld).", err);
					Event(NEV_ERROR, i, 0, text, NULL, NULL);
					Net_DropPeer(i, NULL);
				} else {
					p->connecting = 0;
					Net_SendHello(p);
				}
			}
			if (p->fd >= 0 && !p->connecting && p->inLen > 0) Net_TakeFrames(i);
			if (p->fd >= 0 && !p->connecting && FD_ISSET(p->fd, &rd)) Net_ReadPeer(i);
		}

		{
			ULONG now = AmigaTimer_GetMs();
			if (s_serving && !s_closed && Net_PeerCount() < NET_MAX_PEERS && (LONG)(now - s_nextAnnounce) >= 0) {
				Net_Announce();
				s_nextAnnounce = now + 1000;
			}
			if (s_searching && (LONG)(now - s_searchEnd) >= 0) {
				s_searching = 0;
				Net_CloseFd(&s_udpFd);
				Event(NEV_SEARCH_DONE, 0, 0, NULL, NULL, NULL);
			}
			for (i = 0; i < NET_MAX_PEERS; i++) {
				Peer *p = &s_peers[i];
				if (p->fd >= 0 && p->connecting && (LONG)(now - p->connectEnd) >= 0) {
					Event(NEV_ERROR, i, 0, "No answer from the server.", NULL, NULL);
					Net_DropPeer(i, NULL);
				}
				if (p->ok && (LONG)(now - p->nextPing) >= 0) {
					UBYTE b[4];
					PutBE32(b, now);
					Net_FrameOut(p, F_PING, b, 4);
					p->nextPing = now + 1000;
				}
			}
		}
		NET_QT("net: flush");
		for (i = 0; i < NET_MAX_PEERS; i++) Net_FlushOut(&s_peers[i]);
		NET_QT("net: flushed");
		s_countEClocks += AmigaTimer_EClock() - start;
	}

	Amiga_ExitLog("net: quit");
	for (i = 0; i < NET_MAX_PEERS; i++) {
		if (s_peers[i].fd >= 0 && !s_peers[i].connecting) Net_Bye(&s_peers[i], "Disconnected.");
	}
	Amiga_ExitLog("net: bye sent");
	Net_StopAll();
	Amiga_ExitLog("net: sockets closed");
	CloseLibrary(SocketBase);
	SocketBase = NULL;
	Amiga_ExitLog("net: library closed");
	FreeSignal(wake);
	Forbid();
	s_netTask = NULL;
	Signal(s_parentTask, 1UL << s_parentSignal);
}

int Net_Start(const char *version)
{
	if (s_netTask != NULL) return 1;
	strncpy(s_version, version, sizeof(s_version) - 1);
	AmigaTimer_GetMs();	/* timer.device open before the process uses it */
	s_toNet.head = s_toNet.tail = 0;
	s_fromNet.head = s_fromNet.tail = 0;
	s_parentTask = FindTask(NULL);
	s_parentSignal = AllocSignal(-1);
	if (s_parentSignal < 0) return 0;
	s_startFailed = 0;
	s_quitRequest = 0;
	s_quitTrace = 0;
	SetSignal(0, 1UL << s_parentSignal);
	if (CreateNewProcTags(NP_Entry, (ULONG)Net_Main,
	                      NP_Name, (ULONG)"OpenDUNE LAN",
	                      NP_Priority, 1,
	                      NP_StackSize, 16384,
	                      TAG_DONE) == NULL) {
		FreeSignal(s_parentSignal);
		s_parentSignal = -1;
		return 0;
	}
	Wait(1UL << s_parentSignal);
	if (s_startFailed) {
		FreeSignal(s_parentSignal);
		s_parentSignal = -1;
		return 0;
	}
	return 1;
}

void Net_Quit(void)
{
	if (s_netTask == NULL) return;
	Amiga_ExitLog("Net_Quit: signal the network process");
	s_quitTrace = 1;
	s_quitRequest = 1;
	Signal((struct Task *)s_netTask, SIGBREAKF_CTRL_C);
	Wait(1UL << s_parentSignal);
	Amiga_ExitLog("Net_Quit: it ended");
	FreeSignal(s_parentSignal);
	s_parentSignal = -1;
}
