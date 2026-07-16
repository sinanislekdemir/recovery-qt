#!/bin/sh
#
#  File: format.sh - run clang-format over the project sources
#
#  Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
#
#  Usage:
#    scripts/format.sh          # format all sources in place
#    scripts/format.sh --check  # exit non-zero if formatting is needed
#
set -eu

cd "$(dirname "$0")/.."

MODE="-i"
if [ "${1:-}" = "--check" ]; then
  MODE="--dry-run -Werror"
fi

find src \( -path src/vendor -o -path src/.ccls-cache \) -prune -o \
  \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) -print |
  xargs clang-format $MODE
