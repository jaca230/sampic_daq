#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
exec "${PYTHON:-python3}" "$SCRIPT_DIR/dual_crate_external_generator_rate_scan.py" \
  --config "$PROJECT_DIR/config/agilent_solo_rate_scan.default.json" "$@"
