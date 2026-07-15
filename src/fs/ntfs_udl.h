/*

    File: ntfs_udl.h

    Copyright (C) 2007 Christophe GRENIER <grenier@cgsecurity.org>
  
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
#ifndef _NTFS_UDL_H
#define _NTFS_UDL_H
#ifdef __cplusplus
extern "C" {
#endif
#include "dir_common.h"
#include "recovery.h"


int ntfs_undelete_part(disk_t *disk_car, const partition_t *partition, const int verbose, char **current_cmd);

#if defined(HAVE_LIBNTFS) || defined(HAVE_LIBNTFS3G)
#if defined(HAVE_LIBNTFS)
#include <ntfs/volume.h>
#elif defined(HAVE_LIBNTFS3G)
#include <ntfs-3g/volume.h>
#endif
#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif
#include "ntfs_inc.h"
void scan_disk(ntfs_volume *vol, file_info_t *dir_list);
void ntfs_fill_clusters(file_node_t *node, ntfs_volume *vol, uint64_t inode);
typedef void (*scan_progress_cb)(const char *msg, uint64_t current, uint64_t total, uint64_t found);
void scanner_deep_ntfs(scan_tree_t *tree, disk_t *disk,
		const partition_t *partition, ntfs_volume *vol,
		scan_progress_cb progress_cb);
#endif

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif
