#!/bin/bash
# Build script for SAMPIC Playground

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "========================================="
echo "  Building SAMPIC Playground"
echo "========================================="
echo "Project:   $PROJECT_DIR"
echo "Build dir: $BUILD_DIR"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake
echo "Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building..."
make -j$(nproc)

echo ""
echo "========================================="
echo "  Build complete!"
echo "  Executable: $BUILD_DIR/bin/sampic_playground"
echo "========================================="
