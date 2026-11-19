#!/usr/bin/env bash
# Usage: Scripts/uepy.sh <script.py>   (relative paths resolve against the current directory)
#
# Runs an editor Python script headless. Exits non-zero when the editor fails, when the
# output contains a "DIA_FAIL" line or a Python traceback, or when no "DIA_OK" line was
# printed (an unhandled exception in -run=pythonscript can otherwise still exit 0).
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <script.py>" >&2
	exit 2
fi

SCRIPT="$1"
if [[ ! -f "$SCRIPT" ]]; then
	echo "uepy: script not found: $SCRIPT" >&2
	exit 2
fi
# Absolute path: the editor resolves relative paths against its own cwd, not ours.
SCRIPT="$(cd "$(dirname "$SCRIPT")" && pwd)/$(basename "$SCRIPT")"

LOG="$(mktemp -t uepy.XXXXXX)"
trap 'rm -f "$LOG"' EXIT

# -FullStdOutLogOutput: without it -stdout drops Log-verbosity output, i.e. every Python print().
set +e
"$UE_EDITOR_CMD" "$UPROJECT" -run=pythonscript -Script="$SCRIPT" -unattended -nosplash -stdout -FullStdOutLogOutput 2>&1 | tee "$LOG"
RC=${PIPESTATUS[0]}
set -e

if [[ $RC -ne 0 ]]; then
	echo "uepy: editor exited with code $RC" >&2
	exit "$RC"
fi
if grep -qE '(^|[^A-Za-z_])DIA_FAIL( |$)' "$LOG"; then
	echo "uepy: script reported DIA_FAIL" >&2
	exit 1
fi
if grep -q 'Traceback (most recent call last)' "$LOG"; then
	echo "uepy: Python traceback in output" >&2
	exit 1
fi
if ! grep -qE '(^|[^A-Za-z_])DIA_OK ' "$LOG"; then
	echo "uepy: no DIA_OK line in output" >&2
	exit 1
fi
