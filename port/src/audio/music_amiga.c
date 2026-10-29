/** @file src/audio/music_amiga.c The music (AmigaOS): recordings played from disk.
 *
 * The AdLib music of Dune II recorded once on the PC (tools/music/make_music.py)
 * into PROGDIR:MUSIC (ini music_dir), one file per piece of g_table_musics:
 * DUNE<n>_<track>.RAW = 'ODMU', loop start, length (32-bit big endian, loop
 * 0xFFFFFFFF: plays once), then 8-bit signed mono at 22030 Hz.
 *
 * Paula plays it on the two channels the voices (dsp_amiga.c) left free, a
 * left and a right one with the same samples: no mixing, no CPU time but the
 * reading. MUSIC_BUFFERS buffers in chip RAM are queued to audio.device, each
 * one filled again from the file when both channels have played it
 * (Music_Amiga_Tick, every tick of the video driver, on every screen). A
 * missing file or directory: silence, as before.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/audio.h>
#include <dos/dos.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <clib/alib_protos.h>

#undef Debug

#include "types.h"
#include "../inifile.h"
#include "../os/error.h"
#include "../timer.h"

#include "music_amiga.h"

#define MUSIC_RATE     22030
#define MUSIC_BUFFERS  4
#define MUSIC_BUFSIZE  11016		/* half a second, even */
#define MUSIC_FADE_MS  2000

typedef struct Buffer {
	struct IOAudio io[2];		/* left, right */
	bool pending[2];		/* sent, not back yet */
} Buffer;

static struct MsgPort *s_port;
static struct IOAudio *s_alloc;
static ULONG s_units;			/* the two channels */
static uint8 *s_chip;
static Buffer s_buf[MUSIC_BUFFERS];
static UWORD s_period;
static int s_volume = 40;		/* 0..64, ini music_volume */
static char s_dir[128] = "PROGDIR:MUSIC";
static bool s_ready;

static char s_path[192];		/* the piece playing (tests: od.py --state) */
static BPTR s_file;
static uint32 s_loop, s_length, s_pos;	/* samples */
static bool s_atEnd;			/* the file is read to its end (no loop) */
static bool s_fading;
static uint32 s_fadeStart;

static void Music_Amiga_Uninit(void);

/* Stops what plays and waits for all requests. */
static void Music_Amiga_Halt(void)
{
	int b, c;

	for (b = 0; b < MUSIC_BUFFERS; b++) {
		for (c = 0; c < 2; c++) {
			struct IORequest *io = (struct IORequest *)&s_buf[b].io[c];
			if (!s_buf[b].pending[c]) continue;	/* (back and taken: not waited for again) */
			if (CheckIO(io) == NULL) AbortIO(io);
			WaitIO(io);
			s_buf[b].pending[c] = false;
		}
	}
	if (s_file != 0) {
		Close(s_file);
		s_file = 0;
	}
	s_fading = false;
}

bool Music_Amiga_Init(void)
{
	static UBYTE channels[] = { 3, 5, 10, 12 };	/* a left and a right one */
	ULONG clock;
	int b, c;

	if (s_ready) return true;
	s_volume = IniFile_GetInteger("music_volume", 40);
	if (s_volume < 0) s_volume = 0;
	if (s_volume > 64) s_volume = 64;
	IniFile_GetString("music_dir", "PROGDIR:MUSIC", s_dir, sizeof(s_dir));

	clock = (GfxBase->DisplayFlags & PAL) ? 3546895UL : 3579545UL;
	s_period = (UWORD)((clock + MUSIC_RATE / 2) / MUSIC_RATE);
	s_port = CreateMsgPort();
	s_alloc = s_port != NULL ? (struct IOAudio *)CreateIORequest(s_port, sizeof(struct IOAudio)) : NULL;
	s_chip = AllocVec(MUSIC_BUFFERS * MUSIC_BUFSIZE, MEMF_CHIP | MEMF_CLEAR);
	if (s_alloc == NULL || s_chip == NULL) {
		Music_Amiga_Uninit();
		return false;
	}
	s_alloc->ioa_Request.io_Message.mn_Node.ln_Pri = 0;	/* below the voices */
	s_alloc->ioa_Request.io_Flags = ADIOF_NOWAIT;
	s_alloc->ioa_Data = channels;
	s_alloc->ioa_Length = sizeof(channels);
	if (OpenDevice((CONST_STRPTR)AUDIONAME, 0, (struct IORequest *)s_alloc, 0) != 0) {
		s_alloc->ioa_Request.io_Device = NULL;
		Warning("music: no free audio channels\n");
		Music_Amiga_Uninit();
		return false;
	}
	s_units = (ULONG)s_alloc->ioa_Request.io_Unit;
	for (b = 0; b < MUSIC_BUFFERS; b++) {
		for (c = 0; c < 2; c++) {
			struct IOAudio *io = &s_buf[b].io[c];
			*io = *s_alloc;
			/* left: channels 0 and 3, right: 1 and 2 */
			io->ioa_Request.io_Unit = (struct Unit *)(s_units & (c ? 0x06 : 0x09));
			io->ioa_Request.io_Command = CMD_WRITE;
			io->ioa_Request.io_Flags = ADIOF_PERVOL;
			io->ioa_Data = s_chip + b * MUSIC_BUFSIZE;
			io->ioa_Length = MUSIC_BUFSIZE;
			io->ioa_Period = s_period;
			io->ioa_Cycles = 1;
		}
	}
	s_ready = true;
	atexit(Music_Amiga_Uninit);
	return true;
}

static void Music_Amiga_Uninit(void)
{
	if (s_ready) Music_Amiga_Halt();
	s_ready = false;
	if (s_alloc != NULL) {
		if (s_alloc->ioa_Request.io_Device != NULL) CloseDevice((struct IORequest *)s_alloc);
		DeleteIORequest((struct IORequest *)s_alloc);
		s_alloc = NULL;
	}
	if (s_port != NULL) { DeleteMsgPort(s_port); s_port = NULL; }
	if (s_chip != NULL) { FreeVec(s_chip); s_chip = NULL; }
}

/* Reads the next n samples into dst (after the end: the loop, or silence). */
static void Music_Amiga_Read(int8 *dst, uint32 n)
{
	while (n > 0) {
		uint32 k = s_length - s_pos;
		LONG got;

		if (k == 0) {
			if (s_loop < s_length && Seek(s_file, 12 + s_loop, OFFSET_BEGINNING) >= 0) {
				s_pos = s_loop;
				continue;
			}
			s_atEnd = true;
			memset(dst, 0, n);
			return;
		}
		if (k > n) k = n;
		got = Read(s_file, dst, k);
		if (got <= 0) {
			s_length = s_pos;	/* a short file: its end here */
			continue;
		}
		s_pos += got;
		dst += got;
		n -= got;
	}
}

/* The volume now (the fade out lowers it). */
static UWORD Music_Amiga_Volume(void)
{
	uint32 t;
	if (!s_fading) return (UWORD)s_volume;
	t = Timer_GetTime() - s_fadeStart;
	if (t >= MUSIC_FADE_MS) return 0;
	return (UWORD)(s_volume * (MUSIC_FADE_MS - t) / MUSIC_FADE_MS);
}

/* Fills buffer b and sends it to both channels. */
static void Music_Amiga_Queue(int b)
{
	int c;
	UWORD volume = Music_Amiga_Volume();

	Music_Amiga_Read((int8 *)(s_chip + b * MUSIC_BUFSIZE), MUSIC_BUFSIZE);
	for (c = 0; c < 2; c++) {
		struct IOAudio *io = &s_buf[b].io[c];
		io->ioa_Request.io_Command = CMD_WRITE;
		io->ioa_Request.io_Flags = ADIOF_PERVOL;
		io->ioa_Data = s_chip + b * MUSIC_BUFSIZE;
		io->ioa_Length = MUSIC_BUFSIZE;
		io->ioa_Period = s_period;
		io->ioa_Volume = volume;
		io->ioa_Cycles = 1;
		BeginIO((struct IORequest *)io);
		s_buf[b].pending[c] = true;
	}
}

bool Music_Amiga_Play(const char *name, int track)
{
	char path[192], *p;
	uint8 header[12];
	int b;

	if (!s_ready) {
		/* the first piece: after the voices took their channels */
		static bool tried;
		if (tried) return false;
		tried = true;
		if (!Music_Amiga_Init()) return false;
	}
	Music_Amiga_Halt();
	snprintf(path, sizeof(path), "%s/%s_%d.RAW", s_dir, name, track);
	for (p = path; *p != '\0'; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
	strcpy(s_path, path);
	s_file = Open((CONST_STRPTR)path, MODE_OLDFILE);
	if (s_file == 0) return false;
	if (Read(s_file, header, 12) != 12 || memcmp(header, "ODMU", 4) != 0) {
		Close(s_file);
		s_file = 0;
		return false;
	}
	s_loop   = ((uint32)header[4] << 24) | ((uint32)header[5] << 16) | ((uint32)header[6] << 8) | header[7];
	s_length = ((uint32)header[8] << 24) | ((uint32)header[9] << 16) | ((uint32)header[10] << 8) | header[11];
	s_pos = 0;
	s_atEnd = false;

	/* all buffers queued with both channels stopped, then started together */
	s_alloc->ioa_Request.io_Command = CMD_STOP;
	s_alloc->ioa_Request.io_Unit = (struct Unit *)s_units;
	DoIO((struct IORequest *)s_alloc);
	for (b = 0; b < MUSIC_BUFFERS; b++) Music_Amiga_Queue(b);
	s_alloc->ioa_Request.io_Command = CMD_START;
	s_alloc->ioa_Request.io_Unit = (struct Unit *)s_units;
	DoIO((struct IORequest *)s_alloc);
	return true;
}

void Music_Amiga_Tick(void)
{
	struct IOAudio *io;
	int b;

	if (!s_ready) return;
	while ((io = (struct IOAudio *)GetMsg(s_port)) != NULL) {
		for (b = 0; b < MUSIC_BUFFERS; b++) {
			if (io == &s_buf[b].io[0]) s_buf[b].pending[0] = false;
			if (io == &s_buf[b].io[1]) s_buf[b].pending[1] = false;
		}
	}
	if (s_fading && Music_Amiga_Volume() == 0) {
		Music_Amiga_Halt();
		return;
	}
	if (s_file == 0) return;
	for (b = 0; b < MUSIC_BUFFERS; b++) {
		if (s_buf[b].pending[0] || s_buf[b].pending[1]) continue;
		if (s_atEnd) continue;		/* the end: the queued ones play out */
		Music_Amiga_Queue(b);
	}
	if (s_atEnd) {
		for (b = 0; b < MUSIC_BUFFERS; b++) if (s_buf[b].pending[0] || s_buf[b].pending[1]) return;
		Close(s_file);			/* played out */
		s_file = 0;
	}
}

void Music_Amiga_Stop(void)
{
	if (s_ready) Music_Amiga_Halt();
}

void Music_Amiga_FadeOut(void)
{
	if (!s_ready || s_file == 0 || s_fading) return;
	s_fading = true;
	s_fadeStart = Timer_GetTime();
}

/** What plays (the file, "-" when nothing, with " fading" while it fades). */
const char *Music_Amiga_Now(void)
{
	static char text[208];
	if (!Music_Amiga_IsPlaying()) return "-";
	sprintf(text, "%s at %lu s%s", s_path, (unsigned long)(s_pos / MUSIC_RATE), s_fading ? " fading" : "");
	return text;
}

bool Music_Amiga_IsPlaying(void)
{
	return s_ready && s_file != 0;
}
