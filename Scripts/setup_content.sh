#!/usr/bin/env bash
# Copies the Mannequin content from the engine's third-person template into the project
# (skipped when it is already there). Usage: Scripts/setup_content.sh
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

SRC="$UE_ROOT/Templates/TemplateResources/High/Characters/Content/Mannequins"
DST="$PROJECT_ROOT/Content/Characters/Mannequins"

if [[ -e "$DST" ]]; then
	echo "Mannequins already present: $DST"
else
	if [[ ! -d "$SRC" ]]; then
		echo "Template content not found: $SRC" >&2
		exit 1
	fi
	mkdir -p "$(dirname "$DST")"
	cp -R "$SRC" "$DST"
	echo "Copied Mannequins to $DST"
fi

for f in Meshes/SKM_Manny_Simple.uasset Anims/Unarmed/ABP_Unarmed.uasset; do
	if [[ ! -f "$DST/$f" ]]; then
		echo "Missing expected asset: $DST/$f" >&2
		exit 1
	fi
	echo "OK $DST/$f"
done
