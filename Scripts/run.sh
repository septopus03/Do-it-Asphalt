#!/usr/bin/env bash
# Opens the project in the Unreal Editor.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor" "$UPROJECT" "$@"
