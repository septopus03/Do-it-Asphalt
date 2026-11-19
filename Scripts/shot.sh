#!/usr/bin/env bash
# Usage: Scripts/shot.sh <name> ["cheat; cheat"]
# Starts the game windowed on L_Main, runs the cheats, then saves Saved/Screenshots/<name>.png after SHOT_DELAY
# seconds (default 4; the game also waits until shaders have finished compiling) and quits.
# Exits with 1 if the PNG was not produced. Example: Scripts/shot.sh ground_150 "dia.TopView; dia.Complete 150"
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

if [[ $# -lt 1 || $# -gt 2 ]]; then
	echo "Usage: $0 <name> [\"cheat; cheat\"]" >&2
	exit 2
fi

NAME="$1"
CHEATS="${2:-}"
SHOT_DELAY="${SHOT_DELAY:-4}"
SHOT="$PROJECT_ROOT/Saved/Screenshots/$NAME.png"
# UnrealEditor-Cmd instead of UnrealEditor: the GUI launcher detaches from its child and would return at once, while
# the Cmd binary opens the same game window (-game) but runs in the foreground until the game quits.
EDITOR_BIN="$UE_EDITOR_CMD"

# A stale picture must never satisfy the check below.
rm -f "$SHOT"

# -ExecCmds separates commands with commas; accept the friendlier ';' as well.
EXEC="${CHEATS//;/,}"
EXEC="${EXEC%"${EXEC##*[![:space:]]}"}"
if [[ -n "${EXEC// /}" ]]; then
	EXEC="$EXEC, "
else
	EXEC=""
fi
EXEC="${EXEC}dia.ShotAndQuit $SHOT_DELAY $NAME"

"$EDITOR_BIN" "$UPROJECT" L_Main -game -windowed -ResX=1280 -ResY=720 -nosplash -ExecCmds="$EXEC" || true

if [[ ! -s "$SHOT" ]]; then
	echo "Screenshot was not produced: $SHOT" >&2
	exit 1
fi
echo "Screenshot saved: $SHOT"
