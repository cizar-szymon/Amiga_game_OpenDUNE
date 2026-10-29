#!/bin/sh
# After tools/lan_test.sh (map Arrakis 1): the joiner in WinUAE (Atreides,
# its Construction Yard at 480,135 on the screen) builds a concrete slab and
# places it right of the yard; its two tanks are sent south.
cd "$(dirname "$0")/.." || exit 1
OD="python tools/od.py --config C:/amiga-dev/amiagent-winuae.cfg"
$OD "ldown 300 60" "wait 3" "lmove 556 170" "wait 3" "lup 556 170" "wait 20" "rclick 420 300" "wait 60" || exit 1
$OD "click 480 135" "wait 20" "click 607 192" "wait 600" || exit 1
$OD "click 607 192" "wait 30" "move 504 128" "wait 10" "click 504 128" "wait 60" || exit 1
echo "built and placed"
