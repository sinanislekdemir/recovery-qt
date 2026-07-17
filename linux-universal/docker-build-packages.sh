#!/usr/bin/env bash
# File: docker-build-packages.sh
#
# Runs inside the recovery-qt-linux-universal-build container: configures,
# builds, and produces DEB and RPM packages via CPack.
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

set -euo pipefail

VERSION="${VERSION:-$(cmake /src -N 2>/dev/null | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p')}"
VERSION="${VERSION:-7.3.0}"

# ---------------------------------------------------------------------------
# Build recovery-qt (Release)
# ---------------------------------------------------------------------------
cmake -S /src -B /tmp/build-pkg \
    -DCMAKE_BUILD_TYPE=Release \
    "$@"

cmake --build /tmp/build-pkg --parallel

# ---------------------------------------------------------------------------
# Strip debug symbols — rpmbuild does this automatically but CPack DEB
# does not; stripping here keeps both packages consistently small.
# ---------------------------------------------------------------------------
strip /tmp/build-pkg/recovery-qt

# ---------------------------------------------------------------------------
# DEB
# ---------------------------------------------------------------------------
mkdir -p /src/build
cd /tmp/build-pkg && cpack -G DEB -D CPACK_PACKAGE_VERSION="${VERSION}"
cp recovery-qt_*.deb /src/build/
echo "Done: build/recovery-qt_*.deb"

# ---------------------------------------------------------------------------
# RPM
# ---------------------------------------------------------------------------
cd /tmp/build-pkg && cpack -G RPM \
    -D CPACK_PACKAGE_VERSION="${VERSION}" \
    -D CPACK_RPM_PACKAGE_VERSION="${VERSION}"
cp recovery-qt-*.rpm /src/build/
echo "Done: build/recovery-qt-*.rpm"
