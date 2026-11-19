#!/usr/bin/env bash
# Usage: Scripts/test.sh [filter]   (default filter: DoItAsphalt)
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

FILTER="${1:-DoItAsphalt}"
REPORT_DIR="$PROJECT_ROOT/Saved/TestReport"

# Remove any stale report so a crashed run can never look like a pass.
rm -rf "$REPORT_DIR"

"$UE_EDITOR_CMD" "$UPROJECT" \
	-ExecCmds="Automation RunTests $FILTER; Quit" \
	-unattended -nullrhi -nosplash -nosound -stdout \
	-ReportExportPath="$REPORT_DIR" || true

REPORT="$REPORT_DIR/index.json"
if [[ ! -f "$REPORT" ]]; then
	echo "No test report found at $REPORT" >&2
	exit 1
fi

python3 - "$REPORT" <<'PY'
import json, sys

with open(sys.argv[1], encoding="utf-8-sig") as f:
    data = json.load(f)

passed = data.get("succeeded", 0) + data.get("succeededWithWarnings", 0)
failed = data.get("failed", 0)
print(f"PASSED {passed} / FAILED {failed}")
sys.exit(1 if failed > 0 or passed + failed == 0 else 0)
PY
