#!/usr/bin/env bash
# File: build.sh
#
# Host-side entrypoint: builds the dedicated cross-compile Docker image
# (cached after the first run) and produces build-win/recovery-qt.exe.
# Nothing is installed on the host.
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

set -euo pipefail

cd "$(dirname "$0")/.."

IMAGE=recovery-qt-win-build

docker build -t "$IMAGE" win/

docker run --rm \
    -u "$(id -u):$(id -g)" \
    -v "$PWD:/src" \
    -w /src \
    "$IMAGE" \
    win/docker-build.sh "$@"

echo
echo "Done: build-win/recovery-qt.exe"
