#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
REPO_ROOT=$(realpath "$SCRIPT_DIR/../../../../..")
BINARY="$PROJECT_DIR/build/bin/l2_external_gate_probe"
VENDOR_LIB_DIR="$REPO_ROOT/external/sampic_256ch_lib/lib"
LPDEVC_LIB_DIR="$REPO_ROOT/external/sampic_256ch_lib/lpdevc_install/lib"

if [[ ! -x "$BINARY" ]]; then
  echo "l2_external_gate_probe is not built. Run scripts/build.sh first." >&2
  exit 1
fi

export LD_LIBRARY_PATH="$VENDOR_LIB_DIR:$LPDEVC_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$BINARY" "$@"
