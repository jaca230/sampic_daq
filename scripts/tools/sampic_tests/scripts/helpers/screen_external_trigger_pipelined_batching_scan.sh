#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
export SCREEN_SESSION=${SCREEN_SESSION:-external_trigger_pipelined_batching_scan}
CMD="$SCRIPT_DIR/external_trigger_pipelined_batching_scan.sh"
DEFAULT_CONFIG="$SCRIPT_DIR/../../config/external_trigger_batching_scan.pipelined.json"

if [[ ! -x "$CMD" ]]; then
  echo "Error: $CMD not found or not executable." >&2
  exit 1
fi

exec "$SCRIPT_DIR/screen_external_trigger_batching_scan.sh" \
  --config "$DEFAULT_CONFIG" \
  "$@"
