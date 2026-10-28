#!/bin/sh
# Instructions of the binary that a 68060 does not execute in hardware and
# 68060.library emulates in a trap: 64-bit MULS.L/MULU.L/DIVS.L/DIVU.L and the
# 68881/68882 transcendental FPU instructions (fsin, fetox, flogn, fmovecr...).
# Runs inside the Docker toolchain: tools/check060.sh [binary]
BIN=${1:-build/amiga/OpenDUNE}
cd "$(dirname "$0")/.."
MSYS_NO_PATHCONV=1 docker run --rm -v "$(pwd -W 2>/dev/null || pwd):/src" sacredbanana/amiga-compiler:m68k-amigaos sh -c "
export PATH=/opt/amiga/bin:\$PATH; cd /src
m68k-amigaos-objdump -m m68k:68060 -d $BIN > /tmp/dis.txt
R64='	(mul[su]l|div[su]l)[ 	]+[^ ]*,%?d[0-7],%?d[0-7]\$'
RFP='	f(sin|cos|tan|asin|acos|atan|atanh|sinh|cosh|tanh|etox|etoxm1|twotox|tentox|logn|lognp1|log10|log2|sincos|movecr|mod|rem|scale|getexp|getman)[a-z]*[ 	]'
echo \"64-bit mul/div: \$(grep -c -E \"\$R64\" /tmp/dis.txt)   emulated FPU: \$(grep -c -E \"\$RFP\" /tmp/dis.txt)\"
grep -E "$RFP" /tmp/dis.txt | head -4
for re in \"\$R64\" \"\$RFP\"; do
  awk -v re=\"\$re\" '/^[0-9a-f]+ <.*>:\$/ { fn=\$2 } \$0 ~ re { n[fn]++ } END { for (f in n) print \"  \" n[f], f }' /tmp/dis.txt | sort -rn | head -15
done"
