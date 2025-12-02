#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/../..")
BUILD_DIR="$PROJECT_DIR/build"

BINARY="$BUILD_DIR/bin/lecroy_test"

if [[ ! -x "$BINARY" ]]; then
  echo "Lecroy test binary not found at $BINARY. Run scripts/build.sh first." >&2
  exit 1
fi

exec "$BINARY" "$@"
