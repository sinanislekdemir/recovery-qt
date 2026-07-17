#!/usr/bin/env bash
# File: build.sh
#
# Host-side entrypoint: builds the dedicated Ubuntu 22.04 Docker image
# (cached after the first run) and produces the portable Linux tarball
# in build/recovery-qt-<version>-x86_64.tar.gz.
# Nothing is installed on the host.
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

set -euo pipefail

cd "$(dirname "$0")/.."

IMAGE=recovery-qt-linux-universal-build

docker build -t "$IMAGE" linux-universal/

docker run --rm \
    -u "$(id -u):$(id -g)" \
    -v "$PWD:/src" \
    -w /src \
    "$IMAGE" \
    /build/docker-build.sh "$@"

echo
echo "Done: build/recovery-qt-*-x86_64.tar.gz"
