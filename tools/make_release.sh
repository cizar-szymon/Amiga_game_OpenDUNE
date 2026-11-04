#!/bin/sh
# The release of the Amiga port (after tools/build.sh), number from port/VERSION:
#   releases/amiga/OpenDUNE/...           the public release, in git: the program,
#   releases/amiga/OpenDUNE.info          icons, opendune.ini, doc/ (ALL texts and PDFs:
#                                         descriptions PL/EN, history, Install, COPYING),
#   releases/amiga/OpenDUNE-<v>.zip       empty data/ and MUSIC/ with a ReadMe -
#                                         no game data
#   private/OpenDUNE-<v>-full.zip         the same + the Dune II data (data/*.PAK) and
#                                         the music (build/music/*.RAW): runs at once;
#                                         only on this PC (private/ is not in git)
# The archives (tools/make_zip.py, the user's choice: ZIP) unpack on the Amiga
# with UnZip:  UnZip OpenDUNE-<v>.zip -d Work:Games
# The Polish translation (tools/lang/polish -> lang/POLISH.PAK, a translation
# of Westwood's texts) goes only into the full archive: the user's decision,
# tools/lang/polish is not in git either.
#   tools/make_release.sh [--no-full]
set -e
cd "$(dirname "$0")/.."
V=$(cat port/VERSION)
D=docs/release/amiga
R=releases/amiga
FULL=1
for a in "$@"; do
	case "$a" in
		--no-full) FULL=0 ;;
	esac
done

grep -a -q "\$VER: OpenDUNE $V " build/amiga/OpenDUNE.s || { echo "build/amiga/OpenDUNE.s is not $V: tools/build.sh first" >&2; exit 1; }

# the public tree
rm -rf "$R/OpenDUNE"
mkdir -p "$R/OpenDUNE/data" "$R/OpenDUNE/MUSIC"
cp build/amiga/OpenDUNE.s "$R/OpenDUNE/OpenDUNE"
# Tool Types: the player's settings, winning over opendune.ini (os/options_amiga.c);
# in brackets they are off, as hints
python tools/make_icon.py "$R/OpenDUNE/OpenDUNE.info" --frame tools/icon/title112.png --crop 0 0 112 70 --lit-frame --pos 8 4 \
  --tooltypes "(DATADIR=Work:Dune2/)" "(MUSIC_DIR=PROGDIR:MUSIC)" \
  "(MUSIC_VOLUME=40)" "(LAN_INFO=1)" "(SKIP_INTRO=0)" >/dev/null 2>&1
cp tools/icon/warp-drawer.info "$R/OpenDUNE.info"	# the game drawer: the WARP logo (tools/icon/warp-drawer.info, ../WARP-drawer.info)
[ -f build/amiga/MakeMusic ] || { echo "build/amiga/MakeMusic missing: tools/music/amiga/build.sh first" >&2; exit 1; }
cp build/amiga/MakeMusic "$R/OpenDUNE/MakeMusic"	# records the music on the Amiga
python tools/make_icon.py "$R/OpenDUNE/MakeMusic.info" --frame tools/icon/makemusic.png --crop 0 0 54 44 --transparent --pos 140 16 >/dev/null 2>&1	# loudspeaker and notes
python tools/make_install.py >/dev/null	# Install.txt and Install.pdf (EN, PL, DE) from Install_src.txt
cp "$D/opendune.ini" "$R/OpenDUNE/"
# ALL texts and PDFs in one drawer doc (the user's rule for every port, 3.11.2026)
mkdir -p "$R/OpenDUNE/doc"
cp "$D/Install.txt" "$D/Install.pdf" "$D/OpenDUNE.txt" "$D/OpenDUNE_en.txt" "$D/OpenDUNE_release.txt" "$D/OpenDUNE_release_en.txt" "$R/OpenDUNE/doc/"
cp COPYING "$R/OpenDUNE/doc/COPYING"			# GPL-2.0, goes with the program
python tools/make_doc_icon.py "$R/OpenDUNE/doc.info" --frame tools/icon/drawer.png --crop 0 0 46 40 --transparent --drawer --window 430 230 --pos 230 12 >/dev/null 2>&1
# the texts with icons (tools/icon/draw_icons.py), opened by MultiView
# (file:picture:x:y - Install.txt in the first row, the descriptions PL | EN below)
for t in Install.txt:doc_i:72:10 OpenDUNE.txt:doc_pl:72:80 OpenDUNE_en.txt:doc_en:282:80 OpenDUNE_release.txt:doc_pl:72:150 OpenDUNE_release_en.txt:doc_en:282:150; do
	IFS=: read -r f pic x y <<EOT
$t
EOT
	python tools/make_doc_icon.py "$R/OpenDUNE/doc/$f.info" --frame "tools/icon/$pic.png" --crop 0 0 36 44 --transparent --project SYS:Utilities/MultiView --pos "$x" "$y" >/dev/null 2>&1
done
cp "$D/data_ReadMe.txt" "$R/OpenDUNE/data/ReadMe.txt"
cp "$D/MUSIC_ReadMe.txt" "$R/OpenDUNE/MUSIC/ReadMe.txt"

# the archives: older public ones out of git into private/old (kept)
mkdir -p private/old
for f in "$R"/OpenDUNE-*.zip "$R"/OpenDUNE-*.lha; do
	[ -e "$f" ] && [ "$f" != "$R/OpenDUNE-$V.zip" ] && { git rm -q --cached "$f" 2>/dev/null || true; mv "$f" private/old/; }
done
python tools/make_zip.py "$R/OpenDUNE-$V.zip" "$R" OpenDUNE OpenDUNE.info
if [ "$FULL" = 1 ]; then
	mkdir -p private/old
	# only the current full archive in private/, the older ones in private/old (kept)
	for f in private/OpenDUNE-*-full.zip; do
		[ -e "$f" ] && [ "$f" != "private/OpenDUNE-$V-full.zip" ] && mv "$f" private/old/
	done
	LANG_ADD=
	if [ -d tools/lang/polish ]; then
		python tools/lang/make_polish.py build/lang_pak/POLISH.PAK >/dev/null
		LANG_ADD="--add build/lang_pak OpenDUNE/lang"
	fi
	python tools/make_zip.py "private/OpenDUNE-$V-full.zip" "$R" OpenDUNE OpenDUNE.info \
		--add data "OpenDUNE/data" --add build/music "OpenDUNE/MUSIC" $LANG_ADD
fi
ls -l "$R/OpenDUNE-$V.zip" private/OpenDUNE-$V-full.zip 2>/dev/null
