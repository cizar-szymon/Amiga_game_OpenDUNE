/** @file src/audio/dsp_amiga.c Paula (audio.device) implementation of the DSP.
 *
 * Dune II's voices and sound samples are 8-bit unsigned VOC blocks at
 * various rates. Paula plays each one directly at its own rate (period =
 * clock / rate), on one left and one right channel, from a buffer in chip
 * RAM: no mixing and no resampling, so no CPU time while it plays. */

#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/audio.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <clib/alib_protos.h>

#undef Debug

#include "types.h"
#include "../os/endian.h"
#include "../os/error.h"

#include "dsp.h"

#define MAX_LEN 131070	/* audio.device: up to 65535 words */

static struct MsgPort *s_port;
static struct IOAudio *s_alloc;
static struct IOAudio *s_io[2];		/* left, right */
static uint8 *s_chip;
static uint32 s_chipSize;
static ULONG s_clock;
static bool s_playing;

void DSP_Stop(void)
{
	int c;
	if (!s_playing) return;
	for (c = 0; c < 2; c++) {
		if (!CheckIO((struct IORequest *)s_io[c])) AbortIO((struct IORequest *)s_io[c]);
		WaitIO((struct IORequest *)s_io[c]);
	}
	s_playing = false;
}

void DSP_Uninit(void)
{
	int c;
	DSP_Stop();
	for (c = 0; c < 2; c++) {
		if (s_io[c] != NULL) FreeVec(s_io[c]);
		s_io[c] = NULL;
	}
	if (s_alloc != NULL) {
		if (s_alloc->ioa_Request.io_Device != NULL) CloseDevice((struct IORequest *)s_alloc);
		DeleteIORequest((struct IORequest *)s_alloc);
		s_alloc = NULL;
	}
	if (s_port != NULL) { DeleteMsgPort(s_port); s_port = NULL; }
	if (s_chip != NULL) { FreeVec(s_chip); s_chip = NULL; }
	s_chipSize = 0;
}

bool DSP_Init(void)
{
	static UBYTE channels[] = { 3, 5, 10, 12 };	/* a left and a right one */
	ULONG units;
	int c;

	s_clock = (GfxBase->DisplayFlags & PAL) ? 3546895UL : 3579545UL;
	s_port = CreateMsgPort();
	if (s_port == NULL) return false;
	s_alloc = (struct IOAudio *)CreateIORequest(s_port, sizeof(struct IOAudio));
	if (s_alloc == NULL) { DSP_Uninit(); return false; }
	s_alloc->ioa_Request.io_Message.mn_Node.ln_Pri = 1;
	s_alloc->ioa_Request.io_Flags = ADIOF_NOWAIT;
	s_alloc->ioa_Data = channels;
	s_alloc->ioa_Length = sizeof(channels);
	if (OpenDevice((CONST_STRPTR)AUDIONAME, 0, (struct IORequest *)s_alloc, 0) != 0) {
		s_alloc->ioa_Request.io_Device = NULL;
		Warning("audio.device: no free channels\n");
		DSP_Uninit();
		return false;
	}
	units = (ULONG)s_alloc->ioa_Request.io_Unit;
	for (c = 0; c < 2; c++) {
		s_io[c] = (struct IOAudio *)AllocVec(sizeof(struct IOAudio), MEMF_PUBLIC | MEMF_CLEAR);
		if (s_io[c] == NULL) { DSP_Uninit(); return false; }
		*s_io[c] = *s_alloc;
		/* left: channels 0 and 3, right: 1 and 2 */
		s_io[c]->ioa_Request.io_Unit = (struct Unit *)(units & (c ? 0x06 : 0x09));
	}
	atexit(DSP_Uninit);
	return true;
}

void DSP_Play(const uint8 *data)
{
	uint32 len, freq, i, period;
	int c;

	if (s_io[0] == NULL) return;
	DSP_Stop();

	/* skip Create Voice File header */
	data += READ_LE_UINT16(data + 20);
	/* only block type 1 (sound data) */
	if (*data != 1) return;
	len = data[1] | (data[2] << 8) | ((uint32)data[3] << 16);
	len -= 2;
	data += 4;
	/* byte 0: frequency divisor, byte 1: codec (0 = 8-bit unsigned PCM) */
	freq = 1000000 / (256 - data[0]);
	if (data[1] != 0) Warning("Unsupported VOC codec 0x%02x\n", (int)data[1]);
	data += 2;
	if (len > MAX_LEN) len = MAX_LEN;
	len &= ~1UL;
	if (len < 2) return;

	if (len > s_chipSize) {
		if (s_chip != NULL) FreeVec(s_chip);
		s_chipSize = (len + 4095) & ~4095UL;
		s_chip = AllocVec(s_chipSize, MEMF_CHIP);
		if (s_chip == NULL) { s_chipSize = 0; return; }
	}
	for (i = 0; i < len; i++) s_chip[i] = data[i] ^ 0x80;	/* unsigned -> signed */

	period = (s_clock + freq / 2) / freq;
	if (period < 124) period = 124;
	for (c = 0; c < 2; c++) {
		struct IOAudio *io = s_io[c];
		io->ioa_Request.io_Command = CMD_WRITE;
		io->ioa_Request.io_Flags = ADIOF_PERVOL;
		io->ioa_Data = s_chip;
		io->ioa_Length = len;
		io->ioa_Period = (UWORD)period;
		io->ioa_Volume = 64;
		io->ioa_Cycles = 1;
		BeginIO((struct IORequest *)io);
	}
	s_playing = true;
}

/**
 * Should return 2 if playing sound, 0 if not
 */
uint8 DSP_GetStatus(void)
{
	if (!s_playing) return 0;
	if (CheckIO((struct IORequest *)s_io[0]) && CheckIO((struct IORequest *)s_io[1])) {
		WaitIO((struct IORequest *)s_io[0]);
		WaitIO((struct IORequest *)s_io[1]);
		s_playing = false;
		return 0;
	}
	return 2;
}
