/* MaxTrax (.MX, the music of the Amiga Dune II) recorded to 8-bit samples
 * (maxtrax.cpp; the same as tools/music/mx_render.py). */
#ifndef MAXTRAX_H
#define MAXTRAX_H

/* Records score SCORE of the MaxTrax file MX (LEN bytes) at RATE Hz, 8-bit
 * signed mono, at most MAXSAMPLES samples. A score with a repeat (A0 02/03)
 * is recorded with its start and two passes of the repeat, *LOOP = the start
 * of the second pass; else it plays once, *LOOP = -1. GAIN256: the gain x256
 * on top of the one of mx_render.py (2.5 for the missions' music: 640).
 * NULL: not a MaxTrax file, no such score, no memory. */
signed char *MaxTrax_Render(const unsigned char *mx, long len, int score, long rate,
                            long maxSamples, int gain256, long *length, long *loop);

#endif
