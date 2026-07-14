#!/bin/sh
set -e

BUILD_DIR="build"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "==> Configuring..."
cmake -S . -B "$BUILD_DIR"

echo "==> Building..."
cmake --build "$BUILD_DIR" -- -j"$NPROC"

echo "==> Packaging .deb..."
cd "$BUILD_DIR" && cpack -G DEB

DEB_FILE=$(ls recovery-qt_*.deb 2>/dev/null | head -1)
if [ -n "$DEB_FILE" ]; then
    echo "==> Done: $BUILD_DIR/$DEB_FILE"
fi
