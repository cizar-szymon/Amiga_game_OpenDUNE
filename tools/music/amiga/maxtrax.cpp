/*
 * MaxTrax (.MX) recorded to 8-bit samples - the music of the Amiga Dune II
 * (DUNEGAME.ENG/GER/FRE score 19: the missions' music) for the OpenDUNE port.
 *
 * The same player as tools/music/mx_render.py, written for this port from the
 * format and how the MaxTrax driver behaves (as ScummVM's maxtrax.cpp
 * describes it; no code of it): a 50 Hz interrupt, 4 Paula voices (0, 3 left,
 * 1, 2 right), 16 channels, patches = samples in up to several octaves with
 * attack/release envelopes in ms. Whole numbers only (no FPU, no 64-bit
 * multiplications the 68060 would trap): the period from a table of powers of
 * two, the sample positions in 16.16.
 */
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "maxtrax.h"

#define VBLANK        50
#define PAULA         3546895L	/* PAL Paula clock */
#define K_VALUE       0x9fd77L
#define PREF_PERIOD   0x8fd77L
#define PERIOD_LIMIT  0x6f73dL

namespace {

struct Patch {
	bool used;
	int tune, volume, octaves;
	long attack, total;
	const unsigned char *env;	/* (duration, volume) x (attack + release) */
	int attackEnv, releaseEnv;
	const signed char *data;
};

struct Channel {
	int patch, volume, bendRange, right;
	long pitchReal;
	int rpn;
};

struct Voice {
	int status;	/* 0 free, 1 halt, 2 decay, 3 release, 4 sustain, 5 attack, 6 start */
	int channel;	/* -1: none */
	int lastVolume, tieBreak;
	const Patch *patch;
	long pre;
	int noteVolume;
	long baseVolume;
	long stop;
	const unsigned char *env;
	int envIdx, envLen;
	long ticksLeft, incr;
	long attackStart, attackLen, sustainLen;
	long pos;
	unsigned long frac;
	long period;
};

unsigned short s_pow2[1024];	/* 2^(i/1024) x 32768 */

unsigned U16(const unsigned char *p) { return ((unsigned)p[0] << 8) | p[1]; }
unsigned long U32(const unsigned char *p) { return ((unsigned long)U16(p) << 16) | U16(p + 2); }

long CalcTempo(long t) { return ((t & 0xFFF0) << 8) / (5 * VBLANK); }

long VolDelta(long delta, long time)
{
	long div = time * VBLANK;
	return div <= 1000 ? delta : (1000 * delta) / div;
}

/* period = 2^(tone / 65536) */
long Period(long tone)
{
	long e = tone >> 16;
	unsigned long m = s_pow2[(tone & 0xFFFF) >> 6];	/* x 32768 */
	return (long)(((m << e) + 16384) >> 15);
}

}	/* namespace */

signed char *MaxTrax_Render(const unsigned char *mx, long len, int score, long rate,
                            long maxSamples, int gain256, long *length, long *loop)
{
	Patch patches[64];
	Channel chans[16];
	Voice voices[4];
	const unsigned char *p = mx, *end = mx + len, *events = NULL;
	unsigned long numEvents = 0;
	int tempo0, flags, numScores, numSamples, i;
	long *mix;
	signed char *out;
	long mul, pos = 0, marks[2];
	int nmarks = 0;

	if (len < 10 || memcmp(mx, "MXTX", 4) != 0) return NULL;
	if (s_pow2[0] == 0) {
		for (i = 0; i < 1024; i++) s_pow2[i] = (unsigned short)(pow(2.0, i / 1024.0) * 32768.0 + 0.5);
	}
	tempo0 = U16(p + 4);
	flags = U16(p + 6);
	p += 8;
	if (flags & 0x8000) p += 256;
	numScores = U16(p);
	p += 2;
	for (i = 0; i < numScores; i++) {
		unsigned long n;
		if (p + 4 > end) return NULL;
		n = U32(p);
		p += 4;
		if (i == score) { events = p; numEvents = n; }
		p += 6 * n;
	}
	if (events == NULL || numEvents == 0 || p + 2 > end) return NULL;
	memset(patches, 0, sizeof(patches));
	numSamples = U16(p);
	p += 2;
	for (i = 0; i < numSamples; i++) {
		Patch *pt;
		int ac, rc;
		long size;
		if (p + 20 > end) return NULL;
		pt = &patches[U16(p) & 63];
		pt->used = true;
		pt->tune = (short)U16(p + 2);
		pt->volume = U16(p + 4);
		pt->octaves = U16(p + 6);
		pt->attack = (long)U32(p + 8);
		pt->total = pt->attack + (long)U32(p + 12);
		ac = U16(p + 16);
		rc = U16(p + 18);
		p += 20;
		pt->env = p;
		pt->attackEnv = ac;
		pt->releaseEnv = rc;
		p += 4 * (ac + rc);
		size = pt->total * ((1L << pt->octaves) - 1);
		pt->data = (const signed char *)p;
		p += size;
		if (p > end) return NULL;
	}

	mix = (long *)calloc(maxSamples + rate, sizeof(long));
	if (mix == NULL) return NULL;
	for (i = 0; i < 16; i++) {
		chans[i].patch = i;
		chans[i].volume = 128;
		chans[i].bendRange = 24;
		chans[i].pitchReal = 0;
		chans[i].right = i & 1;
		chans[i].rpn = 0xFFFF;
	}
	memset(voices, 0, sizeof(voices));
	for (i = 0; i < 4; i++) voices[i].channel = -1;

	{
		const bool handleVolume = (flags & 2) != 0;
		const long frameUnit = (1000L << 8) / VBLANK;
		long tickUnit = CalcTempo((long)tempo0 << 4);
		long ticks = 0, nextTime = (long)U16(events + 2);
		unsigned long idx = 0;
		bool ended = false;
		long frame = 0;
		struct { unsigned long at; int left; } reps[4];
		int nreps = 0;

		for (;;) {
			long f0, f1, n;
			const unsigned char *e;
			int v;

			if (pos >= maxSamples) break;
			/* the events of this interrupt */
			ticks += tickUnit;
			{
				long millis = ticks >> 8;
				if (!ended && idx < numEvents) {
					long delta = nextTime - millis;
					while (delta <= 0) {
						int cmd, par, ch;
						unsigned start, stop;
						Channel *c;
						e = events + 6 * idx;
						cmd = e[0];
						par = e[1];
						start = U16(e + 2);
						stop = U16(e + 4);
						(void)start;
						ch = par & 15;
						c = &chans[ch];
						if (cmd < 0x80) {
							int vol = (par & 0xF0) >> 1;
							const Patch *pt = &patches[c->patch & 63];
							if (vol != 0 && pt->used) {
								int pick = c->right ? 1 : 0;
								Voice *a = &voices[pick], *b = &voices[pick ^ 3], *w;
								long plain, octv, total;
								if (((a->status << 8 | a->lastVolume) + a->tieBreak) > (b->status << 8 | b->lastVolume)) {
									a->tieBreak = 0;
									b->tieBreak = 1;
									pick ^= 3;
								}
								w = &voices[pick];
								{
									int tb = w->tieBreak;	/* the old note cut, its tie break kept */
									memset(w, 0, sizeof(*w));
									w->tieBreak = tb;
								}
								plain = K_VALUE + 0x3C000L - (((long)cmd << 14) + (((long)pt->tune << 11) / 3)) / 3;
								octv = plain <= PREF_PERIOD ? 0 : (plain + 0xFFFF - PREF_PERIOD) >> 16;
								if (octv > pt->octaves - 1) octv = pt->octaves - 1;
								w->pre = plain - (octv << 16);
								w->patch = pt;
								w->channel = ch;
								w->noteVolume = handleVolume ? vol + 1 : 128;
								w->baseVolume = 0;
								w->status = 6;
								total = pt->total;
								w->attackStart = (total << octv) - total;
								w->attackLen = pt->attack << octv;
								w->sustainLen = (total - pt->attack) << octv;
								w->stop = (delta + (long)stop) << 8;
								if (w->stop < 0) w->stop = 0;
							}
						} else if (cmd == 0x80) {
							tickUnit = CalcTempo((long)par << 4);
						} else if (cmd == 0xB0) {
							int ctl = stop >> 8, val = stop & 0xFF;
							if (ctl == 7) c->volume = val == 0 ? 0 : val + 1;
							else if (ctl == 10) c->right = val > 0x40 || (val == 0x40 && (ch & 1) != 0);
							else if (ctl == 0x64) c->rpn = (c->rpn & 0xFF00) | val;
							else if (ctl == 0x65) c->rpn = (c->rpn & 0xFF) | (val << 8);
							else if (ctl == 6 && c->rpn == 0) c->bendRange = val < 24 ? val : 24;
							else if (ctl == 0x7B || ctl == 0x7E || ctl == 0x7F) {
								for (v = 0; v < 4; v++) if (voices[v].channel == ch && voices[v].status > 3) voices[v].stop = 0;
							}
						} else if (cmd == 0xA0) {
							int sub = stop >> 8, arg = stop & 0xFF;
							if (sub == 2 && nreps < 4) {
								reps[nreps].at = idx;
								reps[nreps].left = arg;
								nreps++;
							} else if (sub == 3 && nreps > 0) {
								if (reps[nreps - 1].left > 0) {
									reps[nreps - 1].left--;
									idx = reps[nreps - 1].at;
									if (nmarks < 2) marks[nmarks] = pos;
									nmarks++;
								} else {
									nreps--;
								}
							}
						} else if (cmd == 0xC0) {
							c->patch = stop & 63;
						} else if (cmd == 0xE0) {
							long bend = ((stop & 0x7F00) >> 1) | (stop & 0x7F);
							c->pitchReal = ((c->bendRange * bend) >> 5) - ((long)c->bendRange << 8);
						} else if (cmd == 0xFF) {
							ended = true;
							break;
						}
						idx++;
						if (idx >= numEvents) {
							ended = true;
							break;
						}
						delta += (long)U16(events + 6 * idx + 2);
					}
					nextTime = delta + millis;
				}
			}
			if (nmarks >= 2) {
				pos = marks[1];
				break;
			}
			/* the voices */
			for (v = 0; v < 4; v++) {
				Voice *w = &voices[v];
				long vol, tone;
				int cv;
				if (w->channel < 0) continue;
				if (w->status > 3) {
					w->stop -= tickUnit;
					if (w->stop <= 0) w->status = 3;
				}
				if (w->status == 6) {
					if (w->patch->attackEnv) {
						w->env = w->patch->env;
						w->envLen = w->patch->attackEnv;
						w->envIdx = 0;
						w->ticksLeft = (long)U16(w->env) << 8;
						w->incr = VolDelta((long)U16(w->env + 2) - w->baseVolume, U16(w->env));
						w->status = 5;
					} else {
						w->status = 4;
						w->baseVolume = w->patch->volume;
					}
				} else if (w->status == 3) {
					if (w->patch->releaseEnv) {
						w->env = w->patch->env + 4 * w->patch->attackEnv;
						w->envLen = w->patch->releaseEnv;
						w->envIdx = 0;
						w->ticksLeft = (long)U16(w->env) << 8;
						w->incr = VolDelta((long)U16(w->env + 2) - w->baseVolume, U16(w->env));
						w->status = 2;
					} else {
						w->status = 1;
					}
				}
				if ((w->status == 5 || w->status == 2) && w->env != NULL) {
					if (w->ticksLeft > frameUnit) {
						w->baseVolume += w->incr;
						if (w->baseVolume < 0) w->baseVolume = 0;
						if (w->baseVolume > 0x8000) w->baseVolume = 0x8000;
						w->ticksLeft -= frameUnit;
					} else {
						const unsigned char *s = w->env + 4 * w->envIdx;
						w->baseVolume = U16(s + 2);
						w->envIdx++;
						if (w->envIdx < w->envLen) {
							s += 4;
							w->ticksLeft = (long)U16(s) << 8;
							w->incr = VolDelta((long)U16(s + 2) - w->baseVolume, U16(s));
						} else if (w->status == 2) {
							w->status = 1;
						} else {
							w->status = 4;
							w->env = NULL;
						}
					}
				}
				if (w->status == 1) {
					w->status = 0;
					w->channel = -1;
					w->lastVolume = 0;
					continue;
				}
				vol = w->noteVolume < 128 ? (w->noteVolume * 0x40) >> 7 : 0x40;
				if (w->baseVolume < 0x8000) vol = (vol * w->baseVolume) >> 15;
				cv = chans[w->channel].volume;
				if (cv < 128) vol = (vol * cv) >> 7;
				w->lastVolume = vol < 64 ? (int)vol : 64;
				tone = w->pre + (chans[w->channel].pitchReal << 6) / 3;
				w->period = tone >= PERIOD_LIMIT ? Period(tone) : 0;
			}
			/* the sound of this frame */
			f0 = (frame * rate) / VBLANK;
			f1 = ((frame + 1) * rate) / VBLANK;
			n = f1 - f0;
			pos = f0;
			for (v = 0; v < 4; v++) {
				Voice *w = &voices[v];
				unsigned long step;
				long k, al = w->attackLen, sl = w->sustainLen;
				const signed char *d;
				int vol = w->lastVolume;
				if (w->channel < 0 || w->period == 0 || vol == 0) continue;
				step = ((((unsigned long)PAULA << 4) / (unsigned long)w->period) << 12) / (unsigned long)rate;
				d = w->patch->data + w->attackStart;
				if (sl == 0 && w->pos >= al) {
					w->stop = 0;	/* the sample is over: released (no sustain part) */
					continue;
				}
				for (k = 0; k < n; k++) {
					if (w->pos < al + sl) mix[f0 + k] += (long)d[w->pos] * vol;
					w->frac += step;
					w->pos += (long)(w->frac >> 16);
					w->frac &= 0xFFFF;
					if (sl > 0) while (w->pos >= al + sl) w->pos -= sl;
				}
			}
			pos = f1;
			frame++;
			if (ended) {
				bool any = false;
				for (v = 0; v < 4; v++) if (voices[v].channel >= 0) any = true;
				if (!any) break;
			}
		}
	}

	if (pos > maxSamples) pos = maxSamples;
	out = (signed char *)malloc(pos > 0 ? pos : 1);
	if (out == NULL) {
		free(mix);
		return NULL;
	}
	/* mx_render.py: x / 64 x 127 / (128 x 1.1) x gain */
	mul = (127L * gain256 * 16384L) / 576717L;
	for (i = 0; i < pos; i++) {
		long s = (mix[i] * mul + 32768L) >> 16;
		out[i] = (signed char)(s > 127 ? 127 : s < -128 ? -128 : s);
	}
	free(mix);
	*length = pos;
	*loop = nmarks >= 1 ? marks[0] : -1;
	return out;
}
