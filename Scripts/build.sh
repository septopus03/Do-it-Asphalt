#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DoItAsphaltEditor Mac Development -project="$UPROJECT"
