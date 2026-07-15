/*

    File: fat.h

    Copyright (C) 1998-2004,2007-2008 Christophe GRENIER <grenier@cgsecurity.org>
  
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

#ifndef _FAT_H
#define _FAT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "fat_common.h"

int comp_FAT(disk_t *disk, const partition_t *partition, const unsigned long int fat_size, const unsigned long int sect_res);


int log_fat2_info(const struct fat_boot_sector*fh1, const struct fat_boot_sector*fh2, const upart_type_t upart_type, const unsigned int sector_size);


unsigned int get_next_cluster(disk_t *disk, const partition_t *partition, const upart_type_t upart_type, const int offset, const unsigned int cluster);


int set_next_cluster(disk_t *disk, const partition_t *partition, const upart_type_t upart_type, const int offset, const unsigned int cluster, const unsigned int next_cluster);


int is_fat(const partition_t *partition);


int is_part_fat(const partition_t *partition);


int is_part_fat12(const partition_t *partition);


int is_part_fat16(const partition_t *partition);


int is_part_fat32(const partition_t *partition);


unsigned int fat32_get_prev_cluster(disk_t *disk, const partition_t *partition, const unsigned int fat_offset, const unsigned int cluster, const unsigned int no_of_cluster);


int fat32_free_info(disk_t *disk, const partition_t *partition, const unsigned int fat_offset, const unsigned int no_of_cluster, unsigned int *next_free, unsigned int *free_count);


unsigned long int fat32_get_free_count(const unsigned char *boot_fat32, const unsigned int sector_size);


unsigned long int fat32_get_next_free(const unsigned char *boot_fat32, const unsigned int sector_size);


int recover_FAT(disk_t *disk, const struct fat_boot_sector*fat_header, partition_t *partition, const int verbose, const int dump_ind, const int backup);


int check_FAT(disk_t *disk, partition_t *partition, const int verbose);


int test_FAT(disk_t *disk, const struct fat_boot_sector *fat_header, const partition_t *partition, const int verbose, const int dump_ind);


int recover_OS2MB(const disk_t *disk, const struct fat_boot_sector*fat_header, partition_t *partition, const int verbose, const int dump_ind);


int check_OS2MB(disk_t *disk, partition_t *partition, const int verbose);


int check_VFAT_volume_name(const char *name, const unsigned int max_size);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif
