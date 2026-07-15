/*

    File: recovery.h

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

#ifndef RECOVERY_H
#define RECOVERY_H
#ifdef __cplusplus
extern "C" {
#endif
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include "list.h"

typedef struct param_disk_struct disk_t;
typedef struct partition_struct partition_t;

typedef enum { NODE_FILE = 0, NODE_DIR = 1 } node_type_t;

typedef struct file_node {
    struct td_list_head siblings;
    struct td_list_head children;
    struct file_node *parent;
    char *name;
    uint64_t size;
    uint64_t first_sector;
    uint64_t num_sectors;
    time_t mtime;
    unsigned int sector_size;
    unsigned int type : 1;
    unsigned int marked : 1;
    unsigned int deleted : 1;
    unsigned int expanded : 1;
    unsigned int orphan : 1;
    unsigned int backup_restored : 1;
    unsigned int backup_modified : 1;
    uint64_t *cluster_list;
    uint32_t  cluster_count;
    uint32_t  cluster_size;
} file_node_t;

static_assert(sizeof(file_node_t) >= 64, "file_node_t too small");
static_assert(NODE_FILE == 0 && NODE_DIR == 1, "node_type_t values");

typedef struct {
    file_node_t *root;
    uint64_t total_files;
    uint64_t total_dirs;
    uint64_t total_size;
} scan_tree_t;

scan_tree_t *tree_new(void);
file_node_t *tree_add_path(scan_tree_t *tree, const char *path, bool is_dir,
    uint64_t size, uint64_t first_sector, uint64_t num_sectors,
    time_t mtime, unsigned int sector_size, bool deleted);
file_node_t *tree_find_path(scan_tree_t *tree, const char *path);
char *tree_get_path(const file_node_t *node, const file_node_t *root,
    char *buf, size_t bufsize);
void tree_free(scan_tree_t *tree);
uint64_t tree_count_marked(const file_node_t *dir, uint64_t *size_out);
void tree_count_changes(const file_node_t *dir, uint64_t *del_out,
    uint64_t *mod_out, uint64_t *size_out);

int scanner_run(scan_tree_t *tree, disk_t *disk, const partition_t *partition, bool deep);

const char *tree_format_size(uint64_t bytes, char *buf, size_t bufsize);

int carver_run(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
    const char *ext_filter, bool deep_scan);

int restore_files(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
    const char *dest_dir);

int restore_file_node(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
    const char *dest_dir, file_node_t *node);

unsigned char *read_file_bytes(scan_tree_t *tree, disk_t *disk,
    const partition_t *partition, file_node_t *node, size_t *out_size);

#ifdef __cplusplus
}
#endif
#endif
