#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
REPO_ROOT=$(realpath "$PROJECT_DIR/../../..")
BINARY="$PROJECT_DIR/build/bin/sampling_frequency_waveform_scan"
DEFAULT_CONFIG="$PROJECT_DIR/config/sampling_frequency_waveform_scan.crate_n1.json"

if [[ ! -x "$BINARY" ]]; then
  echo "sampling_frequency_waveform_scan is not built." >&2
  echo "Build it with: cmake --build $PROJECT_DIR/build --target sampling_frequency_waveform_scan -j2" >&2
  exit 1
fi

export LD_LIBRARY_PATH="$REPO_ROOT/build/lib:$REPO_ROOT/external/sampic_256ch_lib/lpdevc_install/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

has_config=false
for argument in "$@"; do
  [[ "$argument" == "--config" ]] && has_config=true
done
if [[ "$has_config" == false ]]; then
  exec "$BINARY" --config "$DEFAULT_CONFIG" "$@"
fi
exec "$BINARY" "$@"
