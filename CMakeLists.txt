cmake_minimum_required(VERSION 3.16)
project(recovery_qt VERSION 7.3.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------
# Dependencies
# ---------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Iconv REQUIRED)

pkg_check_modules(NTFS3G REQUIRED libntfs-3g)
pkg_check_modules(EXT2FS REQUIRED ext2fs)
pkg_check_modules(COM_ERR REQUIRED com_err)
pkg_check_modules(UUID REQUIRED uuid)

# ---------------------------
# Config header generation
# ---------------------------
string(TIMESTAMP BUILD_DATE "%B %Y")
if(WIN32)
    set(CONFIG_H_TEMPLATE ${CMAKE_SOURCE_DIR}/win/config.h.cmake.in)
else()
    set(CONFIG_H_TEMPLATE ${CMAKE_SOURCE_DIR}/config.h.cmake.in)
endif()
configure_file(
    ${CONFIG_H_TEMPLATE}
    ${CMAKE_BINARY_DIR}/config.h
)

# ---------------------------
# C sources - Core utilities
# ---------------------------
set(CORE_UTIL_C
    src/core/common.c    src/core/crc.c       src/core/log.c
    src/core/misc.c      src/core/setdate.c
)

# ---------------------------
# C sources - Disk I/O and partitioning
# ---------------------------
set(DISK_C
    src/disk/hdaccess.c    src/disk/hdcache.c    src/disk/hidden.c
    src/disk/hpa_dco.c     src/disk/intrf.c      src/disk/iso.c
    src/disk/list_sort.c   src/disk/log_part.c   src/disk/parti386.c
    src/disk/partgpt.c     src/disk/parthumax.c  src/disk/partmac.c
    src/disk/partnone.c    src/disk/partsun.c    src/disk/partxbox.c
    src/disk/partauto.c    src/disk/partgptw.c   src/disk/fnctdsk.c
    src/disk/autoset.c     src/disk/chgtype.c    src/disk/unicode.c
    src/disk/next.c        src/disk/suspend_no.c
)

# ---------------------------
# C sources - Filesystem modules
# ---------------------------
set(FS_C
    src/fs/analyse.c       src/fs/apfs.c          src/fs/apfs_common.c
    src/fs/bfs.c           src/fs/bsd.c           src/fs/btrfs.c
    src/fs/cramfs.c        src/fs/exfat.c         src/fs/ext2.c
    src/fs/ext2_common.c   src/fs/fat.c           src/fs/fat_common.c
    src/fs/fatx.c          src/fs/f2fs.c          src/fs/jfs.c
    src/fs/gfs2.c          src/fs/hfs.c           src/fs/hfsp.c
    src/fs/hpfs.c          src/fs/luks.c          src/fs/lvm.c
    src/fs/md.c            src/fs/netware.c       src/fs/ntfs.c
    src/fs/refs.c          src/fs/rfs.c           src/fs/savehdr.c
    src/fs/sun.c           src/fs/swap.c          src/fs/sysv.c
    src/fs/ufs.c           src/fs/vmfs.c          src/fs/wbfs.c
    src/fs/xfs.c           src/fs/zfs.c
    src/fs/ntfs_io.c       src/fs/ntfs_utl.c      src/fs/ntfs_udl.c
    src/fs/luksdec.c
)

# ---------------------------
# C sources - Directory drivers
# ---------------------------
set(DIR_C
    src/fs/fat_dir.c       src/fs/ntfs_dir.c      src/fs/ext2_dir.c
    src/fs/exfat_dir.c     src/fs/rfs_dir.c       src/fs/iso_dir.c
)

# ---------------------------
# C sources - Recovery engine
# ---------------------------
set(RECOVERY_C
    src/core/tree.c       src/core/scanner.c     src/core/restore.c
    src/core/carver.c     src/core/backup.c      src/core/session.c
    src/core/dir.c         src/core/geometry.c     src/core/file_list.c
    src/core/utfsize.c     src/core/progress.c  src/core/default_location.c
)

# ---------------------------
# C sources - File format registry and carvers
# ---------------------------
set(FORMAT_REG_C
    src/formats/filegen.c  src/formats/file_sig.c
)

file(GLOB FILE_FORMATS_C CONFIGURE_DEPENDS "src/formats/file_*.c")
list(FILTER FILE_FORMATS_C EXCLUDE REGEX "file_found\\.c$")
list(FILTER FILE_FORMATS_C EXCLUDE REGEX "file_awk\\.c$")

# ---------------------------
# Vendored Argon2 (CC0 / Apache-2.0)
# ---------------------------
set(ARGON2_C
    src/vendor/argon2/argon2.c
    src/vendor/argon2/core.c
    src/vendor/argon2/encoding.c
    src/vendor/argon2/ref.c
    src/vendor/argon2/thread.c
    src/vendor/argon2/blake2/blake2b.c
)

# ---------------------------
# Windows-specific sources (win/)
# ---------------------------
set(WIN_C "")
set(WIN_RESOURCES "")
if(WIN32)
    set(WIN_C
        win/src/win32.c
        win/src/hdwin32.c
        win/src/mingw_compat.c
    )
    set(WIN_RESOURCES win/recovery-qt.rc)
endif()

# ---------------------------
# All C sources
# ---------------------------
set(ALL_C_SOURCES
    ${CORE_UTIL_C}
    ${DISK_C}
    ${FS_C}
    ${DIR_C}
    ${RECOVERY_C}
    ${FORMAT_REG_C}
    ${FILE_FORMATS_C}
    ${ARGON2_C}
    ${WIN_C}
)

# ---------------------------
# C++ sources (Qt wrappers and UI)
# ---------------------------
set(CPP_SOURCES
    src/main.cpp
    src/wrappers/disk.cpp
    src/wrappers/partitionlist.cpp
    src/wrappers/signatureregistry.cpp
    src/wrappers/filetreemodel.cpp
    src/wrappers/progresscallback.cpp
    src/wrappers/workerbase.cpp
    src/wrappers/scanner.cpp
    src/wrappers/carver.cpp
    src/wrappers/restorer.cpp
    src/wrappers/simpleworker.cpp
    src/wrappers/luksmanager.cpp
    src/wrappers/sessionmanager.cpp
    src/ui/mainwindow.cpp
    src/ui/diskselectionwidget.cpp
    src/ui/partitionselectionwidget.cpp
    src/ui/browserwidget.cpp
    src/ui/formatselectordialog.cpp
    src/ui/lukspassworddialog.cpp
    src/ui/aboutdialog.cpp
    src/ui/imagepreviewdialog.cpp
    src/ui/progressdialog.cpp
)

# ---------------------------
# Executable
# ---------------------------
add_executable(recovery_qt ${ALL_C_SOURCES} ${CPP_SOURCES} ${WIN_RESOURCES})
set_target_properties(recovery_qt PROPERTIES
    OUTPUT_NAME "recovery-qt"
    AUTOMOC ON
)

if(WIN32)
    # Qt6 defines UNICODE, which redirects CreateFile(A) → CreateFileW among
    # other Win32 APIs.  recovery-qt's win32 C sources pass char* strings and
    # must call the ANSI versions; the Qt C++ wrappers do not need W variants.
    qt_disable_unicode_defines(recovery_qt)
endif()

set_source_files_properties(${ALL_C_SOURCES} PROPERTIES LANGUAGE C)

target_include_directories(recovery_qt PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/fs
    ${CMAKE_SOURCE_DIR}/src/disk
    ${CMAKE_SOURCE_DIR}/src/formats
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/vendor/argon2
    ${CMAKE_BINARY_DIR}
)

if(WIN32)
    target_include_directories(recovery_qt PRIVATE ${CMAKE_SOURCE_DIR}/win/src)
endif()

target_include_directories(recovery_qt SYSTEM PRIVATE
    ${NTFS3G_INCLUDE_DIRS}
    ${EXT2FS_INCLUDE_DIRS}
    ${COM_ERR_INCLUDE_DIRS}
    ${UUID_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

target_link_libraries(recovery_qt PRIVATE
    Qt6::Core
    Qt6::Widgets
    Threads::Threads
    ZLIB::ZLIB
    ${NTFS3G_LINK_LIBRARIES}
    ${EXT2FS_LINK_LIBRARIES}
    ${COM_ERR_LINK_LIBRARIES}
    ${UUID_LINK_LIBRARIES}
    OpenSSL::Crypto
    Iconv::Iconv
    ${CMAKE_DL_LIBS}
)

if(WIN32)
    set_target_properties(recovery_qt PROPERTIES WIN32_EXECUTABLE ON)
    target_link_options(recovery_qt PRIVATE -static -static-libgcc -static-libstdc++ -s)
    target_link_libraries(recovery_qt PRIVATE ws2_32 crypt32)
endif()

# ---------------------------
# Compile definitions
# ---------------------------
string(TIMESTAMP BUILD_UTC "%Y-%m-%d %H:%M:%S UTC" UTC)
target_compile_definitions(recovery_qt PRIVATE
    HAVE_CONFIG_H
    TESTDISK_LSB
    BUILD_UTC="${BUILD_UTC}"
    _GNU_SOURCE
    _LARGEFILE_SOURCE
    _LARGEFILE64_SOURCE
    _FILE_OFFSET_BITS=64
)

if(WIN32)
    target_compile_definitions(recovery_qt PRIVATE __USE_MINGW_ANSI_STDIO=1)

    # Headless console harness exercising the C core (scanner, restore,
    # preview capture, LUKS decrypt) without the GUI. Runs under wine.
    add_executable(core_test EXCLUDE_FROM_ALL ${ALL_C_SOURCES} win/src/core_test_win.c)
    set_target_properties(core_test PROPERTIES OUTPUT_NAME "core-test")
    set_source_files_properties(win/src/core_test_win.c PROPERTIES LANGUAGE C)
    target_include_directories(core_test PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/fs
        ${CMAKE_SOURCE_DIR}/src/disk
        ${CMAKE_SOURCE_DIR}/src/formats
        ${CMAKE_SOURCE_DIR}/src/common
        ${CMAKE_SOURCE_DIR}/src/vendor/argon2
        ${CMAKE_SOURCE_DIR}/win/src
        ${CMAKE_BINARY_DIR}
    )
    target_include_directories(core_test SYSTEM PRIVATE
        ${NTFS3G_INCLUDE_DIRS}
        ${EXT2FS_INCLUDE_DIRS}
        ${COM_ERR_INCLUDE_DIRS}
        ${UUID_INCLUDE_DIRS}
        ${ZLIB_INCLUDE_DIRS}
    )
    target_link_libraries(core_test PRIVATE
        Threads::Threads
        ZLIB::ZLIB
        ${NTFS3G_LINK_LIBRARIES}
        ${EXT2FS_LINK_LIBRARIES}
        ${COM_ERR_LINK_LIBRARIES}
        ${UUID_LINK_LIBRARIES}
        OpenSSL::Crypto
        Iconv::Iconv
        ws2_32
        crypt32
    )
    target_compile_definitions(core_test PRIVATE
        HAVE_CONFIG_H
        TESTDISK_LSB
        BUILD_UTC="${BUILD_UTC}"
        _GNU_SOURCE
        _LARGEFILE_SOURCE
        _LARGEFILE64_SOURCE
        _FILE_OFFSET_BITS=64
        __USE_MINGW_ANSI_STDIO=1
    )
    target_compile_options(core_test PRIVATE
        -include ${CMAKE_BINARY_DIR}/config.h
        -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-address-of-packed-member
    )
    target_link_options(core_test PRIVATE -static -static-libgcc)
endif()

# ---------------------------
# Compile options
# ---------------------------
# -Wno-unused-parameter: the photorec file-format plugins and disk/fs
#   callbacks all share fixed function signatures, so unused parameters are
#   architectural, not a code smell.
# -Wno-address-of-packed-member: FAT/NTFS parsers read on-disk packed
#   structures; unaligned access is well-defined on the Linux/x86_64 target.
# Everything else stays strict so genuinely new warnings surface as smells.
target_compile_options(recovery_qt PRIVATE
    -include ${CMAKE_BINARY_DIR}/config.h
    -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-address-of-packed-member
)

# Vendored third-party code (argon2) is kept byte-identical to upstream;
# do not lint it.
set_source_files_properties(${ARGON2_C} PROPERTIES COMPILE_OPTIONS "-w")

if(WIN32)
    # Static Qt6Core bundles its own blake2 with the same public symbol names;
    # rename the vendored argon2 copies to avoid duplicate symbols.
    set_property(SOURCE ${ARGON2_C} APPEND PROPERTY COMPILE_DEFINITIONS
        blake2b_init_param=argon2_blake2b_init_param
        blake2b_init=argon2_blake2b_init
        blake2b_init_key=argon2_blake2b_init_key
        blake2b_update=argon2_blake2b_update
        blake2b_final=argon2_blake2b_final
        blake2b=argon2_blake2b
    )
endif()

# ---------------------------
# Install
# ---------------------------
include(GNUInstallDirs)

install(TARGETS recovery_qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(NOT WIN32)
    install(FILES ${CMAKE_SOURCE_DIR}/src/resources/recovery-qt.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/logo.png
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
            RENAME recovery-qt.png)
    install(FILES README.md COPYING
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
            OPTIONAL)

    # ---------------------------
    # CPack / DEB + RPM packaging
    # ---------------------------
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "recovery-qt")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_VENDOR "Sinan Islekdemir")
    set(CPACK_PACKAGE_CONTACT "sinan@islekdemir.com")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Data recovery and file carving tool")
    set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/COPYING)
    set(CPACK_RESOURCE_FILE_README ${CMAKE_SOURCE_DIR}/README.md)

    # DEB
    set(CPACK_DEBIAN_PACKAGE_NAME "recovery-qt")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sinan Islekdemir <sinan@islekdemir.com>")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/sinanislekdemir/recovery-qt")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6 (>= 6.2), libqt6core6 (>= 6.2), libntfs-3g89, libext2fs2, zlib1g, libuuid1, policykit-1")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # RPM
    set(CPACK_RPM_PACKAGE_NAME "recovery-qt")
    set(CPACK_RPM_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_RPM_PACKAGE_VENDOR "Sinan Islekdemir")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0-or-later")
    set(CPACK_RPM_PACKAGE_SUMMARY "Data recovery and file carving tool")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/sinanislekdemir/recovery-qt")
    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_REQUIRES
        "qt6-qtbase-gui >= 6.2, qt6-qtbase >= 6.2, ntfs-3g, e2fsprogs-libs, zlib, libuuid, polkit")

    include(CPack)
endif()
