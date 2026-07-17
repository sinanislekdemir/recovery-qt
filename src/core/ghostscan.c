/*
    
    File: ghostscan.c

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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "common.h"
#include "fnctdsk.h"
#include "analyse.h"
#include "ghostscan.h"

extern const arch_fnct_t arch_none;

#define GHOSTSCAN_BUFFER_SIZE 8192
#define GHOSTSCAN_DEDUP_SECTORS 64

static int matches_known_partition(const list_part_t *known_list, const partition_t *part, unsigned int sector_size) {
  const list_part_t *item;
  uint64_t threshold = (uint64_t)GHOSTSCAN_DEDUP_SECTORS * sector_size;

  for (item = known_list; item != NULL; item = item->next) {
    uint64_t off_diff;
    if (!item->part)
      continue;
    off_diff = (part->part_offset > item->part->part_offset) ? part->part_offset - item->part->part_offset
                                                             : item->part->part_offset - part->part_offset;
    if (off_diff <= threshold)
      return 1;
  }
  return 0;
}

static int is_duplicate(const list_part_t *result_list, uint64_t offset, upart_type_t upart_type,
                        unsigned int sector_size) {
  const list_part_t *item;
  uint64_t threshold = (uint64_t)GHOSTSCAN_DEDUP_SECTORS * sector_size;

  for (item = result_list; item != NULL; item = item->next) {
    if (!item->part)
      continue;
    if (item->part->upart_type == upart_type) {
      uint64_t existing = item->part->part_offset;
      if (offset >= existing - threshold && offset <= existing + threshold)
        return 1;
    }
  }
  return 0;
}

list_part_t *scan_for_ghost_partitions(disk_t *disk, const ghostscan_config_t *cfg) {
  list_part_t *result = NULL;
  unsigned char *buffer;
  unsigned int sector_size;
  uint64_t start_sector, end_sector, total_sectors;
  uint64_t sector;

  if (!disk || !cfg || cfg->stride_sectors == 0)
    return NULL;

  sector_size = disk->sector_size;
  if (sector_size == 0)
    sector_size = DEFAULT_SECTOR_SIZE;

  start_sector = cfg->offset_start / sector_size;
  end_sector = cfg->offset_end / sector_size;
  if (end_sector == 0 || end_sector > disk->disk_size / sector_size)
    end_sector = disk->disk_size / sector_size;
  total_sectors = end_sector - start_sector;

  buffer = (unsigned char *)MALLOC(GHOSTSCAN_BUFFER_SIZE);
  if (!buffer)
    return NULL;

  {
    unsigned int last_pct = 0;

    for (sector = start_sector; sector < end_sector; sector += cfg->stride_sectors) {
      uint64_t offset = sector * sector_size;
      partition_t *part;

      if (disk->pread(disk, buffer, GHOSTSCAN_BUFFER_SIZE, offset) != (int)GHOSTSCAN_BUFFER_SIZE)
        continue;

      part = partition_new(&arch_none);
      if (!part)
        continue;
      part->part_offset = offset;

      if (search_type_0(buffer, disk, part, 0, 0) > 0 || search_type_1(buffer, disk, part, 0, 0) > 0 ||
          search_type_2(buffer, disk, part, 0, 0) > 0 || search_type_8(buffer, disk, part, 0, 0) > 0 ||
          search_type_16(buffer, disk, part, 0, 0) > 0 || search_type_64(buffer, disk, part, 0, 0) > 0 ||
          search_type_128(buffer, disk, part, 0, 0) > 0 || search_type_2048(buffer, disk, part, 0, 0) > 0) {
        if (!is_duplicate(result, part->part_offset, part->upart_type, sector_size) &&
            !(cfg->skip_list && matches_known_partition(cfg->skip_list, part, sector_size))) {
          int insert_error = 0;

          part->status = STATUS_DELETED;
          result = insert_new_partition(result, part, 1, &insert_error);
          if (insert_error)
            free(part);
        } else {
          free(part);
        }
      } else {
        free(part);
      }

      if (cfg->progress_cb) {
        unsigned int pct = (total_sectors > 0) ? (unsigned int)((sector - start_sector) * 100 / total_sectors) : 0;
        if (pct != last_pct) {
          last_pct = pct;
          if (cfg->progress_cb(sector - start_sector, total_sectors, cfg->user_data))
            break;
        }
      }
    }
  }

  free(buffer);
  return result;
}
