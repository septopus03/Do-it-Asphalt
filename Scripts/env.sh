#!/usr/bin/env bash
# Shared environment for all project scripts. Source this file; do not execute it.
SCRIPTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
export PROJECT_ROOT="$(cd "$SCRIPTS_DIR/.." && pwd)"
export UPROJECT="$PROJECT_ROOT/DoItAsphalt.uproject"
export UE_EDITOR_CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
