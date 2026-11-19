#!/usr/bin/env bash
# Usage: Scripts/uepy.sh <script.py>
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <script.py>" >&2
	exit 2
fi

"$UE_EDITOR_CMD" "$UPROJECT" -run=pythonscript -Script="$1" -unattended -nosplash -stdout
