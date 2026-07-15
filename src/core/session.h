/*
    File: session.h

    Copyright (C) 2025 Sinan Islekdemir <sinan@islekdemir.com>

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
#ifndef SESSION_H
#define SESSION_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "recovery.h"
#include "common.h"

#define SESSION_MAGIC      0x53455352  /* "RSES" little-endian */
#define SESSION_VERSION    1
#define SESSION_HEADER_SIZE 116

#define SESSION_FLAG_COMPLETED      0x01
#define SESSION_FLAG_CANCELLED      0x02
#define SESSION_FLAG_ENCRYPTED      0x04
#define SESSION_FLAG_LUKS_DECRYPTED 0x08

#define SESSION_OP_DEEP_SCAN  1
#define SESSION_OP_CARVE      2

#define SESSION_PHASE_DIR_WALK   1
#define SESSION_PHASE_MFT_SCAN   2
#define SESSION_PHASE_EXT_INODE  3
#define SESSION_PHASE_FAT_DEEP   4
#define SESSION_PHASE_INDX_DEEP  5
#define SESSION_PHASE_CARVER     6

typedef struct {
  uint32_t version;
  uint32_t flags;
  uint32_t op_type;
  uint64_t timestamp;
  uint64_t progress1;
  uint64_t progress2;
  int      resume_phase;
  uint64_t resume_offset;
  unsigned int sector_size;
  uint64_t disk_size;
  uint64_t part_offset;
  uint64_t part_size;
  uint32_t upart_type;
  uint32_t part_type_i386;
  uint32_t encrypted;
  uint64_t luks_offset;
  char    *device_path;
  char    *model;
  char    *ext_filter;
  scan_tree_t *tree;
} session_info_t;

int session_save(const char *filepath, const scan_tree_t *tree,
    const disk_t *disk, const partition_t *partition,
    int op_type, const char *ext_filter,
    uint64_t progress1, uint64_t progress2,
    int resume_phase, uint64_t resume_offset,
    int flags);

session_info_t *session_load(const char *filepath);

void session_free(session_info_t *info);

typedef struct {
  const char *filepath;
  scan_tree_t *tree;
  disk_t *disk;
  const partition_t *partition;
  int op_type;
  const char *ext_filter;
  int resume_phase;
  uint64_t resume_offset;
  time_t last_save_time;
} session_save_ctx_t;

extern session_save_ctx_t *g_active_save_ctx;

int session_save_checkpoint(uint64_t progress1, uint64_t progress2);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif /* SESSION_H */
