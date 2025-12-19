#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
BUILD_DIR="$PROJECT_DIR/build"
REPO_ROOT=$(realpath "$SCRIPT_DIR/../../../../..")
VENDOR_LIB_DIR="$REPO_ROOT/external/sampic_256ch_lib/lib"
LPDEVC_LIB_DIR="$REPO_ROOT/external/sampic_256ch_lib/lpdevc_install/lib"
USE_GDB=0
PASS_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --gdb)
      USE_GDB=1
      shift
      ;;
    *)
      PASS_ARGS+=("$1")
      shift
      ;;
  esac
done

BINARY="$BUILD_DIR/bin/sampic_deadtime_scan"

if [[ ! -x "$BINARY" ]]; then
  echo "Calibration mode binary not found at $BINARY. Run scripts/build.sh first." >&2
  exit 1
fi

declare -a RUNTIME_LIB_PATHS=()
if [[ -d "$VENDOR_LIB_DIR" ]]; then
  RUNTIME_LIB_PATHS+=("$VENDOR_LIB_DIR")
fi
if [[ -d "$LPDEVC_LIB_DIR" ]]; then
  RUNTIME_LIB_PATHS+=("$LPDEVC_LIB_DIR")
fi
if [[ ${#RUNTIME_LIB_PATHS[@]} -gt 0 ]]; then
  RUNTIME_LIB_PATH=$(IFS=:; echo "${RUNTIME_LIB_PATHS[*]}")
  if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
    export LD_LIBRARY_PATH="$RUNTIME_LIB_PATH:$LD_LIBRARY_PATH"
  else
    export LD_LIBRARY_PATH="$RUNTIME_LIB_PATH"
  fi
fi

cd "$PROJECT_DIR"

if [[ $USE_GDB -eq 1 ]]; then
  exec gdb --args "$BINARY" --mode calibration "${PASS_ARGS[@]}"
else
  exec "$BINARY" --mode calibration "${PASS_ARGS[@]}"
fi
