#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(realpath "$0")")

PARPORT_TRIGGER_DIR="${PARPORT_TRIGGER_DIR:-/home/pioneer/packages/parport_trigger}"
BUILD_FIRST=false
PASSTHROUGH_ARGS=()

show_help() {
    cat <<'EOF'
Usage: ./scripts/parport_trigger/setup_parport_trigger_device.sh [options] [-- <setup args>]

Options:
  --repo <path>    Path to parport_trigger repository
  --build          Build parport_trigger userspace + kernel module before setup
  -h, --help       Show this help

Any remaining args are forwarded to:
  <repo>/scripts/setup_trigger_device.sh

Examples:
  ./scripts/parport_trigger/setup_parport_trigger_device.sh --build --overwrite
  PARPORT_TRIGGER_DIR=/path/to/parport_trigger ./scripts/parport_trigger/setup_parport_trigger_device.sh --parport-number 0 --overwrite
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --repo)
            if [[ $# -lt 2 ]]; then
                echo "[setup_parport_trigger_device.sh, ERROR] --repo expects a path" >&2
                exit 1
            fi
            PARPORT_TRIGGER_DIR=$(realpath "$2")
            shift 2
            ;;
        --build)
            BUILD_FIRST=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        --)
            shift
            while [[ $# -gt 0 ]]; do
                PASSTHROUGH_ARGS+=("$1")
                shift
            done
            ;;
        *)
            PASSTHROUGH_ARGS+=("$1")
            shift
            ;;
    esac
done

if [[ ! -d "$PARPORT_TRIGGER_DIR" ]]; then
    echo "[setup_parport_trigger_device.sh, ERROR] Repository not found: $PARPORT_TRIGGER_DIR" >&2
    echo "Set PARPORT_TRIGGER_DIR or pass --repo <path>" >&2
    exit 1
fi

BUILD_SCRIPT="$PARPORT_TRIGGER_DIR/scripts/build.sh"
SETUP_SCRIPT="$PARPORT_TRIGGER_DIR/scripts/setup_trigger_device.sh"

if [[ "$BUILD_FIRST" == true ]]; then
    if [[ ! -x "$BUILD_SCRIPT" ]]; then
        echo "[setup_parport_trigger_device.sh, ERROR] Missing executable build script: $BUILD_SCRIPT" >&2
        exit 1
    fi
    echo "[setup_parport_trigger_device.sh] Building parport_trigger from $PARPORT_TRIGGER_DIR"
    "$BUILD_SCRIPT" -o
fi

if [[ ! -x "$SETUP_SCRIPT" ]]; then
    echo "[setup_parport_trigger_device.sh, ERROR] Missing executable setup script: $SETUP_SCRIPT" >&2
    exit 1
fi

echo "[setup_parport_trigger_device.sh] Running trigger device setup"
"$SETUP_SCRIPT" "${PASSTHROUGH_ARGS[@]}"
