#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
REPO_ROOT=$(realpath "$PROJECT_DIR/../../..")
BINARY="$PROJECT_DIR/build/bin/trigger_startup_readiness_test"

if [[ ! -x "$BINARY" ]]; then
  echo "trigger_startup_readiness_test not found at $BINARY. Run scripts/build.sh first." >&2
  exit 1
fi

export LD_LIBRARY_PATH="$REPO_ROOT/build/lib:$REPO_ROOT/external/sampic_256ch_lib/lpdevc_install/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$BINARY" "$@"
