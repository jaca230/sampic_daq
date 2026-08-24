#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
TRIGGER_PROBE="$SCRIPT_DIR/trigger_probe.sh"
DEFAULT_CONFIG="$PROJECT_DIR/config/double_pulse_deadtime_scan.default.json"
DEFAULT_OUTPUT_DIR="$PROJECT_DIR/data/external_trigger_probe/latest"

declare -a OUTPUT_ARGS=()
HAS_OUTPUT_DIR=false
for arg in "$@"; do
  if [[ "$arg" == "--output-dir" ]]; then
    HAS_OUTPUT_DIR=true
    break
  fi
done
if [[ "$HAS_OUTPUT_DIR" == false ]]; then
  OUTPUT_ARGS=(--output-dir "$DEFAULT_OUTPUT_DIR")
fi

exec "$TRIGGER_PROBE" \
  --config "$DEFAULT_CONFIG" \
  --self-trigger-channels \
  --l2-external-gate \
  --all-channels \
  --skip-lecroy \
  --manage-lecroy-output \
  --lecroy-output-channel B \
  "${OUTPUT_ARGS[@]}" \
  "$@"
