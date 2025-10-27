#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")
BUILD_DIR="$PROJECT_DIR/build"
MODE="pulser"
POSITIONAL=()

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

exec "$BINARY" "${POSITIONAL[@]}"
