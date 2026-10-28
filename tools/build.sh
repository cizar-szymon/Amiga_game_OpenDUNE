#!/bin/sh
# Build in Docker (bebbo gcc 6.5) -> build/amiga/OpenDUNE (+ OpenDUNE.s stripped).
#   tools/build.sh [clean]
cd "$(dirname "$0")/.."
CLEAN=
[ "$1" = clean ] && CLEAN='rm -rf /build/* && '
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/src" -v opendune-build:/build \
  sacredbanana/amiga-compiler:m68k-amigaos sh -c "export PATH=/opt/amiga/bin:\$PATH; cd /src/port && $CLEAN \
  make -f Makefile.amiga -j 16 2>&1 | grep -E 'error|warning|undefined' ; \
  test -f /build/OpenDUNE && mkdir -p /src/build/amiga && cp /build/OpenDUNE /build/OpenDUNE.s /src/build/amiga/ && \
  ls -l /src/build/amiga/"
