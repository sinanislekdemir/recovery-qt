/*

    File: apfs.h

    Copyright (C) 2021 Christophe GRENIER <grenier@cgsecurity.org>
    Modified 2026 by Sinan Islekdemir <sinan@islekdemir.com>

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
#ifndef _APFS_H
#define _APFS_H
#include "apfs_common.h"
#ifdef __cplusplus
extern "C" {
#endif

int check_APFS(disk_t *disk_car, partition_t *partition);

int recover_APFS(const disk_t *disk_car, const nx_superblock_t *sb, partition_t *partition, const int verbose,
                 const int dump_ind);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif
