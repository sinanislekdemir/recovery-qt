#!/usr/bin/env bash
# File: docker-build-packages.sh
#
# Runs inside the recovery-qt-linux-universal-build container: configures,
# builds, and produces DEB and RPM packages via CPack.
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

set -euo pipefail

# ---------------------------------------------------------------------------
# Build recovery-qt (Release)
# ---------------------------------------------------------------------------
cmake -S /src -B /tmp/build-pkg \
    -DCMAKE_BUILD_TYPE=Release \
    "$@"

cmake --build /tmp/build-pkg --parallel

# ---------------------------------------------------------------------------
# DEB
# ---------------------------------------------------------------------------
cpack -G DEB -B /tmp/build-pkg
mkdir -p /src/build
cp /tmp/build-pkg/recovery-qt_*.deb /src/build/
echo "Done: build/recovery-qt_*.deb"

# ---------------------------------------------------------------------------
# RPM
# ---------------------------------------------------------------------------
cpack -G RPM -B /tmp/build-pkg
cp /tmp/build-pkg/recovery-qt-*.rpm /src/build/
echo "Done: build/recovery-qt-*.rpm"
