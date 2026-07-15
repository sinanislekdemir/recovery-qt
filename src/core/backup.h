/*
    File: backup.h

    Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>

    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
#ifndef BACKUP_H
#define BACKUP_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "recovery.h"

#define PBACKUP_MAGIC 0x44534B42  /* "DSKB" little-endian */
#define PBACKUP_VERSION 1
#define PBACKUP_HEADER_SIZE 72
#define PBACKUP_FILE_FLAG_DIR      0x01
#define PBACKUP_FILE_FLAG_NO_CLUST 0x02
#define PBACKUP_MAX_PATH 4096

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    int32_t  fs_type;
    uint64_t disk_size;
    uint32_t sector_size;
    uint64_t part_offset;
    uint64_t part_size;
    uint32_t cluster_bytes;
    uint64_t data_offset;
    uint64_t created;
    uint32_t file_count;
    uint16_t model_len;
    uint16_t reserved2;
} __attribute__ ((gcc_struct, __packed__)) pbackup_header_t;

int backup_create(disk_t *disk, const partition_t *partition,
    const char *dest_dir);
int backup_restore(scan_tree_t *tree, disk_t *disk,
    const partition_t *partition, const char *path);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif
