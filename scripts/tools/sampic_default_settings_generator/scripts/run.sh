#!/bin/bash

# --------------------------------------------------------------------------
# Save original working directory
# --------------------------------------------------------------------------
ORIG_DIR=$(pwd)

# --------------------------------------------------------------------------
# Get the absolute path of the script directory
# --------------------------------------------------------------------------
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR="$(cd -P "$(dirname "$SOURCE")" >/dev/null 2>&1 && pwd)"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
SCRIPT_DIR="$(cd -P "$(dirname "$SOURCE")" >/dev/null 2>&1 && pwd)"
BASE_DIR="$SCRIPT_DIR/.."

# --------------------------------------------------------------------------
# Default flags
# --------------------------------------------------------------------------
DEBUG=false
VALGRIND=false
EXE_ARGS=()

# --------------------------------------------------------------------------
# Help message
# --------------------------------------------------------------------------
show_help() {
    echo "Usage: $0 [OPTIONS] [-- <args>]"
    echo
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -d, --debug    Run with gdb"
    echo "  -v, --valgrind Run with valgrind"
    echo
    echo "Arguments after '--' are passed directly to the executable."
    exit 0
}

# --------------------------------------------------------------------------
# Parse arguments
# --------------------------------------------------------------------------
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        -d|--debug)
            DEBUG=true
            shift
            ;;
        -v|--valgrind)
            VALGRIND=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        --)
            shift
            EXE_ARGS+=("$@")
            break
            ;;
        *)
            echo "[ERROR] Unknown option: $1"
            show_help
            ;;
    esac
done

# --------------------------------------------------------------------------
# Define executable path
# --------------------------------------------------------------------------
EXECUTABLE="$BASE_DIR/build/bin/sampic_defaults"

if [ ! -x "$EXECUTABLE" ]; then
    echo "[ERROR] Executable not found or not executable: $EXECUTABLE"
    exit 1
fi

# --------------------------------------------------------------------------
# Run executable
# --------------------------------------------------------------------------
cd "$BASE_DIR" || exit 1

echo "[INFO] Running sampic_defaults..."

if [ "$DEBUG" = true ]; then
    gdb --args "$EXECUTABLE" "${EXE_ARGS[@]}"
elif [ "$VALGRIND" = true ]; then
    valgrind --leak-check=full --track-origins=yes "$EXECUTABLE" "${EXE_ARGS[@]}"
else
    "$EXECUTABLE" "${EXE_ARGS[@]}"
fi

# --------------------------------------------------------------------------
# Return to original directory
# --------------------------------------------------------------------------
cd "$ORIG_DIR"
