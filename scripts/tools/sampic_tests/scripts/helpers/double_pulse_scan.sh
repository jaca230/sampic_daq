#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
SCRIPTS_ROOT=$(realpath "$SCRIPT_DIR/..")

exec "$SCRIPTS_ROOT/run.sh" --double-pulse -- "$@"
