#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
DEFAULT_CONFIG="$SCRIPT_DIR/../../config/external_trigger_batching_scan.pipelined.json"

exec "$SCRIPT_DIR/external_trigger_batching_scan.sh" \
  --config "$DEFAULT_CONFIG" \
  "$@"
