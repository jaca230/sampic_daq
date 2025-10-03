#!/bin/bash

# --------------------------------------------------------------------------
# Save original working directory
# --------------------------------------------------------------------------
ORIG_DIR=$(pwd)

# --------------------------------------------------------------------------
# Resolve script directory
# --------------------------------------------------------------------------
SOURCE="${BASH_SOURCE[0]}"
while [ -L "$SOURCE" ]; do
    DIR="$(cd -P "$(dirname "$SOURCE")" >/dev/null 2>&1 && pwd)"
    SOURCE="$(readlink "$SOURCE")"
    [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
SCRIPT_DIR="$(cd -P "$(dirname "$SOURCE")" >/dev/null 2>&1 && pwd)"
BASE_DIR="$SCRIPT_DIR/../.."

# --------------------------------------------------------------------------
# Defaults
# --------------------------------------------------------------------------
INDEX=""
DEBUG=false
VALGRIND=false
PRELOAD_LIBS=""
EXE_ARGS=()

# --------------------------------------------------------------------------
# Help
# --------------------------------------------------------------------------
show_help() {
    echo "Usage: $0 -i <index> [OPTIONS] [-- <args>]"
    echo
    echo "Required:"
    echo "  -i <index>           Frontend index (0-99)"
    echo
    echo "Options:"
    echo "  -d, --debug          Run with gdb"
    echo "  -v, --valgrind       Run with valgrind"
    echo "  --preload <libs>     Comma-separated list of libraries for LD_PRELOAD"
    echo
    echo "Additional arguments after '--' are passed to the executable."
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
                echo "[ERROR] --preload requires a comma-separated list"
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

if ! [[ "$INDEX" =~ ^[0-9]+$ ]] || [ "$INDEX" -lt 0 ] || [ "$INDEX" -gt 99 ]; then
    echo "[ERROR] Index must be an integer between 0 and 99."
    exit 1
fi

INDEX_STR=$(printf "%02d" "$INDEX")

# --------------------------------------------------------------------------
# Executable path
# --------------------------------------------------------------------------
EXECUTABLE="$BASE_DIR/build/sampic_frontend"
if [ ! -x "$EXECUTABLE" ]; then
    echo "[ERROR] Executable not found or not executable: $EXECUTABLE"
    exit 1
fi

# --------------------------------------------------------------------------
# Screen session
# --------------------------------------------------------------------------
SESSION_NAME="sampic_frontend${INDEX_STR}"

if screen -list | grep -q "\.${SESSION_NAME}"; then
    echo "[ERROR] Screen session '$SESSION_NAME' already exists."
    exit 1
fi

if [ "$DEBUG" = true ]; then
    CMD="cd \"$BASE_DIR\" && gdb --args $EXECUTABLE -i $INDEX ${EXE_ARGS[*]}"
elif [ "$VALGRIND" = true ]; then
    CMD="cd \"$BASE_DIR\" && valgrind --leak-check=full --track-origins=yes $EXECUTABLE -i $INDEX ${EXE_ARGS[*]}"
else
    CMD="cd \"$BASE_DIR\" && $EXECUTABLE -i $INDEX ${EXE_ARGS[*]}"
fi


# Start screen detached
screen -S "$SESSION_NAME" -dm bash -c "$CMD"

# Verify creation
if screen -list | grep -q "\.${SESSION_NAME}"; then
    echo "Started '$EXECUTABLE' with index $INDEX in screen session '$SESSION_NAME'."
else
    echo "[ERROR] Failed to start screen session '$SESSION_NAME'."
    exit 1
fi

cd "$ORIG_DIR"
