#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")
BUILD_DIR="$PROJECT_DIR/build"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" "$@"
cmake --build "$BUILD_DIR"
