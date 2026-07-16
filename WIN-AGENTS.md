# Windows Cross-Build Guide

Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>

All Windows-specific material lives under `win/`. A dedicated Docker image
(`recovery-qt-win-build`) contains the full MinGW-w64 cross toolchain and
every statically built dependency — the host system stays completely clean.

## Quick Build

```sh
make win          # or:  ./win/build.sh
```

The first run builds the Docker image (Qt 6 is compiled from source; ~20 min,
cached thereafter). Subsequent runs only rebuild the application and produce
`build-win/recovery-qt.exe` — a single statically linked 64-bit binary, no
DLLs needed.

## Directory Layout

```
win/
├── Dockerfile              # Cross-compile image (Ubuntu 24.04 + x86_64-w64-mingw32)
├── toolchain.cmake         # CMake toolchain consumed inside the container
├── build.sh                # Host entrypoint: docker build + docker run
├── docker-build.sh         # CMake configure + build, runs inside the container
├── config.h.cmake.in       # Windows build configuration header (TARGET_WIN32)
├── recovery-qt.rc          # PE version info + embedded manifest reference
├── recovery-qt.manifest    # UAC requireAdministrator (replaces pkexec on Linux)
├── README.md               # Usage instructions
├── src/
│   ├── win32.c              # Upstream TestDisk Windows disk layer (Christophe Grenier)
│   ├── win32.h
│   ├── hdwin32.c            # Upstream TestDisk Windows model/serial queries
│   ├── hdwin32.h
│   ├── mingw_compat.c       # POSIX stubs for symbols referenced by linked libs
│   └── core_test_win.c      # Headless wine test harness for the C core
└── stubs/
    ├── pwd.h               # Stub for building libntfs-3g (getpwnam / getpwuid)
    ├── grp.h               # Stub for building libntfs-3g (getgrnam / getgrgid)
    └── syslog.h            # Stub for building libntfs-3g (openlog / syslog / closelog)

build-win/                   # Build output (not committed)
├── recovery-qt.exe          # ~26 MB, PE32+ GUI x86-64, fully static
└── core-test.exe            # Headless diagnostic harness (run under wine or VM)
```

## Dependency Stack (cross-compiled from source)

| Library           | How built                                                              | Notes |
|-------------------|------------------------------------------------------------------------|-------|
| zlib 1.3.1        | `win32/Makefile.gcc` with `CC=x86_64-w64-mingw32-gcc-posix`           | Static-only |
| win-iconv 0.0.8   | CMake + toolchain file                                                  | Static; FindIconv locates it via CMAKE_FIND_ROOT_PATH |
| OpenSSL 3.3.2     | `./Configure mingw64 --cross-compile-prefix=...`                       | Static `libcrypto.a` only (no TLS needed) |
| e2fsprogs 1.47.1  | Autotools, `--host=x86_64-w64-mingw32`                                 | Only `lib/et`, `lib/uuid`, `lib/ext2fs` are compiled |
| libntfs-3g 2022.10.3 | Autotools + 5 sed patches (see below)                               | Read-only via custom device ops; FUSE/plugins disabled |
| Qt 6.7.3          | Host build (no-GUI) → cross build (static, no-sql, no-network)         | qtbase only; `qt_disable_unicode_defines()` applied  |

## ntfs-3g Patches (applied in Dockerfile)

The upstream source tarball needs five small portability fixes for MinGW-w64:

1. **`ntfstime.h`**: `struct timespec` redefinition — the guard `!defined(st_mtime) & !defined(__timespec_defined)` is broken (bitwise `&` and missing `_TIMESPEC_DEFINED`):
   ```sh
   sed -i 's/.../_TIMESPEC_DEFINED/' include/ntfs-3g/ntfstime.h
   ```

2. **`dir.c`**: Transparent union (`index_union`) silently ignored on x86_64 ms_abi. Change the function signature to `void*` and cast callers' arguments:
   ```sh
   sed -i 's/, index_union iu,/, void *iu_raw,/' libntfs-3g/dir.c
   sed -i 's/iu\.ia/((INDEX_ALLOCATION *)iu_raw)/g'  libntfs-3g/dir.c
   sed -i 's/iu\.ir/((INDEX_ROOT *)iu_raw)/g'        libntfs-3g/dir.c
   ```

3. **`compat.h`**: The `#ifdef WINDOWS` block does `#define __attribute__(X) /*nothing*/`, silently deleting `__packed__` from every on-disk struct. `sizeof(NTFS_BOOT_SECTOR)` becomes 520 instead of 512, corrupting every read:
   ```sh
   sed -i '/^#define __attribute__(X)\s*\/\*nothing\*\//d' include/ntfs-3g/compat.h
   ```

4. **`__CRT__NO_INLINE`**: MinGW-w64 headers emit `static inline` wrappers for POSIX-substitute functions (e.g. `fstat`). With `-std=gnu17` these become external definitions that collide in the static archive. The fix is to force `extern` declarations (libmingwex provides the real implementations):
   ```
   CFLAGS="-D__CRT__NO_INLINE ..."
   ```

5. **POSIX stub headers**: `acls.c` and `security.c` unconditionally `#include <pwd.h>` and `<grp.h>`. Provide trivial stubs in `win/stubs/` (the functions are never called in read-only usage).

## Platform-Abstraction Layer

### Disk I/O

| Layer      | Linux path                          | Windows path                |
|------------|-------------------------------------|-----------------------------|
| Enumeration | `hd_parse()` probes `/dev/sd*`, etc. | `hd_parse()` iterates `\\.\PhysicalDriveN` |
| Open       | `open()` + `O_BINARY`               | `CreateFileA()` via `win32.c` |
| Read       | `pread()` or `lseek`+`read`         | `SetFilePointer`+`ReadFile` via `win32.c` |
| Size       | `BLKGETSIZE64` ioctl                | `IOCTL_DISK_GET_LENGTH_INFO` |
| Geometry   | `HDIO_GETGEO` ioctl                 | `IOCTL_DISK_GET_DRIVE_GEOMETRY_EX` |
| Model      | `/sys/dev/block/.../model`          | `IOCTL_STORAGE_QUERY_PROPERTY` |

Source: `src/disk/hdaccess.c` calls into `win/src/{win32,hdwin32}.c` under `#if defined(__MINGW32__)`. These files were restored verbatim from upstream TestDisk (Christophe Grenier, GPL-2.0).

### LUKS Decryption

Pure userspace — zero kernel dependencies on both platforms. Uses OpenSSL EVP
for AES-XTS/CBC plus vendored Argon2 for LUKS2 KDF. The decrypted payload is
presented through a `disk_t` wrapper (`luksdec_pread` / `luksdec_description` /
etc.) so all existing filesystem drivers work unchanged.

### Filesystem Drivers

- **FAT**: Custom structs with `__attribute__((gcc_struct, __packed__))` — MinGW
  respects `gcc_struct`, so no portability issue.
- **NTFS**: Uses `libntfs-3g` exclusively through custom `ntfs_device_operations`
  (`src/fs/ntfs_io.c`). The library's own device layer is disabled
  (`--disable-device-default-io-ops`).
- **EXT2/3/4**: Uses `libext2fs` through a custom `io_manager`
  (`src/fs/ext2_dir.c`). No kernel mount, no FUSE.
- **exFAT**: Own parser in `src/fs/exfat.c` + `exfat_dir.c`.

### GUI

- **Privilege escalation**: Linux uses `pkexec` re-launch; Windows uses an
  embedded UAC manifest (`requireAdministrator`). The `geteuid() != 0` block
  in `src/main.cpp` is fully excluded under `#ifndef _WIN32`.
- **Lock/log files**: `QDir::tempPath()` instead of hardcoded `/tmp`.
- **Preview capture**: Linux uses `open_memstream()`; MinGW uses a temp file
  (`src/core/dir.c`). The temp file is loaded lazily into memory on first
  `get_capture_buffer()` call, then unlinked.

## Known Bugs Fixed (Root Causes)

### Bug 1: No disks detected (real Windows, even as Administrator)

**Cause**: Qt 6 injects `-DUNICODE` via `INTERFACE_COMPILE_DEFINITIONS`. This
tells the Windows SDK headers we want the wide-character API, so `CreateFile`
becomes `CreateFileW` — but `win32.c` passes `const char*` strings (e.g.
`\\.\PhysicalDrive0`). The wide function silently interprets the byte string as
a malformed UTF-16 path, `CreateFileW` returns `INVALID_HANDLE_VALUE`, and zero
disks are found.

**Fix**: `qt_disable_unicode_defines(recovery_qt)` in CMakeLists.txt.

### Bug 2: NTFS "No filesystem detected" / crash

**Cause**: libntfs-3g's `compat.h` defines `#define __attribute__(X) /*nothing*/`
when the `WINDOWS` preprocessor macro is set. This silently removes `__packed__`
from every on-disk struct, changing `sizeof(NTFS_BOOT_SECTOR)` from 512 to 520
bytes. Every read returns shifted garbage. Additionally, the `index_union`
transparent-union has no effect on the Windows ABI, so `ntfs_filldir()` receives
garbage pointers and crashes.

**Fix**: Delete the `__attribute__` removal line from `compat.h` before building
libntfs-3g, and replace the transparent union with explicit `void*` casts.

### Bug 3: FAT32 preview images showed "Corrupted image, can't be previewed"

**Cause**: On MinGW `open_memstream()` is unavailable, so `dir.c` writes
captured preview bytes to a temp file. The `clear_memory_capture()` function
was deleting this temp file before `get_capture_buffer()` had a chance to read
it back — so the GUI always received NULL.

**Fix**: Moved the file-to-memory load (`capture_load_from_file()`) inside
`clear_memory_capture()`, called before the temp file is removed.

## Diagnostic Harness

`win/src/core_test_win.c` compiles to `build-win/core-test.exe`, a headless
Windows console program that exercises the entire C core without Qt:

```
core-test.exe <image> [--luks-pass PASS] [--restore-to DIR]
```

It opens the image, detects the filesystem type, runs `scanner_run()`, dumps
the file tree, calls `read_file_bytes()` for preview, optionally restores
marked files, and runs `carver_run()`. Works under wine for fast iteration:

```sh
make win                                    # build both recovery-qt + core-test
wine build-win/core-test.exe /path/to/image.dd --luks-pass rosebud
```

If the GUI crashes but `core-test.exe` passes, the crash is in the Qt/dialog
layer (threading, Qt::DirectConnection, signal delivery), not in the C core.

## Threading (LUKS Post-Decrypt Crash — Fixed)

The LUKS GUI crash on Windows (decryption succeeded, then scan/file-browser
died) was never reproduced in the headless harness — both `core-test.exe` and
`recovery-qt.exe --headless-test ...` passed under wine. That localised the
failure to the Qt GUI machinery:

- **Root cause**: `Scanner` progress callbacks were `Qt::DirectConnection`,
  which invokes GUI-update slots directly from the worker thread. On Windows
  this is a hard crash (GUI thread affinity enforced); Linux tolerates it more
  often. **Fixed by switching to `Qt::AutoConnection`** in
  `src/wrappers/scanner.cpp` — signals emitted from the worker thread are now
  queued to the GUI thread. Confirmed working on Windows.

## Build Command Reference

```sh
make              # show help
make linux        # native build → build/recovery-qt
make win          # cross build   → build-win/recovery-qt.exe
make format       # clang-format all sources (skips src/vendor, win/src)
make lint         # clang-tidy (needs build/ configured)
make clean        # remove build/ and build-win/
```

## CMake Platform Branches

In `CMakeLists.txt`:

| Item                     | Linux                        | Windows                        |
|--------------------------|------------------------------|--------------------------------|
| Config template          | `config.h.cmake.in`          | `win/config.h.cmake.in`        |
| Target macro             | `TARGET_LINUX`               | `TARGET_WIN32`                 |
| C sources                | `src/` only                  | `src/` + `win/src/*.c`         |
| GUI subsystem            | (none)                       | `WIN32_EXECUTABLE ON`          |
| Unicode defines          | (none)                       | `qt_disable_unicode_defines()` |
| Link options             | (none)                       | `-static -static-libgcc -static-libstdc++ -s` |
| Link libraries           | `${CMAKE_DL_LIBS}`           | `ws2_32 crypt32`                |
| Install rules            | `.desktop`, `.png`, deb CPack | skipped                        |

## Wine Testing Notes

- Physical drives (`\\.\PhysicalDriveN`) are not accessible under wine.
  Test with disk images (`*.img`, `*.dd`) via "Open Image" or the `core-test.exe`
  harness.
- UAC manifests are ignored by wine (process always starts unprivileged).
- Wine 9.0 (Ubuntu 24.04) works: all three bug-fixing harness runs passed.
  To test the full GUI under wine:
  ```sh
  RECOVERY_QT_LUKS_PASS=rosebud wine build-win/recovery-qt.exe
  ```
  The env-var auto-fills the LUKS password dialog. Manual clicks are still
  needed for "Open Image" and "Scan".
