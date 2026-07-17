# File: Dockerfile
#
# Dedicated build image for creating the portable Linux tarball, DEB, and RPM
# packages of recovery-qt. Builds on Ubuntu 22.04 for glibc 2.35 compatibility.
# The tarball bundles all non-system libraries via linuxdeploy; DEB and RPM
# are produced by CPack from the same build.
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        qt6-base-dev \
        qt6-base-dev-tools \
        libgl1-mesa-dev \
        libegl1-mesa-dev \
        ntfs-3g-dev \
        libext2fs-dev \
        comerr-dev \
        zlib1g-dev \
        uuid-dev \
        libssl-dev \
        ca-certificates \
        curl \
        file \
        xz-utils \
        patchelf \
        rpm \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# linuxdeploy (self-extracting AppImage — APPIMAGE_EXTRACT_AND_RUN=1)
# ---------------------------------------------------------------------------
ARG LINUXDEPLOY_VER=1-alpha-20240109-1
RUN curl -fsSL \
        -o /usr/local/bin/linuxdeploy \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VER}/linuxdeploy-x86_64.AppImage" \
    && chmod +x /usr/local/bin/linuxdeploy

# ---------------------------------------------------------------------------
# linuxdeploy-plugin-qt (bundles Qt6 libraries, plugins, qt.conf, rpath)
# ---------------------------------------------------------------------------
RUN curl -fsSL \
        -o /usr/local/bin/linuxdeploy-plugin-qt \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
    && chmod +x /usr/local/bin/linuxdeploy-plugin-qt

ENV APPIMAGE_EXTRACT_AND_RUN=1

COPY docker-build.sh /build/docker-build.sh
RUN chmod +x /build/docker-build.sh

COPY docker-build-packages.sh /build/docker-build-packages.sh
RUN chmod +x /build/docker-build-packages.sh

WORKDIR /src
CMD ["/build/docker-build.sh"]
