# Windows cross-build

Everything Windows-specific lives in this directory. The cross toolchain and
all statically built dependencies are contained in a dedicated Docker image so
the host system stays clean.

## Build

```sh
./win/build.sh
```

The first run builds the Docker image (`recovery-qt-win-build`), which
cross-compiles the full static dependency stack with MinGW-w64:

- zlib
- win-iconv
- OpenSSL (libcrypto)
- e2fsprogs (libext2fs, libcom_err, libuuid)
- ntfs-3g (libntfs-3g, used read-only through custom device ops)
- Qt 6 (qtbase, static)

This takes a while (Qt is built from source) but is cached. Subsequent runs
only rebuild the application and produce `build-win/recovery-qt.exe` - a
single statically linked binary, no DLLs to ship.

## Layout

- `Dockerfile` - the cross-compile image
- `toolchain.cmake` - CMake toolchain used inside the container
- `build.sh` - host entrypoint (docker build + docker run)
- `docker-build.sh` - runs inside the container
- `config.h.cmake.in` - Windows build configuration header template
- `src/win32.c`, `src/hdwin32.c` - Windows disk access layer, restored
  verbatim from upstream TestDisk (Christophe Grenier)
- `recovery-qt.rc`, `recovery-qt.manifest` - version info and UAC manifest
  (`requireAdministrator` replaces the pkexec flow used on Linux)

## Notes

- Elevation is handled by the embedded UAC manifest; Windows prompts before
  the process starts.
- Lock and log files go to the user's temp directory
  (`QDir::tempPath()/recovery-qt.{pid,log}`).
