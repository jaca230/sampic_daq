#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
REPO_ROOT=$(realpath "$PROJECT_DIR/../../..")
BINARY="$PROJECT_DIR/build/bin/double_pulse_timestamp_scan"
CONFIG="$PROJECT_DIR/config/double_pulse_waveform_scan.default.json"

if [[ ! -x "$BINARY" ]]; then
  echo "double_pulse_timestamp_scan is not built." >&2
  echo "Build it with: cmake --build $PROJECT_DIR/build --target double_pulse_timestamp_scan -j2" >&2
  exit 1
fi

RUNTIME_LIB_PATH="$REPO_ROOT/external/sampic_256ch_lib/lib:$REPO_ROOT/external/sampic_256ch_lib/lpdevc_install/lib"
export LD_LIBRARY_PATH="$RUNTIME_LIB_PATH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$BINARY" --config "$CONFIG" "$@"
