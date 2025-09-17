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
# Default settings
# --------------------------------------------------------------------------
INDEX=""
DEBUG=false
VALGRIND=false
PRELOAD_LIBS=""
EXE_ARGS=()

# --------------------------------------------------------------------------
# Help message
# --------------------------------------------------------------------------
show_help() {
    echo "Usage: $0 -i <index> [OPTIONS] [-- <args>]"
    echo
    echo "Required:"
    echo "  -i <index>           Set frontend index (integer 0-99)"
    echo
    echo "Options:"
    echo "  -h, --help           Show this help message"
    echo "  -d, --debug          Run with gdb"
    echo "  -v, --valgrind       Run with valgrind"
    echo "  --preload <libs>     Comma-separated list of libraries to LD_PRELOAD"
    echo
    echo "Arguments after '--' are passed directly to the executable."
    exit 1
}

# --------------------------------------------------------------------------
# Parse arguments
# --------------------------------------------------------------------------
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        -i)
            INDEX="$2"
            shift 2
            ;;
        -d|--debug)
            DEBUG=true
            shift
            ;;
        -v|--valgrind)
            VALGRIND=true
            shift
            ;;
        --preload)
            if [[ -n "$2" && "$2" != -* ]]; then
                PRELOAD_LIBS="${2//,/':' }"
                shift 2
            else
                echo "[ERROR] --preload requires a comma-separated list of paths"
                exit 1
            fi
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
# Validate index
# --------------------------------------------------------------------------
if [ -z "$INDEX" ]; then
    echo "[ERROR] -i <index> is required."
    show_help
fi

if ! [[ "$INDEX" =~ ^[0-9]+$ ]]; then
    echo "[ERROR] Index must be an integer (0-99)."
    exit 1
fi

if [ "$INDEX" -lt 0 ] || [ "$INDEX" -gt 99 ]; then
    echo "[ERROR] Index must be between 0 and 99."
    exit 1
fi

INDEX_STR=$(printf "%02d" "$INDEX")

# --------------------------------------------------------------------------
# Define executable path
# --------------------------------------------------------------------------
EXECUTABLE="$BASE_DIR/build/sampic_frontend"

if [ ! -x "$EXECUTABLE" ]; then
    echo "[ERROR] Executable not found or not executable: $EXECUTABLE"
    exit 1
fi

# --------------------------------------------------------------------------
# Run executable
# --------------------------------------------------------------------------
cd "$BASE_DIR" || exit 1

echo "[INFO] Running Sampic Frontend with index $INDEX_STR..."

if [ "$DEBUG" = true ]; then
    gdb --args "$EXECUTABLE" -i "$INDEX" "${EXE_ARGS[@]}"
elif [ "$VALGRIND" = true ]; then
    valgrind --leak-check=full --track-origins=yes "$EXECUTABLE" -i "$INDEX" "${EXE_ARGS[@]}"
else
    "$EXECUTABLE" -i "$INDEX" "${EXE_ARGS[@]}"
fi

# --------------------------------------------------------------------------
# Return to original directory
# --------------------------------------------------------------------------
cd "$ORIG_DIR"
