/*

    File: iso_dir.h

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
#ifndef _ISO_DIR_H
#define _ISO_DIR_H
#ifdef __cplusplus
extern "C" {
#endif

struct iso_dir_struct
{
  unsigned int block_size;
  unsigned int joliet;
  uint32_t root_extent;
  uint32_t root_size;
};

dir_partition_t dir_partition_iso_init(disk_t *disk, const partition_t *partition, dir_data_t *dir_data, const int verbose);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif
