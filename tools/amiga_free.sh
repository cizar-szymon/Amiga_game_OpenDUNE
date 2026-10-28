#!/bin/sh
# Exit 1 when OpenDUNE runs on the Amiga (the user may be playing): gate
# every start, deploy and test command with  tools/amiga_free.sh [cfg] || exit 1
# An icon-started game is not in Status; its screen is in titles. The test
# channel RAM:od-cmd reaches ANY running copy with debug_commands=1.
cd "$(dirname "$0")/.."
CFG=${1:-C:/amiga-dev/amiagent.cfg}
if python tools/amiga_remote.py --config "$CFG" titles | grep -q "^OpenDUNE"; then
  echo "OpenDUNE is running on the Amiga - not touching it" >&2
  exit 1
fi
exit 0
