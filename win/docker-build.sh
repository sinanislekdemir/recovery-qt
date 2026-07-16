#!/usr/bin/env bash
# File: docker-build.sh
#
# Runs inside the recovery-qt-win-build container: configures and builds the
# Windows binary into build-win/.
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

set -euo pipefail

cmake -S . -B build-win -GNinja \
    -DCMAKE_TOOLCHAIN_FILE=win/toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_HOST_PATH="${QT_HOST:-/opt/qt-host}" \
    "$@"

cmake --build build-win --parallel
