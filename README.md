# OpenDUNE for the Amiga with a Warp card

**Dune II: The Battle for Arrakis** on the Amiga: [OpenDUNE](https://github.com/OpenDUNE/OpenDUNE),
the open source engine of the PC version 1.07, ported to AmigaOS 3.

**This is a special port for the Amiga 500 / 1200 / 3000 / 4000 with a Warp accelerator card:
Warp 560, Warp 1240, Warp 1260, Warp 1260 Extreme or Warp 7060.** It was ported and tested on
these machines (an A1200 with a Warp 1260 Extreme and an A4000 with a Warp, both 68060 at 100 MHz),
and it is tuned for them: the picture is put together in the card's fast graphics memory and scrolled by
the card's blitter, so only what changed travels to the card. That fast graphics card is what
makes the large screens, the smooth scrolling and unit movement and the frame rates below
possible. Other 68040/68060 Amigas with a Picasso96 card (and WinUAE, Amiberry) run it as well,
but slower graphics cards will not reach the same speed.

- large screen 400x300 to 960x540 (default 640x480), scrolling by the card's blitter, smooth unit
  movement (up to 60 fps)
- group selection with a rubber band, right-button orders, map zoom x2/x3, pause (P)
- **LAN game for up to 3 players** (fog of war per player, chat, pause, save/load by the server,
  a lost player pauses the game for everyone)
- **internet game for 2 or 3 players** through a game server: a list of the games waiting, a code
  and an optional password, no router setup
- **bug reports from the game**: F10 opens a window that sends your description, the description
  of the Amiga and the report files to the game server
- **new pictures of the mentats** and of the house selection screen (`modern=1`, on in the package;
  made for the port), and an easy to read font (`font=clean`)
- English and German (Westwood's texts from your own Dune II), chosen in the game; a Polish
  translation (`lang/POLISH.PAK`) is not part of the public release
- music from disk (recordings of the original AdLib music, made from your own data)
- F10 bug reports, Amiga info window (CPU, memory, card, Warp clock)

## Play it

The ready-to-unpack release is in [`releases/amiga`](releases/amiga):
`OpenDUNE-<version>.zip` (also unpacked next to it; on the Amiga: `UnZip OpenDUNE-<version>.zip -d Work:Games`). Read
[`OpenDUNE_en.txt`](releases/amiga/OpenDUNE/doc/OpenDUNE_en.txt) (Polish:
[`OpenDUNE.txt`](releases/amiga/OpenDUNE/doc/OpenDUNE.txt)) and the release history
[`OpenDUNE_release_en.txt`](releases/amiga/OpenDUNE/doc/OpenDUNE_release_en.txt).

**Requirements:** an Amiga with a Warp card (68040 or 68060; other 68040/68060 cards work,
slower),
32 MB Fast RAM (the game needs 8 MB free), AmigaOS 3.1+ (tested 3.2), a Picasso96 card with an
8-bit mode of 640x480 or more, Paula sound; a TCP/IP stack for the LAN game. At the start the game
checks the Amiga and says what is missing (no RTG card, too little Fast RAM); on a 68020/68030 it
warns that it may run slowly.

**Settings:** the screen size, language and frame counter are chosen in the game (Options) and
kept. Other settings are in `opendune.ini` or as Tool Types of the OpenDUNE icon (e.g.
`DATADIR=Work:Dune2/`), which win over `opendune.ini`.

### The game data is not included

The graphics, sounds, speech and missions are files of the original **Dune II for the PC**,
copyrighted by Westwood/Virgin (today Electronic Arts). They are not in this repository and we do
not link to downloads: copy them from **your own original copy** (version 1.07 if possible) into
`OpenDUNE/data`:

`ATRE.PAK DUNE.PAK ENGLISH.PAK FINALE.PAK HARK.PAK MENTAT.PAK MERC.PAK ORDOS.PAK SCENARIO.PAK SOUND.PAK VOC.PAK`
(+ `INTRO.PAK` for the intro). The files of the Amiga version of Dune II do not fit.

### Music

Recordings of the game's AdLib music, made once from your own data (they are not distributed
either): on the Amiga double-click **MakeMusic** (in the release; about 35 minutes on a 68060 at
100 MHz, writes `OpenDUNE/MUSIC`), or on a PC `python tools/music/make_music.py --data <directory
with the .PAK files>` (Python 3 + numpy, ffmpeg, Docker) and copy `build/music/*.RAW` to
`OpenDUNE/MUSIC`. Without them the game runs without music.

## Build

- `tools/build.sh` – the program, in Docker (bebbo m68k-amigaos-gcc 6.5): `build/amiga/OpenDUNE`
- `tools/make_release.sh` – `releases/amiga` and the ZIP archive (`tools/make_zip.py`, Amiga protection bits kept)
- `tools/music/amiga/build.sh` – MakeMusic (AdPlug's ADL player + MAME's OPL2 emulator, LGPL-2.1)
- `tools/make_doc_pdf.py` – the documentation as a PDF
- the release number: `port/VERSION`

The port's sources are in `port/` (the Amiga parts in `*_amiga.c`, `#if defined(__amigaos__)`).
How it was done, the decisions and the measurements: [`docs/PORTING.md`](docs/PORTING.md) (Polish).

## Licence

GPL-2.0, as OpenDUNE (see the sources). Dune II is a trademark of its owners; this project only
contains the engine, not the game.
