/*
 * MakeMusic - records the music of Dune II for the OpenDUNE Amiga port on the
 * Amiga itself, from the player's own game data (no PC needed).
 *
 *   MakeMusic [DATA] [TO] [PEAK]
 *     DATA  the directory with SOUND.PAK of Dune II for the PC (default PROGDIR:data)
 *     TO    where the recordings go (default PROGDIR:MUSIC, made if missing)
 *     PEAK  (tests) only print the loudest sample of all pieces
 *
 * The AdLib music (the .ADL files in SOUND.PAK) is played by AdPlug's Westwood
 * ADL player (adl.cpp) on the OPL2 emulator of MAME (fmopl.c, whole numbers,
 * quick enough for a 68040/68060), each of the 37 pieces of the game's music
 * table straight at 22030 Hz (Paula PAL period 161) into its own file,
 * DUNE<n>_<track>.RAW: 'ODMU', the loop start, the length (32-bit big endian,
 * loop 0xFFFFFFFF: plays once), then 8-bit signed mono - the format
 * tools/music/make_music.py makes on a PC and audio/music_amiga.c plays.
 * A piece the original loops is recorded once with its loop start: found in
 * the notes struck each tick (the driver's random effects make them repeat
 * not quite exactly), as make_music.py does. Pieces the AdLib version does not
 * have (silent) get no file.
 *
 * Builds for the Amiga (bebbo m68k-amigaos-g++, tools/music/amiga/build.sh) and
 * for a PC (the same sources, for comparing).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <sys/stat.h>

#include "adl.h"
#include "emuopl.h"

#if defined(__amigaos__)
#define DEFAULT_DATA  "PROGDIR:data"
#define DEFAULT_TO    "PROGDIR:MUSIC"
/* a stack of this size also when started from the icon (libnix: swapstack.o
 * is linked in only when __stkinit is referred to) */
unsigned long __stack = 65536;
extern "C" void __stkinit(void);
void (*const makemusic_stackSwap)(void) = __stkinit;
const char version[] = "$VER: MakeMusic 1.1 (31.10.2026) OpenDUNE music recorder";
/* the window for the output when started from the icon: libnix's own has WAIT
 * (it stays open after the end until its close gadget), here Return closes it
 * (Finish) */
extern "C" { const char *__stdiowin = "CON://640/200/MakeMusic/AUTO/CLOSE"; }
#else
#define DEFAULT_DATA  "data"
#define DEFAULT_TO    "MUSIC"
#endif

#define RATE         22030		/* Paula, PAL, period 161 */
#define MAX_SECONDS  400		/* a looping piece: two loops of up to 200 s */
#define TICK_HZ      72			/* the ADL player's tick */
/* one gain for all pieces (they keep their loudness to each other): the
 * loudest sample of all of them (PEAK) at 127 */
#define PEAK16       16384L

/* g_table_musics of the game (port/src/table/sound.c): file, track */
static const struct { const char *file; int track; } s_musics[] = {
	{"DUNE1", 2}, {"DUNE1", 3}, {"DUNE1", 4}, {"DUNE1", 5}, {"DUNE17", 4}, {"DUNE8", 3}, {"DUNE8", 2},
	{"DUNE1", 6}, {"DUNE2", 6}, {"DUNE3", 6}, {"DUNE4", 6}, {"DUNE5", 6}, {"DUNE6", 6}, {"DUNE9", 4},
	{"DUNE9", 5}, {"DUNE18", 6}, {"DUNE10", 7}, {"DUNE11", 7}, {"DUNE12", 7}, {"DUNE13", 7}, {"DUNE14", 7},
	{"DUNE15", 7}, {"DUNE1", 8}, {"DUNE7", 2}, {"DUNE7", 3}, {"DUNE7", 4}, {"DUNE0", 2}, {"DUNE7", 6},
	{"DUNE16", 7}, {"DUNE19", 4}, {"DUNE19", 2}, {"DUNE19", 3}, {"DUNE20", 2}, {"DUNE16", 8}, {"DUNE0", 3},
	{"DUNE0", 4}, {"DUNE0", 5},
};

#define HASH_START 0x811C9DC5UL

/* The OPL, the notes struck in each tick hashed on the way (for the loop). */
class LogOpl : public Copl
{
public:
	LogOpl(int rate) : real(rate, true, false), cur(HASH_START) { memset(regs, 0, sizeof(regs)); }
	void write(int reg, int val)
	{
		real.write(reg, val);
		reg &= 0xFF;
		if (reg >= 0xB0 && reg <= 0xB8 && (val & 0x20) != 0 && (regs[reg] & 0x20) == 0) {
			mix(((unsigned long)reg << 16) | ((unsigned long)(val & 0x1F) << 8) | regs[reg - 0x10]);
		}
		if (reg == 0xBD && (val & ~regs[reg] & 0x1F) != 0) mix(0xBD00UL | (unsigned long)(val & ~regs[reg] & 0x1F));
		regs[reg] = (unsigned char)val;
	}
	void init() { real.init(); }
	void update(short *buf, int samples) { real.update(buf, samples); }
	unsigned long take() { unsigned long h = cur; cur = HASH_START; return h; }
private:
	void mix(unsigned long v) { cur = ((cur ^ v) * 16777619UL) & 0xFFFFFFFFUL; }
	CEmuopl real;
	unsigned long cur;
	unsigned char regs[256];
};

/* A file of a Westwood PAK (in memory), NULL if not there. */
static const unsigned char *Pak_Find(const unsigned char *pak, long size, const char *name, long *len)
{
	long i = 0;
	while (i + 4 < size) {
		long off = pak[i] | (pak[i + 1] << 8) | ((long)pak[i + 2] << 16) | ((long)pak[i + 3] << 24);
		const char *n = (const char *)pak + i + 4;
		long nlen = (long)strlen(n), next;
		if (off == 0) break;
		next = i + 4 + nlen + 1;
		if (strcasecmp(n, name) == 0) {
			long end = size;
			if (next + 4 <= size) {
				long noff = pak[next] | (pak[next + 1] << 8) | ((long)pak[next + 2] << 16) | ((long)pak[next + 3] << 24);
				if (noff != 0) end = noff;
			}
			*len = end - off;
			return pak + off;
		}
		i = next;
	}
	return NULL;
}

/*
 * The loop of a piece that plays on (render() stopped at MAX_SECONDS): the
 * shortest period L whose notes match within a point of the best one over
 * the second half; its start S the first struck tick from which the rest
 * matches nearly as well. Start and end in samples, false: none found.
 */
static bool FindLoop(const unsigned long *h, const long *pos, long n, long *start, long *end)
{
	long *late = (long *)malloc(sizeof(long) * n), nl = 0, L, best = 0, bestL = 0, t;
	long *scores = (long *)malloc(sizeof(long) * (n / 2 + 1));
	bool ok = false;

	if (late == NULL || scores == NULL) { free(late); free(scores); return false; }
	for (t = n / 2; t < n; t++) if (h[t] != HASH_START) late[nl++] = t;
	if (nl == 0) { free(late); free(scores); return false; }
	for (L = 5 * TICK_HZ; L < n / 2; L++) {
		long same = 0, k;
		for (k = 0; k < nl; k++) if (h[late[k]] == h[late[k] - L]) same++;
		scores[L] = same * 1000 / nl;		/* per mille */
		if (scores[L] > best) best = scores[L];
	}
	for (L = 5 * TICK_HZ; L < n / 2; L++) if (scores[L] >= best - 10) { bestL = L; break; }
	if (bestL > 0 && best >= 500) {
		/* the start: the first struck tick from which the rest matches
		 * within 3 points of the best */
		long na = 0, k, matches = 0;
		long *act = late;			/* (reused) */
		for (t = 0; t < n - bestL; t++) if (h[t] != HASH_START) act[na++] = t;
		for (k = 0; k < na; k++) if (h[act[k]] == h[act[k] + bestL]) matches++;
		/* forwards: the share of matches in [k, na) */
		for (k = 0; k < na; k++) {
			if (matches * 1000 / (na - k) >= best - 30) {
				*start = pos[act[k]];
				*end = pos[act[k] + bestL];
				ok = true;
				break;
			}
			if (h[act[k]] == h[act[k] + bestL]) matches--;
		}
	}
	free(late);
	free(scores);
	return ok;
}

/* One piece: the samples (8-bit), its loop start (-1 none); NULL if silent. */
static signed char *Render(const unsigned char *adl, long adlLen, int track, long *length, long *loop,
                           long *peak, bool peakOnly)
{
	LogOpl opl(RATE);
	CadlPlayer player(&opl);
	long maxSamples = (long)MAX_SECONDS * RATE, total = 0, tail = -1, ticks = 0, maxTicks = MAX_SECONDS * TICK_HZ + 2;
	signed char *out = peakOnly ? NULL : (signed char *)malloc(maxSamples + RATE);
	unsigned long *hash = (unsigned long *)malloc(sizeof(unsigned long) * maxTicks);
	long *pos = (long *)malloc(sizeof(long) * maxTicks);
	short buf[RATE / TICK_HZ + 2];
	long due = 0, myPeak = 0;		/* samples due, in 1/TICK_HZ units */
	bool ended = false;

	*loop = -1;
	if ((!peakOnly && out == NULL) || hash == NULL || pos == NULL ||
	    !player.load("x.adl", CFileProvider(adl, (unsigned long)adlLen))) {
		free(out); free(hash); free(pos);
		return NULL;
	}
	player.rewind(-1);
	player.rewind(track);
	while (total < maxSamples && ticks < maxTicks) {
		bool playing = player.update();
		long n, i;
		hash[ticks] = opl.take();
		pos[ticks] = total;
		ticks++;
		if (!playing && tail < 0) {
			tail = RATE;		/* one second for the release */
			ended = true;
		}
		due += RATE;
		n = due / TICK_HZ;
		due -= n * TICK_HZ;
		opl.update(buf, (int)n);
		for (i = 0; i < n; i++) {
			long v = buf[i];
			if (v < 0 ? -v > myPeak : v > myPeak) myPeak = v < 0 ? -v : v;
			if (out != NULL) {
				long s = (v * 127 + (v < 0 ? -PEAK16 / 2 : PEAK16 / 2)) / PEAK16;
				out[total + i] = (signed char)(s > 127 ? 127 : s < -128 ? -128 : s);
			}
		}
		total += n;
		if (tail >= 0 && (tail -= n) <= 0) break;
	}
	*peak = myPeak;
	*length = total;
	if (!ended) {
		long start, end;
		if (FindLoop(hash, pos, ticks, &start, &end)) {
			*loop = start;
			*length = end;
		}
	}
	free(hash);
	free(pos);
	if (myPeak < 64) {			/* nothing in the AdLib version */
		free(out);
		return NULL;
	}
	return out;
}

static bool WriteRaw(const char *path, const signed char *s, long length, long loop)
{
	unsigned char head[12];
	unsigned long lp = loop < 0 ? 0xFFFFFFFFUL : (unsigned long)loop;
	FILE *f = fopen(path, "wb");
	int i;
	if (f == NULL) return false;
	memcpy(head, "ODMU", 4);
	for (i = 0; i < 4; i++) {
		head[4 + i] = (unsigned char)(lp >> (24 - 8 * i));
		head[8 + i] = (unsigned char)((unsigned long)length >> (24 - 8 * i));
	}
	if (fwrite(head, 12, 1, f) != 1 || fwrite(s, 1, (size_t)length, f) != (size_t)length) {
		fclose(f);
		return false;
	}
	return fclose(f) == 0;
}

static int s_fromIcon;	/* started from the Workbench: the window waits for Return */

/* dir + name: no '/' after a volume ("DDATA:" + "x" = "DDATA:x", a '/' there
 * would be the parent directory on the Amiga) or after a '/' */
static void Join(char *out, size_t size, const char *dir, const char *name)
{
	size_t n = strlen(dir);
	const char *sep = (n == 0 || dir[n - 1] == ':' || dir[n - 1] == '/') ? "" : "/";
	snprintf(out, size, "%s%s%s", dir, sep, name);
}

/* key=value of the game's opendune.ini next to MakeMusic (datadir, music_dir),
 * into value; false if not there or commented out */
static bool Ini_Get(const char *key, char *value, size_t size)
{
#if defined(__amigaos__)
	FILE *f = fopen("PROGDIR:opendune.ini", "r");
#else
	FILE *f = fopen("opendune.ini", "r");
#endif
	char line[256];
	size_t klen = strlen(key);
	bool found = false;

	if (f == NULL) return false;
	while (fgets(line, sizeof(line), f) != NULL) {
		char *p = line, *e;
		while (*p == ' ' || *p == '\t') p++;
		if (strncasecmp(p, key, klen) != 0 || p[klen] != '=') continue;
		p += klen + 1;
		for (e = p + strlen(p); e > p && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '); e--) {}
		*e = '\0';
		if (*p == '\0') continue;
		strncpy(value, p, size - 1);
		value[size - 1] = '\0';
		found = true;
	}
	fclose(f);
	return found;
}

static int Finish(int rc)
{
	if (s_fromIcon) {
		printf("Press Return to close this window.\n");
		fflush(stdout);
		getchar();
	}
	return rc;
}

int main(int argc, char **argv)
{
	const char *data = DEFAULT_DATA, *to = DEFAULT_TO;
	bool peakOnly = false;
	char path[512];
	unsigned char *pak;
	long pakSize, made = 0, silent = 0, allPeak = 0;
	time_t t0 = time(NULL);
	FILE *f;
	unsigned m;

	static char iniData[256], iniTo[256];

	s_fromIcon = argc == 0;	/* (libnix: argc 0 from the Workbench, its window for the output) */
	/* the game's own settings first, the arguments win */
	if (Ini_Get("datadir", iniData, sizeof(iniData))) {
		size_t n = strlen(iniData);
		if (n > 1 && iniData[n - 1] == '/') iniData[n - 1] = '\0';	/* "Work:Dune2/" */
		data = iniData;
	}
	if (Ini_Get("music_dir", iniTo, sizeof(iniTo))) to = iniTo;
	if (argc > 1 && strcasecmp(argv[1], "PEAK") != 0) data = argv[1];
	/* (a directory given as "Work:Dune2/" is fine too) */
	if (argc > 2 && strcasecmp(argv[2], "PEAK") != 0) to = argv[2];
	for (int i = 1; i < argc; i++) if (strcasecmp(argv[i], "PEAK") == 0) peakOnly = true;

	printf("MakeMusic: the music of Dune II for OpenDUNE, from %s (SOUND.PAK) to %s\n", data, to);
	Join(path, sizeof(path), data, "SOUND.PAK");
	f = fopen(path, "rb");
	if (f == NULL) {
		printf("\nCannot open %s.\n"
		       "Copy the Dune II PC data files (1.07) into %s first.\n\n"
		       "Brak pliku %s.\n"
		       "Najpierw skopiuj pliki danych Dune II z PC (1.07) do %s.\n\n"
		       "%s fehlt.\n"
		       "Kopiere zuerst die Dateien von Dune II f\xfcr den PC (1.07) nach %s.\n\n"
		       "Install.txt\n\n", path, data, path, data, path, data);
		return Finish(20);
	}
	fseek(f, 0, SEEK_END);
	pakSize = ftell(f);
	fseek(f, 0, SEEK_SET);
	pak = (unsigned char *)malloc(pakSize);
	if (pak == NULL || fread(pak, 1, pakSize, f) != (size_t)pakSize) {
		printf("Cannot read %s\n", path);
		fclose(f);
		return Finish(20);
	}
	fclose(f);
	if (!peakOnly) {
#if defined(__amigaos__)
		mkdir(to, 0);
#else
		mkdir(to, 0755);
#endif
	}

	for (m = 0; m < sizeof(s_musics) / sizeof(s_musics[0]); m++) {
		char name[16];
		long adlLen, length, loop, peak;
		const unsigned char *adl;
		signed char *s;
		time_t t1 = time(NULL);

		snprintf(name, sizeof(name), "%s.ADL", s_musics[m].file);
		adl = Pak_Find(pak, pakSize, name, &adlLen);
		printf("%2u/%u %s_%d ", m + 1, (unsigned)(sizeof(s_musics) / sizeof(s_musics[0])), s_musics[m].file, s_musics[m].track);
		fflush(stdout);
		if (adl == NULL) {
			printf("- no %s in SOUND.PAK\n", name);
			continue;
		}
		s = Render(adl, adlLen, s_musics[m].track, &length, &loop, &peak, peakOnly);
		if (peak > allPeak) allPeak = peak;
		if (s == NULL) {
			printf("%s\n", peakOnly ? "" : "- silent in the AdLib version, no file");
			if (!peakOnly) silent++;
			continue;
		}
		{
			char file[32];
			snprintf(file, sizeof(file), "%s_%d.RAW", s_musics[m].file, s_musics[m].track);
			Join(path, sizeof(path), to, file);
		}
		if (!WriteRaw(path, s, length, loop)) {
			printf("\n\nCannot write %s (disk full? about 75 MB are needed).\n"
			       "Nie mozna zapisac %s (pelny dysk? potrzeba ok. 75 MB).\n"
			       "%s kann nicht geschrieben werden (Platte voll? etwa 75 MB n\xf6tig).\n\n",
			       path, path, path);
			free(s);
			free(pak);
			return Finish(20);
		}
		free(s);
		made++;
		if (loop >= 0) printf("%ld s, loop from %ld s", length / RATE, loop / RATE);
		else printf("%ld s", length / RATE);
		printf(" (%ld s)\n", (long)(time(NULL) - t1));
	}
	free(pak);
	if (peakOnly) {
		printf("peak %ld\n", allPeak);
		return 0;
	}
	printf("%ld pieces recorded, %ld silent, in %ld s. Done: the game plays them from %s.\n",
	       made, silent, (long)(time(NULL) - t0), to);
	return Finish(0);
}
