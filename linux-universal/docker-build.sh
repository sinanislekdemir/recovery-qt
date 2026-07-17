#!/usr/bin/env bash
# File: docker-build.sh
#
# Runs inside the recovery-qt-linux-universal-build container: configures,
# builds, installs to a staged AppDir, bundles all Qt/dependency libraries via
# linuxdeploy, and creates the portable tarball.
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

set -euo pipefail

# ---------------------------------------------------------------------------
# Build recovery-qt (Release, no LTO overhead)
# ---------------------------------------------------------------------------
cmake -S /src -B /tmp/build-appdir \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    "$@"

cmake --build /tmp/build-appdir --parallel

# ---------------------------------------------------------------------------
# Staged install into /tmp/appdir (becomes the portable AppDir root)
# ---------------------------------------------------------------------------
cmake --install /tmp/build-appdir --prefix /tmp/appdir/usr

# ---------------------------------------------------------------------------
# linuxdeploy: bundle non-system shared libraries, set rpath, create qt.conf
# ---------------------------------------------------------------------------
export QMAKE=qmake6
/usr/local/bin/linuxdeploy \
    --appdir=/tmp/appdir \
    --plugin qt

# ---------------------------------------------------------------------------
# Create versioned tarball
# ---------------------------------------------------------------------------
VERSION=$(cmake /src -N 2>/dev/null | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p' || echo "7.3.0")
VERSION="${VERSION:-7.3.0}"
OUTPUT="recovery-qt-${VERSION}-x86_64.tar.gz"

mkdir -p /src/build
tar czf "/src/build/${OUTPUT}" -C /tmp appdir

echo "Done: build/${OUTPUT}"
