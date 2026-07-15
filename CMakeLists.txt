cmake_minimum_required(VERSION 3.16)
project(recovery_qt VERSION 7.3.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 90)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ---------------------------------------------------------------------------
# Generated config.h (provides HAVE_CONFIG_H, TARGET_LINUX, library defines)
# ---------------------------------------------------------------------------
file(WRITE ${CMAKE_BINARY_DIR}/config.h
[=[
#ifndef PHOTOREC_NC_CONFIG_H
#define PHOTOREC_NC_CONFIG_H

#define PACKAGE "recovery-qt"
#define PACKAGE_BUGREPORT "grenier@cgsecurity.org"
#define PACKAGE_NAME "recovery-qt"
#define PACKAGE_STRING "recovery-qt 7.3-WIP"
#define PACKAGE_TARNAME "recovery-qt"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "7.3-WIP"
#define VERSION "7.3-WIP"

#define TARGET_LINUX 1

#define HAVE_ATEXIT 1
#define HAVE_ATOLL 1
#define HAVE_BYTESWAP_H 1
#define HAVE_CHDIR 1
#define HAVE_CHMOD 1
#define HAVE_DIRENT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_DIRNAME 1
#define HAVE_DUP2 1
#define HAVE_ENDIAN_H 1
#define HAVE_ERRNO_H 1
#define HAVE_EXECV 1
#define HAVE_FCNTL_H 1
#define HAVE_FDATASYNC 1
#define HAVE_FEATURES_H 1
#define HAVE_FSEEKO 1
#define HAVE_FSYNC 1
#define HAVE_FTELLO 1
#define HAVE_FTRUNCATE 1
#define HAVE_GETCWD 1
#define HAVE_GETEUID 1
#define HAVE_GETPWUID 1
#define HAVE_GETOPT_LONG 1
#define HAVE_ICONV 1
#define HAVE_GICONV_H 1
#define HAVE_ICONV_H 1
#define HAVE_GLOB_H 1
#define HAVE_LANGINFO_H 1
#define HAVE_LIBGEN_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_LOCALTIME_R 1
#define HAVE_LSTAT 1
#define HAVE_MEMALIGN 1
#define HAVE_MEMCHR 1
#define HAVE_MEMSET 1
#define HAVE_MALLOC_H 1
#define HAVE_MKDIR 1
#define HAVE_MMAP 1
#define HAVE_NANOSLEEP 1
#define HAVE_OFF64_T 1
#define HAVE_PADDR_T 0
#define HAVE_POSIX_FADVISE 1
#define HAVE_POSIX_MEMALIGN 1
#define HAVE_PREAD 1
#define HAVE_PTHREAD 1
#define HAVE_PWD_H 1
#define HAVE_PWRITE 1
#define HAVE_READLINK 1
#define HAVE_REALPATH 1
#define HAVE_RMDIR 1
#define HAVE_SELECT 1
#define HAVE_SETENV 1
#define HAVE_SETJMP_H 1
#define HAVE_SETLOCALE 1
#define HAVE_SIGNAL_H 1
#define HAVE_SIGACTION 1
#define HAVE_SIGNAL 1
#define HAVE_SLEEP 1
#define HAVE_SNPRINTF 1
#define HAVE_STAT 1
#define HAVE_STDARG_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRCASESTR 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRING_H 1
#define HAVE_STRNCASECMP 1
#define HAVE_STRNLEN 1
#define HAVE_STRPTIME 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1
#define HAVE_STRTOUL 1
#define HAVE_STRTOULL 1
#define HAVE_STRUCT_STAT_ST_BLOCKS 1
#define HAVE_STRUCT_STAT_ST_RDEV 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_MOUNT_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_SYSMACROS_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UTSNAME_H 1
#define HAVE_TIME_H 1
#define HAVE_UNAME 1
#define HAVE_UNISTD_H 1
#define HAVE_UTIME 1
#define HAVE_UTIME_H 1
#define HAVE_VSNPRINTF 1
#define HAVE_WCTOMB 1
#define HAVE_ZLIB_H 1

#define ICONV_CONST

#define HAVE_LIBEXT2FS 1
#define HAVE_LIBCOMM_ERR 1
#define HAVE_LIBNTFS3G 1
#define HAVE_LIBUUID 1
#define HAVE_LIBZ 1
#define HAVE_LIBDL 1

#define HAVE_EXT2FS_EXT2FS_H 1
#define HAVE_EXT2FS_EXT2_FS_H 1
#define HAVE_EXT2FS_GET_GENERIC_BITMAP_START 1
#define HAVE_NTFS_LIBNTFS_VERSION 1
#define HAVE_NTFS_VERSION_H 1
#define NTFS_MBSTOUCS_HAVE_TWO_ARGUMENTS 1
#define NTFS_DEVICE_OPERATIONS_IOCTL_INT 1

#define HAVE_LINUX_FS_H 1
#define HAVE_LINUX_HDREG_H 1
#define HAVE_LINUX_TYPES_H 1

#define STDC_HEADERS 1
#define TESTDISKDATE "July 2025"

#endif
]=])

# ---------------------------
# Dependencies
# ---------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(NTFS3G REQUIRED libntfs-3g)
pkg_check_modules(EXT2FS REQUIRED ext2fs)
pkg_check_modules(COM_ERR REQUIRED com_err)
pkg_check_modules(UUID REQUIRED uuid)
pkg_check_modules(ZLIB REQUIRED zlib)

# ---------------------------
# C sources (legacy testdisk)
# ---------------------------
set(SMALLBASE_C
    src/common.c     src/crc.c         src/ext2_common.c
    src/fat_common.c src/log.c         src/misc.c
    src/setdate.c
)

set(BASE_C
    src/apfs_common.c src/autoset.c    src/fnctdsk.c
    src/hdaccess.c    src/hdcache.c    src/hidden.c
    src/hpa_dco.c     src/intrf.c      src/iso.c
    src/list_sort.c   src/log_part.c   src/parti386.c
    src/partgpt.c     src/parthumax.c  src/partmac.c
    src/partnone.c    src/partsun.c    src/partxbox.c
    src/partauto.c    src/ntfs_io.c    src/ntfs_utl.c
    src/unicode.c     src/chgtype.c    src/partgptw.c
    src/ntfs_udl.c    src/next.c       src/suspend_no.c
)

set(FS_C
    src/analyse.c src/apfs.c   src/bfs.c    src/bsd.c
    src/btrfs.c   src/cramfs.c src/exfat.c  src/ext2.c
    src/fat.c     src/fatx.c   src/f2fs.c   src/jfs.c
    src/gfs2.c    src/hfs.c    src/hfsp.c   src/hpfs.c
    src/luks.c    src/lvm.c    src/md.c     src/netware.c
    src/ntfs.c    src/refs.c   src/rfs.c    src/savehdr.c
    src/sun.c     src/swap.c   src/sysv.c   src/ufs.c
    src/vmfs.c    src/wbfs.c   src/xfs.c    src/zfs.c
)

set(PHOTOREC_C
    src/ptree.c     src/pscanner.c   src/prestore.c
    src/luksnc.c    src/pcarver.c    src/pbackup.c
    src/psession.c
    src/dir.c       src/dirn.c       src/fat_dir.c
    src/ntfs_dir.c  src/ext2_dir.c   src/exfat_dir.c
    src/rfs_dir.c   src/geometry.c   src/filegen.c
    src/file_list.c src/utfsize.c
)

file(GLOB FILE_FORMATS_C CONFIGURE_DEPENDS "src/file_*.c")
list(FILTER FILE_FORMATS_C EXCLUDE REGEX "file_found\\.c$")
list(FILTER FILE_FORMATS_C EXCLUDE REGEX "file_awk\\.c$")

set(ALL_C_SOURCES
    ${SMALLBASE_C}
    ${BASE_C}
    ${FS_C}
    ${PHOTOREC_C}
    ${FILE_FORMATS_C}
    src/stubs.c
)

# ---------------------------
# C++ sources (Qt wrappers)
# ---------------------------
set(CPP_SOURCES
    src/main.cpp
    src/progress_cb.c
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
# Photorec_nc executable
# ---------------------------
add_executable(recovery_qt ${ALL_C_SOURCES} ${CPP_SOURCES})
set_target_properties(recovery_qt PROPERTIES OUTPUT_NAME "recovery-qt")

set_source_files_properties(${ALL_C_SOURCES} PROPERTIES LANGUAGE C)

target_include_directories(recovery_qt PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_BINARY_DIR}
)

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
    ${NTFS3G_LIBRARIES}
    ${EXT2FS_LIBRARIES}
    ${COM_ERR_LIBRARIES}
    ${UUID_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

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

# ---------------------------
# Compile options (C vs C++)
# ---------------------------
target_compile_options(recovery_qt PRIVATE
    -include ${CMAKE_BINARY_DIR}/config.h
    $<$<COMPILE_LANGUAGE:C>:-O2>
    $<$<COMPILE_LANGUAGE:C>:-g>
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-O2>
    $<$<COMPILE_LANGUAGE:CXX>:-g>
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
)

# ---------------------------
# Install
# ---------------------------
include(GNUInstallDirs)

install(TARGETS recovery_qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_SOURCE_DIR}/src/resources/recovery-qt.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES ${CMAKE_SOURCE_DIR}/logo.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME recovery-qt.png)

# ---------------------------
# CPack / DEB packaging
# ---------------------------
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "recovery-qt")
set(CPACK_PACKAGE_VERSION "7.3")
set(CPACK_PACKAGE_VENDOR "Sinan Islekdemir")
set(CPACK_PACKAGE_CONTACT "sinan@islekdemir.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Data recovery and file carving tool")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sinan Islekdemir <sinan@islekdemir.com>")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/sinanislekdemir/recovery-qt")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6widgets6 (>= 6.2), libqt6core6 (>= 6.2), libntfs-3g89, libext2fs2, zlib1g, libuuid1, policykit-1")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/COPYING)
set(CPACK_RESOURCE_FILE_README ${CMAKE_SOURCE_DIR}/README.md)
include(CPack)
