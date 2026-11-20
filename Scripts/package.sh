#!/usr/bin/env bash
# Usage: Scripts/package.sh
# Builds, cooks, stages and archives a Shipping build for Mac into Builds/ (RunUAT BuildCookRun).
# Prints BUILD SUCCESSFUL and the path of the .app only when the packaging really succeeded.
# The cheats (dia.*) are not compiled into Shipping. Takes a long time on the first run (full cook, shaders).
#
# Known pitfall: the project must NOT live in iCloud Drive (~/Desktop, ~/Documents with iCloud sync on). The file provider
# adds Finder/file-provider extended attributes to every new directory, including the .app bundle that Xcode creates in
# Binaries/Mac, and codesign then fails with "resource fork, Finder information, or similar detritus not allowed".
# Clone or move the project to a folder outside iCloud Drive (e.g. ~/Developer) before packaging.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"

RUN_UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
BUILDS_DIR="$PROJECT_ROOT/Builds"

if [[ ! -d "$UE_ROOT" ]]; then
	echo "PACKAGING FAILED: Unreal Engine not found at UE_ROOT=$UE_ROOT (set UE_ROOT to the UE 5.8 installation)." >&2
	exit 1
fi
if [[ ! -x "$RUN_UAT" ]]; then
	echo "PACKAGING FAILED: RunUAT.sh not found or not executable: $RUN_UAT" >&2
	exit 1
fi
if [[ ! -f "$UPROJECT" ]]; then
	echo "PACKAGING FAILED: project file not found: $UPROJECT" >&2
	exit 1
fi

# Warn (do not stop) when the project sits in iCloud Drive: see the note at the top.
DIR="$PROJECT_ROOT"
while [[ "$DIR" != "/" ]]; do
	if xattr -p com.apple.file-provider-domain-id "$DIR" >/dev/null 2>&1; then
		echo "WARNING: $PROJECT_ROOT is inside iCloud Drive; codesign of the .app will probably fail (see the note in Scripts/package.sh)." >&2
		break
	fi
	DIR="$(dirname "$DIR")"
done

mkdir -p "$BUILDS_DIR"

# Marker created before packaging: only an .app newer than this run's start counts, so a stale .app from an earlier run
# can never make this one print BUILD SUCCESSFUL.
START_MARK="$(mktemp "${TMPDIR:-/tmp}/dia-package-start.XXXXXX")"
trap 'rm -f "$START_MARK"' EXIT

"$RUN_UAT" BuildCookRun \
	-project="$UPROJECT" \
	-platform=Mac \
	-clientconfig=Shipping \
	-build -cook -stage -pak -archive \
	-archivedirectory="$BUILDS_DIR"

# RunUAT's exit code is trusted (set -e), but the result must also exist: look for an .app archived by this run.
APP=""
while IFS= read -r -d '' CANDIDATE; do
	APP="$CANDIDATE"
	break
done < <(find "$BUILDS_DIR" -maxdepth 3 -name '*.app' -type d -newer "$START_MARK" -print0 2>/dev/null)

if [[ -z "$APP" ]]; then
	echo "PACKAGING FAILED: RunUAT finished but no .app produced by this run was found under $BUILDS_DIR" >&2
	exit 1
fi

echo "BUILD SUCCESSFUL"
echo "App: $APP"
