#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")
BUILD_DIR="$PROJECT_DIR/build"
MODE="pulser"
POSITIONAL=()
PROJECT_ROOT=$(realpath "$SCRIPT_DIR/../../../..")
VENDOR_LIB_DIR="$PROJECT_ROOT/external/sampic_256ch_lib/lib"
LPDEVC_LIB_DIR="$PROJECT_ROOT/external/sampic_256ch_lib/lpdevc_install/lib"
declare -a RUNTIME_LIB_PATHS=()

print_help() {
  cat <<EOF
Usage: $0 [--pulser | --smoke] [-- <args>]

Options:
  --pulser        Run the pulser rate test (default)
  --smoke         Run the crate smoke test
  -h, --help      Show this help message

Arguments after '--' are passed directly to the selected binary.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pulser)
      MODE="pulser"
      shift
      ;;
    --smoke)
      MODE="smoke"
      shift
      ;;
    -h|--help)
      print_help
      exit 0
      ;;
    --)
      shift
      POSITIONAL+=("$@")
      break
      ;;
    *)
      POSITIONAL+=("$1")
      shift
      ;;
  esac
done

case "$MODE" in
  pulser)
    BINARY="$BUILD_DIR/bin/sampic_pulser_rate"
    ;;
  smoke)
    BINARY="$BUILD_DIR/bin/sampic_crate_smoke"
    ;;
  *)
    echo "Unknown mode: $MODE" >&2
    exit 1
    ;;
esac

if [[ ! -x "$BINARY" ]]; then
  echo "Binary not found at $BINARY. Build it first with ./build.sh" >&2
  exit 1
fi

if [[ -d "$VENDOR_LIB_DIR" ]]; then
  RUNTIME_LIB_PATHS+=("$VENDOR_LIB_DIR")
fi

if [[ -d "$LPDEVC_LIB_DIR" ]]; then
  RUNTIME_LIB_PATHS+=("$LPDEVC_LIB_DIR")
fi

if [[ ${#RUNTIME_LIB_PATHS[@]} -gt 0 ]]; then
  RUNTIME_LIB_PATH=$(IFS=:; echo "${RUNTIME_LIB_PATHS[*]}")
  if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
    export LD_LIBRARY_PATH="$RUNTIME_LIB_PATH:$LD_LIBRARY_PATH"
  else
    export LD_LIBRARY_PATH="$RUNTIME_LIB_PATH"
  fi
fi

exec "$BINARY" "${POSITIONAL[@]}"
