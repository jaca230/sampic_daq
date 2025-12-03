#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PYTHON="${PYTHON:-python3}"

exec "$PYTHON" "$SCRIPT_DIR/pulse_loop.py" "$@"
