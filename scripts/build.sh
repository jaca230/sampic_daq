#!/bin/bash

set -e

# Resolve absolute paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")
BASE_DIR=$(realpath "$SCRIPT_DIR/..")
BUILD_DIR="$BASE_DIR/build"
CLEANUP_SCRIPT="$SCRIPT_DIR/cleanup.sh"

# Default flags
OVERWRITE=false
DEFAULT_JOBS=$(command -v nproc >/dev/null 2>&1 && nproc || getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
JOBS_ARG="-j${DEFAULT_JOBS}"  # Use available processors by default

# Help message
show_help() {
    echo "Usage: ./build.sh [OPTIONS]"
    echo
    echo "Options:"
    echo "  -o, --overwrite           Remove existing build directory before building"
    echo "  -j, --jobs <number>       Specify number of processors to use (default: all available)"
    echo "  -h, --help                Display this help message"
}

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -o|--overwrite)
            OVERWRITE=true
            shift
            ;;
        -j|--jobs)
            if [[ -n "$2" && "$2" != -* ]]; then
                JOBS_ARG="-j$2"
                shift 2
            else
                JOBS_ARG="-j"
                shift
            fi
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "[build.sh, ERROR] Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Optionally clean build
if [ "$OVERWRITE" = true ]; then
    echo "[build.sh] Cleaning previous build with: $CLEANUP_SCRIPT"
    "$CLEANUP_SCRIPT"
fi

# Ensure git submodules are available
if [ -f "$BASE_DIR/.gitmodules" ]; then
    echo "[build.sh] Ensuring git submodules are initialized"
    git -C "$BASE_DIR" submodule update --init --recursive
fi

# Create and enter build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR" || exit 1

# Run CMake and Make
echo "[build.sh] Running cmake in: $BUILD_DIR"
HOST_CC="${SAMPIC_HOST_CC:-/usr/bin/cc}"
HOST_CXX="${SAMPIC_HOST_CXX:-/usr/bin/c++}"
if [ ! -x "$HOST_CC" ] || [ ! -x "$HOST_CXX" ]; then
    echo "[build.sh, ERROR] Host compiler not found: CC=$HOST_CC CXX=$HOST_CXX" >&2
    exit 1
fi
echo "[build.sh] Using host toolchain: $HOST_CXX"
cmake \
    -S "$BASE_DIR" \
    -B "$BUILD_DIR" \
    -DCMAKE_C_COMPILER="$HOST_CC" \
    -DCMAKE_CXX_COMPILER="$HOST_CXX"

echo "[build.sh] Building with make $JOBS_ARG"
make $JOBS_ARG

ensure_ftd2xx_symlink() {
    local vendor_dir="$BASE_DIR/external/sampic_256ch_lib/lpdevc_install"
    if [ ! -d "$vendor_dir" ]; then
        return
    fi

    local newest
    newest=$(ls "$vendor_dir"/libftd2xx.so.* 2>/dev/null | sort -V | tail -n 1)
    if [ -z "$newest" ]; then
        return
    fi

    if [ ! -e "$vendor_dir/libftd2xx.so" ]; then
        (cd "$vendor_dir" && ln -s "$(basename "$newest")" libftd2xx.so)
        echo "[build.sh] Created libftd2xx.so symlink -> $(basename "$newest")"
    fi
}

ensure_ftd2xx_symlink

echo "[build.sh] Build complete."
echo "[build.sh] Executables are in: $BUILD_DIR/bin/"
echo "[build.sh] Libraries are in: $BUILD_DIR/lib/"
