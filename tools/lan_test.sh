#!/bin/sh
# A LAN game test: Amiga 1 (amiagent.cfg) is the server (Harkonnen), WinUAE
# (amiagent-winuae.cfg, ODUNE: = build/amiga) joins (Atreides); START GAME.
#   [PEER=1] tools/lan_test.sh [server-menu-y] [winuae-menu-y]
# menu-y: the "Play on LAN" line of the main menu (154 with a saved game,
# 166 without). Stops at once when the Amiga runs OpenDUNE (the user plays).
cd "$(dirname "$0")/.." || exit 1
tools/amiga_free.sh || exit 1
A="python tools/amiga_remote.py"
W="python tools/amiga_remote.py --config C:/amiga-dev/amiagent-winuae.cfg"
SY=${1:-154}
WY=${2:-166}
HOST=$(grep -i '^host' C:/amiga-dev/amiagent.cfg | sed 's/.*= *//')

$A put build/amiga/OpenDUNE.s WorkSD:Games/OpenDUNEWarp/OpenDUNE >/dev/null || exit 1
$A run "Delete RAM:OpenDUNE-lan.txt QUIET" >/dev/null
$W stop OpenDUNE >/dev/null; sleep 2
$W run "Delete RAM:OpenDUNE-lan.txt QUIET" >/dev/null
$A start "WorkSD:Games/OpenDUNEWarp/OpenDUNE" && $W start "ODUNE:OpenDUNE" || exit 1
sleep 14
# server: Play on LAN, START SERVER, RUN SERVER, Harkonnen
python tools/od.py "click 160 $SY" "wait 40" "click 70 178" "wait 20" "click 70 178" "wait 60" "click 197 124" "wait 10" || exit 1
# WinUAE: Play on LAN, JOIN SERVER, the address, CONNECT, then Atreides
python tools/od.py --config C:/amiga-dev/amiagent-winuae.cfg "click 160 $WY" "wait 40" "click 160 178" "wait 20" "type $HOST" "wait 30" "click 160 178" "wait 300" "click 131 124" "wait 60" || exit 1
# PEER=1: a third player from the PC (tools/lan_peer.py, gives no orders)
if [ "$PEER" = 1 ]; then
	python tools/lan_peer.py "$HOST" --name PEER --seconds ${PEER_SECONDS:-300} > "${PEER_LOG:-/dev/null}" 2>&1 &
	sleep 6
fi
# server: START GAME
python tools/od.py "wait 30" "click 160 178" "wait 10" || exit 1
echo "LAN game started"
