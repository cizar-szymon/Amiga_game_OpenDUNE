/* netaddr: how this Amiga's own IP address can be found (OpenDUNE LAN, the
 * server's address; warpSOCKET on Amiga 2 gave none, 29.10.2026). Tries:
 *  1. a UDP socket connected outwards, getsockname()
 *  2. gethostname() + gethostbyname()
 *  3. ioctl SIOCGIFCONF (the interfaces)
 *  4. WaitSelect with Ctrl-C: does it report the signal?
 */
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

struct Library *SocketBase;

static void show(const char *what, ULONG a)
{
	printf("%s: %s\n", what, (char *)Inet_NtoA(a));
}

int main(void)
{
	long fd;
	SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
	if (SocketBase == NULL) {
		printf("no bsdsocket.library\n");
		return 20;
	}

	/* 1 */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd >= 0) {
		struct sockaddr_in to, me;
		socklen_t len = sizeof(me);
		long r;
		memset(&to, 0, sizeof(to));
		to.sin_family = AF_INET;
		to.sin_port = htons(9);
		to.sin_addr.s_addr = htonl(0xc0000201UL);
		r = connect(fd, (struct sockaddr *)&to, sizeof(to));
		printf("1 connect: %ld (errno %ld)\n", r, (long)Errno());
		memset(&me, 0, sizeof(me));
		r = getsockname(fd, (struct sockaddr *)&me, &len);
		printf("1 getsockname: %ld (errno %ld) len %ld\n", r, (long)Errno(), (long)len);
		show("1 address", me.sin_addr.s_addr);
		/* the same towards the local network (broadcast-free: 192.168.9.1) */
		to.sin_addr.s_addr = htonl(0xc0a80901UL);
		r = connect(fd, (struct sockaddr *)&to, sizeof(to));
		memset(&me, 0, sizeof(me));
		len = sizeof(me);
		getsockname(fd, (struct sockaddr *)&me, &len);
		printf("1b connect to 192.168.9.1: %ld, ", r);
		show("address", me.sin_addr.s_addr);
		CloseSocket(fd);
	}

	/* 2 */
	{
		char host[64];
		long r = gethostname(host, sizeof(host));
		printf("2 gethostname: %ld '%s'\n", r, r == 0 ? host : "");
		if (r == 0) {
			struct hostent *he = gethostbyname((STRPTR)host);
			if (he != NULL && he->h_addr_list[0] != NULL) {
				struct in_addr a;
				memcpy(&a, he->h_addr_list[0], sizeof(a));
				show("2 gethostbyname", a.s_addr);
			} else {
				printf("2 gethostbyname: none (errno %ld)\n", (long)Errno());
			}
		}
	}

	/* 3 */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd >= 0) {
		static char buf[1024];
		struct ifconf ifc;
		long r;
		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = buf;
		r = IoctlSocket(fd, SIOCGIFCONF, (char *)&ifc);
		printf("3 SIOCGIFCONF: %ld (errno %ld) len %ld\n", r, (long)Errno(), (long)ifc.ifc_len);
		if (r == 0) {
			char *p = buf, *end = buf + ifc.ifc_len;
			while (p < end) {
				struct ifreq *ifr = (struct ifreq *)p;
				int sz = sizeof(ifr->ifr_name) + (ifr->ifr_addr.sa_len > sizeof(struct sockaddr) ? ifr->ifr_addr.sa_len : sizeof(struct sockaddr));
				if (ifr->ifr_addr.sa_family == AF_INET) {
					printf("3 %.16s: ", ifr->ifr_name);
					show("", ((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr.s_addr);
				}
				p += sz;
			}
		}
		CloseSocket(fd);
	}

	/* 4: WaitSelect and a pending Ctrl-C */
	{
		struct timeval tv;
		ULONG sigs = SIGBREAKF_CTRL_C;
		long r;
		SetSignal(SIGBREAKF_CTRL_C, SIGBREAKF_CTRL_C);	/* pending already */
		tv.tv_secs = 1;
		tv.tv_micro = 0;
		r = WaitSelect(0, NULL, NULL, NULL, &tv, &sigs);
		printf("4 WaitSelect with a pending Ctrl-C: %ld, signals %08lx (expected 00001000)\n", r, (unsigned long)sigs);
		SetSignal(0, SIGBREAKF_CTRL_C);
	}

	CloseLibrary(SocketBase);
	return 0;
}
