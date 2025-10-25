#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")
BUILD_DIR="$PROJECT_DIR/build"
BINARY="$BUILD_DIR/bin/sampic_pulser_rate"

if [[ ! -x "$BINARY" ]]; then
  echo "Binary not found at $BINARY. Build it first with ./build.sh" >&2
  exit 1
fi

exec "$BINARY" "$@"
