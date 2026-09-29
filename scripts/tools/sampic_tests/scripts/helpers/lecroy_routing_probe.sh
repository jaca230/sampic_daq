#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
REPO_ROOT=$(realpath "$PROJECT_DIR/../../..")
BINARY="$PROJECT_DIR/build/bin/lecroy_routing_probe"
DEFAULT_CONFIG="$PROJECT_DIR/config/lecroy_routing_probe.default.json"

if [[ ! -x "$BINARY" ]]; then
  echo "lecroy_routing_probe is not built." >&2
  echo "Build it with: cmake --build $PROJECT_DIR/build --target lecroy_routing_probe -j2" >&2
  exit 1
fi

RUNTIME_LIB_PATH="$REPO_ROOT/external/sampic_256ch_lib/lib:$REPO_ROOT/external/sampic_256ch_lib/lpdevc_install/lib"
export LD_LIBRARY_PATH="$RUNTIME_LIB_PATH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

has_config=false
for argument in "$@"; do [[ "$argument" == "--config" ]] && has_config=true; done
if [[ "$has_config" == false ]]; then
  exec "$BINARY" --config "$DEFAULT_CONFIG" "$@"
fi
exec "$BINARY" "$@"
