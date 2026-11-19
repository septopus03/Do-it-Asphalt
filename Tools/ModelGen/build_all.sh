#!/usr/bin/env bash
# Regenerate every model (GLB + previews), then verify all of them with check.py.
# Usage: Tools/ModelGen/build_all.sh        (from any directory)
# Exits non-zero as soon as a model script fails, or if any check fails.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BLENDER="${BLENDER:-blender}"

command -v "$BLENDER" >/dev/null || { echo "build_all: '$BLENDER' not found (brew install --cask blender)" >&2; exit 1; }

for script in "$HERE"/*.py; do
    case "$(basename "$script")" in
        common.py|check.py|_*) continue ;;  # helpers and shared _modules are not models
    esac
    echo "== $(basename "$script")"
    log="$(mktemp)"
    if ! "$BLENDER" -b --python-exit-code 1 -P "$script" >"$log" 2>&1; then
        cat "$log"
        rm -f "$log"
        echo "build_all: $(basename "$script") failed" >&2
        exit 1
    fi
    grep -E '^(EXPORTED|PREVIEWS|WARNING)' "$log" || true
    rm -f "$log"
done

python3 "$HERE/check.py"
