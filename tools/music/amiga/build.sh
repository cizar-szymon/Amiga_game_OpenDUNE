#!/bin/sh
# MakeMusic for the Amiga (bebbo m68k-amigaos-gcc/g++ 6.5 in Docker, as tools/build.sh)
# -> build/amiga/MakeMusic.  -O1 as the game (gcc 6.5 -O2 miscompiles some loops),
# 68020 code tuned for the 060 (no 64-bit MULS.L/DIVS.L traps), no FPU needed
# (fmopl.c computes its tables with doubles once, by software).
cd "$(dirname "$0")/../../.."
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/src" \
  sacredbanana/amiga-compiler:m68k-amigaos sh -c "export PATH=/opt/amiga/bin:\$PATH; cd /src/tools/music/amiga && \
  F='-noixemul -mcpu=68020 -mtune=68060 -msoft-float -O1 -fomit-frame-pointer -w' && \
  m68k-amigaos-gcc \$F -c fmopl.c -o /tmp/fmopl.o && \
  m68k-amigaos-g++ \$F -fno-exceptions -fno-rtti -o /src/build/amiga/MakeMusic makemusic.cpp adl.cpp emuopl.cpp /tmp/fmopl.o -lm && \
  m68k-amigaos-strip /src/build/amiga/MakeMusic && ls -l /src/build/amiga/MakeMusic"
