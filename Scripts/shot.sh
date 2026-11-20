#!/usr/bin/env bash
# Usage: Scripts/shot.sh [--menu] <name> ["cheat; cheat"]
# Starts the game windowed on L_Main, runs the cheats, then saves Saved/Screenshots/<name>.png after SHOT_DELAY
# seconds (default 4; the game also waits until shaders have finished compiling) and quits.
# Exits with 1 if the PNG was not produced. Example: Scripts/shot.sh ground_150 "dia.TopView; dia.Complete 150"
# The game uses its own save slot, DIA_Shot, which (and its backup, DIA_Shot_bak) is deleted before every run (the real DoItAsphalt_Slot0 is never touched).
# The game starts in the main menu; dia.SkipMenu goes first among the cheats, unless --menu is given (a picture of the menu itself).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

SKIP_MENU="dia.SkipMenu"
if [[ "${1:-}" == "--menu" ]]; then
	SKIP_MENU=""
	shift
fi

if [[ $# -lt 1 || $# -gt 2 ]]; then
	echo "Usage: $0 [--menu] <name> [\"cheat; cheat\"]" >&2
	exit 2
fi

NAME="$1"
CHEATS="${2:-}"
SHOT_DELAY="${SHOT_DELAY:-4}"
SHOT="$PROJECT_ROOT/Saved/Screenshots/$NAME.png"
# UnrealEditor-Cmd instead of UnrealEditor: the GUI launcher detaches from its child and would return at once, while
# the Cmd binary opens the same game window (-game) but runs in the foreground until the game quits.
EDITOR_BIN="$UE_EDITOR_CMD"

# A stale picture must never satisfy the check below, and a save left by an earlier run must not change this one.
SLOT="DIA_Shot"
rm -f "$SHOT" "$PROJECT_ROOT/Saved/SaveGames/$SLOT.sav" "$PROJECT_ROOT/Saved/SaveGames/${SLOT}_bak.sav"

# -ExecCmds separates commands with commas; accept the friendlier ';' as well.
EXEC="${CHEATS//;/,}"
EXEC="${EXEC%"${EXEC##*[![:space:]]}"}"
if [[ -n "${EXEC// /}" ]]; then
	EXEC="$EXEC, "
else
	EXEC=""
fi
EXEC="${SKIP_MENU:+$SKIP_MENU, }${EXEC}dia.ShotAndQuit $SHOT_DELAY $NAME"

"$EDITOR_BIN" "$UPROJECT" L_Main -game -windowed -ResX=1280 -ResY=720 -nosplash -DIASlot="$SLOT" -ExecCmds="$EXEC" || true

if [[ ! -s "$SHOT" ]]; then
	echo "Screenshot was not produced: $SHOT" >&2
	exit 1
fi
echo "Screenshot saved: $SHOT"
