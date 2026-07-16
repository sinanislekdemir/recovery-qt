# AGENTS.md

## Project Overview

recovery-qt is a standalone fork of testdisk-7.3-WIP providing a Qt6-based
deleted-file browser, selective restore tool, raw file carver, filesystem
backup/restore, and native LUKS1/LUKS2 decryption support.

**Binaries**:
- `build/recovery-qt` — native Linux build (Qt6 Widgets)
- `build-win/recovery-qt.exe` — Windows cross-build (static, MinGW-w64 via Docker)

- DO NOT CHANGE any file license headers. Those headers show the clear ownership and hard work by the creators.
- New features and new files must be marked for Sinan Islekdemir <sinan@islekdemir.com>
- Backwards compatibility with the remote/original project is not important.
- Always run formatter and linter at the end of a task.
- Windows-specific material lives under `win/`; see `WIN-AGENTS.md` for the
  cross-build guide, platform-abstraction notes, and wine testing workflow.

## Build

```sh
make linux                   # native Linux build (cmake configure + build)
make win                     # Windows cross-build via Docker → build-win/recovery-qt.exe
cmake -B build -S .          # configure (exports build/compile_commands.json)
cmake --build build -j       # build
```

The build MUST stay warning-free (`-Wall -Wextra`). Warnings are treated as
early signs of a code smell and must be fixed, not ignored. Two categories are
deliberately disabled in `CMakeLists.txt` because they are architectural rather
than smells: `-Wno-unused-parameter` (photorec plugins/callbacks share fixed
signatures) and `-Wno-address-of-packed-member` (on-disk FAT/NTFS structures on
the Linux/x86_64 target). Vendored `src/vendor/argon2` is not linted.

## Formatter and Linter

Run BOTH at the end of every task:

```sh
scripts/format.sh            # clang-format all sources in place (skips src/vendor)
scripts/format.sh --check    # verify formatting without editing (CI-friendly)
scripts/lint.sh              # clang-tidy over the project (needs build/ configured)
scripts/lint.sh src/core/foo.c   # lint specific file(s)
```

- Formatting is defined by `.clang-format` (LLVM base, 2-space indent, 120 cols,
  includes are never reordered because order is semantically significant).
- Static analysis is defined by `.clang-tidy` (bugprone, clang-analyzer,
  performance, portability). Fix real defects it reports (use-after-free,
  leaks, unused-but-set variables, etc.). Known false positives from intrusive
  linked lists (`container_of`) and ownership passed through function-pointer
  callbacks may remain.


