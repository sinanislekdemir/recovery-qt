/*

    File: tree.c

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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "common.h"
#include "recovery.h"

static file_node_t *node_alloc(const char *name, bool is_dir, uint64_t size, uint64_t first_sector,
                               uint64_t num_sectors, time_t mtime, unsigned int sector_size, bool deleted) {
  file_node_t *node = (file_node_t *)MALLOC(sizeof(file_node_t));
  TD_INIT_LIST_HEAD(&node->siblings);
  TD_INIT_LIST_HEAD(&node->children);
  node->parent = NULL;
  node->name = strdup(name);
  node->size = size;
  node->first_sector = first_sector;
  node->num_sectors = num_sectors;
  node->mtime = mtime;
  node->sector_size = sector_size;
  node->type = is_dir ? NODE_DIR : NODE_FILE;
  node->marked = 0;
  node->deleted = deleted;
  node->expanded = 0;
  return node;
}

static void node_free(file_node_t *node) {
  struct td_list_head *pos, *tmp;
  td_list_for_each_safe(pos, tmp, &node->children) {
    file_node_t *child = td_list_entry(pos, file_node_t, siblings);
    td_list_del(&child->siblings);
    node_free(child);
  }
  if (node->cluster_list)
    free(node->cluster_list);
  free(node->name);
  free(node);
}

scan_tree_t *tree_new(void) {
  scan_tree_t *tree = (scan_tree_t *)MALLOC(sizeof(scan_tree_t));
  tree->root = node_alloc("/", true, 0, 0, 0, 0, 0, 0);
  tree->total_files = 0;
  tree->total_dirs = 0;
  tree->total_size = 0;
  return tree;
}

static file_node_t *tree_find_child(file_node_t *parent, const char *name) {
  struct td_list_head *pos;
  td_list_for_each(pos, &parent->children) {
    file_node_t *child = td_list_entry(pos, file_node_t, siblings);
    if (strcmp(child->name, name) == 0)
      return child;
  }
  return NULL;
}

static file_node_t *node_ensure_dir(file_node_t *parent, const char *name) {
  file_node_t *existing = tree_find_child(parent, name);
  if (existing)
    return existing;
  file_node_t *dir = node_alloc(name, true, 0, 0, 0, 0, 0, 0);
  dir->parent = parent;
  td_list_add_tail(&dir->siblings, &parent->children);
  return dir;
}

file_node_t *tree_add_path(scan_tree_t *tree, const char *path, bool is_dir, uint64_t size, uint64_t first_sector,
                           uint64_t num_sectors, time_t mtime, unsigned int sector_size, bool deleted) {
  char *saveptr;
  char *last_name = NULL;
  char *path_copy = strdup(path);
  file_node_t *current = tree->root;
  char *token = strtok_r(path_copy, "/", &saveptr);

  while (token != NULL) {
    char *next = strtok_r(NULL, "/", &saveptr);
    if (next == NULL) {
      last_name = token;
      break;
    }
    current = node_ensure_dir(current, token);
    token = next;
  }

  if (last_name == NULL) {
    free(path_copy);
    return tree->root;
  }

  {
    file_node_t *existing = tree_find_child(current, last_name);

    if (existing) {
      if (!existing->deleted && deleted) {
        existing->deleted = 1;
        if (existing->type != NODE_DIR) {
          existing->size = size;
          existing->first_sector = first_sector;
          existing->num_sectors = num_sectors;
          existing->mtime = mtime;
          existing->sector_size = sector_size;
        }
      }
      free(path_copy);
      return existing;
    }

    file_node_t *node = node_alloc(last_name, is_dir, size, first_sector, num_sectors, mtime, sector_size, deleted);
    node->parent = current;
    td_list_add_tail(&node->siblings, &current->children);

    if (!is_dir) {
      tree->total_files++;
      tree->total_size += size;
    } else {
      tree->total_dirs++;
    }

    free(path_copy);
    return node;
  }
}

file_node_t *tree_find_path(scan_tree_t *tree, const char *path) {
  if (strcmp(path, "/") == 0)
    return tree->root;
  char *saveptr;
  char *path_copy = strdup(path);
  file_node_t *current = tree->root;
  char *token = strtok_r(path_copy, "/", &saveptr);
  while (token != NULL) {
    current = tree_find_child(current, token);
    if (current == NULL)
      break;
    token = strtok_r(NULL, "/", &saveptr);
  }
  free(path_copy);
  return current;
}

char *tree_get_path(const file_node_t *node, const file_node_t *root, char *buf, size_t bufsize) {
  if (node == NULL || buf == NULL || bufsize == 0)
    return buf;

  buf[0] = '\0';

  const file_node_t *chain[128];
  int count = 0;
  const file_node_t *p = node;
  while (p && p != root) {
    if (count >= 128)
      break;
    chain[count++] = p;
    p = p->parent;
  }

  size_t pos = 0;
  for (int i = count - 1; i >= 0; i--) {
    size_t rem = bufsize - pos;
    if (rem <= 1)
      break;
    int written = snprintf(buf + pos, rem, "/%s", chain[i]->name);
    if (written < 0 || (size_t)written >= rem)
      break;
    pos += (size_t)written;
  }

  return buf;
}

void tree_free(scan_tree_t *tree) {
  if (tree->root)
    node_free(tree->root);
  free(tree);
}

void tree_count_changes(const file_node_t *dir, uint64_t *del_out, uint64_t *mod_out, uint64_t *size_out) {
  uint64_t del = 0, mod = 0, sz = 0;
  struct td_list_head *pos;
  td_list_for_each(pos, &dir->children) {
    const file_node_t *child = td_list_entry(pos, file_node_t, siblings);
    if (child->type == NODE_DIR) {
      uint64_t sub_del = 0, sub_mod = 0, sub_sz = 0;
      tree_count_changes(child, &sub_del, &sub_mod, &sub_sz);
      del += sub_del;
      mod += sub_mod;
      sz += sub_sz;
    } else if (child->deleted) {
      del++;
      sz += child->size;
    } else if (child->backup_modified) {
      mod++;
      sz += child->size;
    }
  }
  *del_out = del;
  *mod_out = mod;
  *size_out = sz;
}

uint64_t tree_count_marked(const file_node_t *dir, uint64_t *size_out) {
  uint64_t count = 0, total_size = 0;
  struct td_list_head *pos;
  td_list_for_each(pos, &dir->children) {
    const file_node_t *child = td_list_entry(pos, file_node_t, siblings);
    if (child->type == NODE_DIR) {
      uint64_t sub_size = 0;
      count += tree_count_marked(child, &sub_size);
      total_size += sub_size;
    } else if (child->marked) {
      count++;
      total_size += child->size;
    }
  }
  if (size_out)
    *size_out = total_size;
  return count;
}

const char *tree_format_size(uint64_t bytes, char *buf, size_t bufsize) {
  const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};
  int suffix_idx = 0;
  double dsize = (double)bytes;
  while (dsize >= 1024.0 && suffix_idx < 4) {
    dsize /= 1024.0;
    suffix_idx++;
  }
  if (suffix_idx == 0)
    snprintf(buf, bufsize, "%llu %s", (unsigned long long)bytes, suffixes[suffix_idx]);
  else
    snprintf(buf, bufsize, "%.1f %s", dsize, suffixes[suffix_idx]);
  return buf;
}
