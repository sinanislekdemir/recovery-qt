# File: Dockerfile
#
# Dedicated cross-compile image for building the Windows (x86_64) version of
# recovery-qt. All MinGW-w64 toolchain bits and statically built dependencies
# live inside this image so the host system stays clean.
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        pkg-config \
        autoconf \
        automake \
        libtool \
        gettext \
        gcc-mingw-w64-x86-64-posix \
        g++-mingw-w64-x86-64-posix \
        binutils-mingw-w64-x86-64 \
        perl \
        python3 \
        ca-certificates \
        curl \
        xz-utils \
        git \
    && rm -rf /var/lib/apt/lists/*

ENV WIN_SYSROOT=/opt/win-sysroot \
    QT_HOST=/opt/qt-host \
    TRIPLET=x86_64-w64-mingw32 \
    CROSS_CC=x86_64-w64-mingw32-gcc-posix \
    CROSS_CXX=x86_64-w64-mingw32-g++-posix

COPY toolchain.cmake /opt/toolchain.cmake

# ---------------------------------------------------------------------------
# zlib (static)
# ---------------------------------------------------------------------------
ARG ZLIB_VERSION=1.3.1
RUN mkdir -p /build && cd /build \
    && curl -fsSLO https://zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz \
    && tar xf zlib-${ZLIB_VERSION}.tar.gz \
    && cd zlib-${ZLIB_VERSION} \
    && make -f win32/Makefile.gcc -j"$(nproc)" \
        CC=${CROSS_CC} AR=${TRIPLET}-ar RC=${TRIPLET}-windres \
    && make -f win32/Makefile.gcc install \
        INCLUDE_PATH=${WIN_SYSROOT}/include \
        LIBRARY_PATH=${WIN_SYSROOT}/lib \
        BINARY_PATH=${WIN_SYSROOT}/bin \
        SHARED_MODE=0 \
    && rm -rf /build

# ---------------------------------------------------------------------------
# win-iconv (static iconv implementation for Windows)
# ---------------------------------------------------------------------------
ARG WINICONV_VERSION=0.0.8
RUN mkdir -p /build && cd /build \
    && curl -fsSL -o win-iconv.tar.gz \
        https://github.com/win-iconv/win-iconv/archive/refs/tags/v${WINICONV_VERSION}.tar.gz \
    && tar xf win-iconv.tar.gz \
    && cmake -S win-iconv-${WINICONV_VERSION} -B win-iconv-build -GNinja \
        -DCMAKE_TOOLCHAIN_FILE=/opt/toolchain.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=${WIN_SYSROOT} \
        -DBUILD_STATIC=ON -DBUILD_SHARED=OFF -DBUILD_EXECUTABLE=OFF -DBUILD_TEST=OFF \
    && cmake --build win-iconv-build \
    && cmake --install win-iconv-build \
    && rm -rf /build

# ---------------------------------------------------------------------------
# OpenSSL (static libcrypto)
# ---------------------------------------------------------------------------
ARG OPENSSL_VERSION=3.3.2
RUN mkdir -p /build && cd /build \
    && curl -fsSLO https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz \
    && tar xf openssl-${OPENSSL_VERSION}.tar.gz \
    && cd openssl-${OPENSSL_VERSION} \
    && ./Configure mingw64 no-shared no-tests no-apps no-docs \
        --prefix=${WIN_SYSROOT} --libdir=lib \
        --cross-compile-prefix=${TRIPLET}- \
    && make -j"$(nproc)" build_libs \
    && make install_dev \
    && rm -rf /build

# ---------------------------------------------------------------------------
# e2fsprogs (static libext2fs, libcom_err, libuuid)
# ---------------------------------------------------------------------------
ARG E2FSPROGS_VERSION=1.47.1
RUN mkdir -p /build && cd /build \
    && curl -fsSLO https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v${E2FSPROGS_VERSION}/e2fsprogs-${E2FSPROGS_VERSION}.tar.xz \
    && tar xf e2fsprogs-${E2FSPROGS_VERSION}.tar.xz \
    && cd e2fsprogs-${E2FSPROGS_VERSION} \
    && ./configure --host=${TRIPLET} --prefix=${WIN_SYSROOT} \
        CC=${CROSS_CC} \
        --disable-nls --disable-rpath --disable-fsck --disable-defrag \
        --disable-resizer --disable-imager --disable-debugfs \
        --disable-e2initrd-helper --disable-fuse2fs --disable-mmp \
        --disable-tdb --disable-testio-debug --enable-libuuid \
    && make -C util subst \
    && make -j"$(nproc)" -C lib/et all && make -C lib/et install \
    && make -j"$(nproc)" -C lib/uuid all && make -C lib/uuid install \
    && make -j"$(nproc)" -C lib/ext2fs all && make -C lib/ext2fs install \
    && rm -rf /build

# ---------------------------------------------------------------------------
# Qt 6 host tools (moc, rcc, uic for the build machine)
# ---------------------------------------------------------------------------
ARG QT_VERSION=6.7.3
RUN mkdir -p /build && cd /build \
    && curl -fsSLO https://download.qt.io/archive/qt/6.7/${QT_VERSION}/submodules/qtbase-everywhere-src-${QT_VERSION}.tar.xz \
    && tar xf qtbase-everywhere-src-${QT_VERSION}.tar.xz \
    && cmake -S qtbase-everywhere-src-${QT_VERSION} -B qt-host-build -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=${QT_HOST} \
        -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
        -DFEATURE_gui=OFF -DFEATURE_sql=OFF -DFEATURE_network=OFF \
        -DFEATURE_dbus=OFF -DFEATURE_icu=OFF \
    && cmake --build qt-host-build --parallel \
    && cmake --install qt-host-build \
    && rm -rf /build

# ---------------------------------------------------------------------------
# Qt 6 for Windows (static)
# ---------------------------------------------------------------------------
RUN mkdir -p /build && cd /build \
    && curl -fsSLO https://download.qt.io/archive/qt/6.7/${QT_VERSION}/submodules/qtbase-everywhere-src-${QT_VERSION}.tar.xz \
    && tar xf qtbase-everywhere-src-${QT_VERSION}.tar.xz \
    && cmake -S qtbase-everywhere-src-${QT_VERSION} -B qt-cross-build -GNinja \
        -DCMAKE_TOOLCHAIN_FILE=/opt/toolchain.cmake \
        -DQT_HOST_PATH=${QT_HOST} \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_INSTALL_PREFIX=${WIN_SYSROOT} \
        -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
        -DFEATURE_sql=OFF -DFEATURE_network=OFF -DFEATURE_dbus=OFF \
               -DFEATURE_icu=OFF \
        -DFEATURE_system_zlib=OFF -DFEATURE_openssl=OFF \
    && cmake --build qt-cross-build --parallel \
    && cmake --install qt-cross-build \
    && rm -rf /build

# ---------------------------------------------------------------------------
# ntfs-3g (static libntfs-3g, read-only usage through custom device ops)
#
# MinGW-w64 has no pwd.h/grp.h/syslog.h and no working transparent_union on
# the ms_abi, so a few POSIX stub headers (win/stubs) and small seds are
# needed. The library's own device I/O layer is disabled: recovery-qt drives
# it exclusively through custom ntfs_device_operations (src/fs/ntfs_io.c).
# ---------------------------------------------------------------------------
ARG NTFS3G_VERSION=2022.10.3
COPY stubs/pwd.h stubs/grp.h stubs/syslog.h /opt/ntfs-stubs/
RUN mkdir -p /build && cd /build \
    && curl -fsSL -o ntfs-3g.tgz \
        https://download.tuxera.com/opensource/ntfs-3g_ntfsprogs-${NTFS3G_VERSION}.tgz \
    && tar xf ntfs-3g.tgz \
    && cd ntfs-3g_ntfsprogs-${NTFS3G_VERSION} \
    && sed -i 's/#if !defined(st_mtime) & !defined(__timespec_defined)/#if !defined(st_mtime) \&\& !defined(__timespec_defined) \&\& !defined(_TIMESPEC_DEFINED)/' \
        include/ntfs-3g/ntfstime.h \
    && sed -i 's/, index_union iu,/, void *iu_raw,/' libntfs-3g/dir.c \
    && sed -i 's/iu\.ia/((INDEX_ALLOCATION *)iu_raw)/g' libntfs-3g/dir.c \
    && sed -i 's/iu\.ir/((INDEX_ROOT *)iu_raw)/g' libntfs-3g/dir.c \
    && sed -i '/^#define __attribute__(X)\s*\/\*nothing\*\//d' include/ntfs-3g/compat.h \
    && sed -i '/^#define __inline__\s*inline/d' include/ntfs-3g/compat.h \
    && ./configure --host=${TRIPLET} --prefix=${WIN_SYSROOT} \
        CC=${CROSS_CC} \
        CFLAGS="-g -O2 -D__CRT__NO_INLINE -Duid_t=int -Dgid_t=int -DS_ISVTX=01000 -DS_ISGID=02000 -DS_ISUID=04000 -DS_IFLNK=0120000 -DS_IFSOCK=0140000 -I/opt/ntfs-stubs" \
        --disable-shared --enable-static \
        --disable-ntfsprogs --disable-ntfs-3g --disable-plugins \
        --disable-crypto --without-uuid --disable-device-default-io-ops \
    && make -j"$(nproc)" -C libntfs-3g \
    && make -C libntfs-3g install rootlibdir='$(libdir)' \
    && make -C include install \
    && find . -name 'libntfs-3g.pc' -exec cp {} ${WIN_SYSROOT}/lib/pkgconfig/ \; \
    && test -f ${WIN_SYSROOT}/lib/pkgconfig/libntfs-3g.pc \
    && rm -rf /build

WORKDIR /src
CMD ["win/docker-build.sh"]
