/*
 * render_adl.cpp - one track of a Dune II AdLib music file (.ADL) to raw
 * 16-bit mono samples, through AdPlug's ADL player and the Nuked OPL3
 * emulator (built and run in Docker by make_music.py).
 *
 *   render_adl FILE.ADL TRACK RATE MAXSECONDS OUT.raw
 *
 * TRACK is the game's index (g_table_musics, 2..8). A track that ends (all
 * channels stopped) gets one second more for the release. A track that
 * plays on and on (the original loops it) is recorded MAXSECONDS long, with
 * the notes struck in each tick (72 Hz: key on of a channel with its
 * frequency, the rhythm bits) in OUT.raw.ticks: make_music.py finds the loop
 * in them (the driver's random effects: the notes repeat, not exactly all).
 * Prints: seconds, the samples written, 1 if it ended (0: plays on).
 */
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <adplug/adplug.h>
#include <adplug/adl.h>
#include <adplug/nemuopl.h>
#include <adplug/fprovide.h>

#define HASH_START 1469598103934665603ULL

/* The OPL, the notes of each tick hashed on the way. */
class LogOpl : public Copl
{
public:
	LogOpl(int rate) : real(rate), cur(HASH_START) { for (int i = 0; i < 256; i++) regs[i] = 0; }
	void write(int reg, int val)
	{
		real.write(reg, val);
		reg &= 0xFF;
		if (reg >= 0xB0 && reg <= 0xB8 && (val & 0x20) != 0 && (regs[reg] & 0x20) == 0) {
			/* key on: the channel, its block and frequency */
			mix((reg << 16) | ((val & 0x1F) << 8) | regs[reg - 0x10]);
		}
		if (reg == 0xBD && (val & ~regs[reg] & 0x1F) != 0) mix(0xBD00 | (val & ~regs[reg] & 0x1F));
		regs[reg] = (uint8_t)val;
	}
	void init() { real.init(); }
	void update(short *buf, int samples) { real.update(buf, samples); }
	uint64_t take() { uint64_t h = cur; cur = HASH_START; return h; }
	CNemuopl real;
private:
	void mix(uint32_t v) { cur = (cur ^ v) * 1099511628211ULL; }
	uint64_t cur;
	uint8_t regs[256];
};

int main(int argc, char **argv)
{
	if (argc != 6) {
		fprintf(stderr, "usage: render_adl FILE.ADL TRACK RATE MAXSECONDS OUT.raw\n");
		return 1;
	}
	const int track = atoi(argv[2]), rate = atoi(argv[3]), maxSeconds = atoi(argv[4]);
	LogOpl opl(rate);
	CadlPlayer player(&opl);
	if (!player.load(argv[1], CProvider_Filesystem())) {
		fprintf(stderr, "%s: not loaded\n", argv[1]);
		return 1;
	}
	player.rewind(-1);	/* the OPL and the driver initialised */
	player.rewind(track);

	std::vector<short> stereo, all;
	std::vector<uint64_t> hash;	/* per tick */
	std::vector<long> pos;		/* the first sample of each tick */
	double perTick = rate / player.getrefresh(), due = 0;
	long tail = -1;
	bool ended = false;
	while ((long)all.size() < (long)maxSeconds * rate) {
		bool playing = player.update();
		hash.push_back(opl.take());
		pos.push_back((long)all.size());
		if (!playing && tail < 0) {
			tail = rate;	/* one second for the release */
			ended = true;
		}
		due += perTick;
		int n = (int)due;
		due -= n;
		stereo.resize(n * 2);
		opl.update(stereo.data(), n);
		for (int i = 0; i < n; i++) all.push_back((short)((stereo[2 * i] + stereo[2 * i + 1]) / 2));
		if (tail >= 0 && (tail -= n) <= 0) break;
	}

	FILE *out = fopen(argv[5], "wb");
	if (out == NULL) return 1;
	fwrite(all.data(), sizeof(short), all.size(), out);
	fclose(out);
	if (!ended) {
		/* plays on: the notes of each tick and where the tick starts, for
		 * make_music.py to find the loop (OUT.raw.ticks: hash, sample, 64-bit) */
		std::string name = std::string(argv[5]) + ".ticks";
		FILE *t = fopen(name.c_str(), "wb");
		if (t == NULL) return 1;
		for (size_t i = 0; i < hash.size(); i++) {
			uint64_t p = (uint64_t)pos[i];
			fwrite(&hash[i], sizeof(uint64_t), 1, t);
			fwrite(&p, sizeof(uint64_t), 1, t);
		}
		fclose(t);
	}
	printf("%.1f %ld %d\n", (double)all.size() / rate, (long)all.size(), ended ? 1 : 0);
	return 0;
}
