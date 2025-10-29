#!/bin/bash
# Run script for SAMPIC Playground

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
EXE="$BUILD_DIR/bin/sampic_playground"

# Check if executable exists
if [ ! -f "$EXE" ]; then
    echo "Error: Executable not found: $EXE"
    echo "Please run build.sh first"
    exit 1
fi

# Default duration is 10 seconds, can be overridden with first argument
DURATION="${1:-10}"

echo "========================================="
echo "  Running SAMPIC Playground"
echo "========================================="
echo "Duration: $DURATION seconds"
echo ""

# Run the benchmark
"$EXE" "$DURATION"

echo ""
echo "Done!"
