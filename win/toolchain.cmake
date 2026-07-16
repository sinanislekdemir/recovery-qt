# File: toolchain.cmake
#
# CMake toolchain for cross-compiling recovery-qt to Windows (x86_64)
# with MinGW-w64 inside the dedicated Docker image (see win/Dockerfile).
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_TRIPLET x86_64-w64-mingw32)
if(NOT DEFINED WIN_SYSROOT)
  set(WIN_SYSROOT /opt/win-sysroot)
endif()

set(CMAKE_C_COMPILER ${MINGW_TRIPLET}-gcc-posix)
set(CMAKE_CXX_COMPILER ${MINGW_TRIPLET}-g++-posix)
set(CMAKE_RC_COMPILER ${MINGW_TRIPLET}-windres)
set(CMAKE_AR ${MINGW_TRIPLET}-ar)
set(CMAKE_RANLIB ${MINGW_TRIPLET}-ranlib)

set(CMAKE_FIND_ROOT_PATH ${WIN_SYSROOT} /usr/${MINGW_TRIPLET})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_PREFIX_PATH ${WIN_SYSROOT})

# Static-only dependency tree lives in the sysroot.
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)

set(ENV{PKG_CONFIG_LIBDIR} "${WIN_SYSROOT}/lib/pkgconfig:${WIN_SYSROOT}/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")
set(PKG_CONFIG_EXECUTABLE pkg-config CACHE FILEPATH "pkg-config")
