# OpenDUNE for the Amiga (68040/68060 + RTG)

**Dune II: The Battle for Arrakis** on the Amiga: [OpenDUNE](https://github.com/OpenDUNE/OpenDUNE),
the open source engine of the PC version 1.07, ported to AmigaOS 3 with a 68040/68060 accelerator
and an RTG graphics card (Picasso96). Tested on Warp 1260 / 7060 (68060 at 100 MHz), WinUAE and
Amiberry.

- large screen 400x300 to 960x540, scrolling by the card's blitter, smooth unit movement (up to 60 fps)
- group selection with a rubber band, right-button orders, map zoom x2/x3, pause
- **LAN game for up to 3 players** (fog of war per player, chat, pause, save/load)
- music from disk (recordings of the original AdLib music, made from your own data)
- F10 bug reports, Amiga info window (CPU, memory, card, Warp clock)

## Play it

The ready-to-unpack release is in [`releases/amiga`](releases/amiga):
`OpenDUNE-<version>.zip` (also unpacked next to it; on the Amiga: `UnZip OpenDUNE-<version>.zip -d Work:Games`). Read
[`OpenDUNE_en.txt`](releases/amiga/OpenDUNE/OpenDUNE_en.txt) (Polish:
[`OpenDUNE.txt`](releases/amiga/OpenDUNE/OpenDUNE.txt)) and the release history
[`OpenDUNE_release_en.txt`](releases/amiga/OpenDUNE/OpenDUNE_release_en.txt).

**Requirements:** 68040/68060, 32 MB Fast RAM, AmigaOS 3.1+ (tested 3.2), Picasso96 with 8-bit
modes, Paula sound; a TCP/IP stack for the LAN game.

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
