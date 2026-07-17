/*
    
    File: ghostscan.h

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
#ifndef _GHOSTSCAN_H
#define _GHOSTSCAN_H
#ifdef __cplusplus
extern "C" {
#endif
#include "common.h"

#define GHOSTSCAN_STRIDE_QUICK 2048
#define GHOSTSCAN_STRIDE_THOROUGH 8
#define GHOSTSCAN_STRIDE_FORENSIC 1

typedef int (*ghostscan_progress_fn)(uint64_t current_sector, uint64_t total_sectors, void *user_data);

typedef struct {
  uint64_t stride_sectors;
  uint64_t offset_start;
  uint64_t offset_end;
  const list_part_t *skip_list;
  ghostscan_progress_fn progress_cb;
  void *user_data;
} ghostscan_config_t;

list_part_t *scan_for_ghost_partitions(disk_t *disk, const ghostscan_config_t *cfg);

#ifdef __cplusplus
}
#endif
#endif
