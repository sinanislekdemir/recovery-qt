/*
    File: prestore.c

    Copyright (C) 2024 Christophe GRENIER <grenier@cgsecurity.org>

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
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include "types.h"
#include "common.h"
#include "intrf.h"
#include "dir_common.h"
#include "dir.h"
#include "dirn.h"
#include "fat_dir.h"
#include "ext2_dir.h"
#include "ntfs_dir.h"
#include "exfat_dir.h"
#include "rfs_dir.h"
#include "fat.h"
#include "ntfs.h"
#include "log.h"
#include "hdaccess.h"
#include "photorec_nc.h"
#include "progress_cb.h"

static void mkdir_recursive(const char *path)
{
  char tmp[4096];
  char *p;
  struct stat st;

  if (path[0] == '\0')
    return;

  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
    return;

  snprintf(tmp, sizeof(tmp), "%s", path);
  p = strrchr(tmp, '/');
  if (p && p != tmp)
  {
    *p = '\0';
    mkdir_recursive(tmp);
  }

#if defined(__MINGW32__) || defined(__CYGWIN__)
  mkdir(path);
#else
  mkdir(path, 0755);
#endif
}

static int init_fs_for_restore(disk_t *disk, const partition_t *partition,
    dir_data_t *dir_data, const char *local_dir)
{
  if (is_part_fat(partition))
  {
    if (dir_partition_fat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
    {
      dir_data->local_dir = strdup(local_dir);
      return 0;
    }
  }
  if (is_part_ntfs(partition))
  {
    if (dir_partition_ntfs_init(disk, partition, dir_data, 0, 0) == DIR_PART_OK)
    {
      dir_data->local_dir = strdup(local_dir);
      return 0;
    }
    if (dir_partition_exfat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
    {
      dir_data->local_dir = strdup(local_dir);
      return 0;
    }
  }
  switch (partition->upart_type)
  {
    case UP_FAT12:
    case UP_FAT16:
    case UP_FAT32:
      if (dir_partition_fat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
      {
        dir_data->local_dir = strdup(local_dir);
        return 0;
      }
      break;
    case UP_EXT4:
    case UP_EXT3:
    case UP_EXT2:
      if (dir_partition_ext2_init(disk, partition, dir_data, 0) == DIR_PART_OK)
      {
        dir_data->local_dir = strdup(local_dir);
        return 0;
      }
      break;
    case UP_NTFS:
      if (dir_partition_ntfs_init(disk, partition, dir_data, 0, 0) == DIR_PART_OK)
      {
        dir_data->local_dir = strdup(local_dir);
        return 0;
      }
      break;
    case UP_EXFAT:
      if (dir_partition_exfat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
      {
        dir_data->local_dir = strdup(local_dir);
        return 0;
      }
      break;
    default:
      break;
  }
  if (dir_partition_ext2_init(disk, partition, dir_data, 0) == DIR_PART_OK)
  {
    dir_data->local_dir = strdup(local_dir);
    return 0;
  }
  if (dir_partition_fat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
  {
    dir_data->local_dir = strdup(local_dir);
    return 0;
  }
  if (dir_partition_ntfs_init(disk, partition, dir_data, 0, 0) == DIR_PART_OK)
  {
    dir_data->local_dir = strdup(local_dir);
    return 0;
  }
  return -1;
}

static int restore_orphan(disk_t *disk, const partition_t *partition,
    const file_node_t *node, const char *dest_dir, uid_t uid, gid_t gid)
{
  char full_path[4096];
  char dir_path[4096];
  char *last_slash;
  uint64_t offset;
  uint64_t remaining;
  FILE *f_out;
  unsigned char buffer[65536];
  const file_node_t *p;
  char *parts[128];
  int count;
  int i;

  count = 0;
  p = node;
  while (p && p->parent)
  {
    parts[count++] = p->name;
    p = p->parent;
  }

  full_path[0] = '\0';
  for (i = count - 1; i >= 0; i--)
  {
    size_t len;
    len = strlen(full_path);
    snprintf(full_path + len, sizeof(full_path) - len,
        "%s%s", (len > 0 && full_path[len - 1] == '/') ? "" : "/", parts[i]);
  }

  snprintf(dir_path, sizeof(dir_path), "%s%s", dest_dir, full_path);
  last_slash = strrchr(dir_path, '/');
  if (last_slash)
  {
    *last_slash = '\0';
    mkdir_recursive(dir_path);
    *last_slash = '/';
  }
  else
  {
    mkdir_recursive(dest_dir);
  }

  f_out = fopen(dir_path, "wb");
  if (f_out == NULL)
  {
    log_error("Cannot create orphan file %s: %s\n", dir_path, strerror(errno));
    return CP_CREATE_FAILED;
  }

  if (node->cluster_list != NULL && node->cluster_count > 0 &&
      node->cluster_size > 0)
  {
    uint64_t bytes_done;
    uint64_t file_remaining;
    uint32_t ci;
    file_remaining = node->size;
    bytes_done = 0;
    for (ci = 0; ci < node->cluster_count && file_remaining > 0; ci++)
    {
      uint64_t cluster_offset;
      uint64_t cluster_bytes;
      uint64_t chunk;
      cluster_offset = partition->part_offset +
          node->cluster_list[ci];
      cluster_bytes = node->cluster_size;
      if (cluster_bytes > file_remaining)
        cluster_bytes = file_remaining;
      chunk = cluster_bytes > sizeof(buffer) ? sizeof(buffer) : cluster_bytes;
      if ((unsigned int)disk->pread(disk, buffer,
          (unsigned int)chunk, cluster_offset) != (unsigned int)chunk)
      {
        log_error("Error reading cluster %u for %s\n", ci, node->name);
        fclose(f_out);
        return CP_READ_FAILED;
      }
      if (fwrite(buffer, 1, (size_t)chunk, f_out) != (size_t)chunk)
      {
        log_error("Error writing backup file %s\n", dir_path);
        fclose(f_out);
        return CP_NOSPACE;
      }
      file_remaining -= chunk;
      bytes_done += chunk;
    }
  }
  else
  {
    offset = partition->part_offset + node->first_sector;
    remaining = (uint64_t)node->num_sectors * disk->sector_size;

    while (remaining > 0)
    {
      unsigned int chunk;
      chunk = (unsigned int)(remaining > sizeof(buffer) ? sizeof(buffer) : remaining);
      if ((unsigned int)disk->pread(disk, buffer, chunk, offset) != chunk)
      {
        log_error("Error reading orphan file %s at offset %llu\n",
            node->name, (unsigned long long)offset);
        fclose(f_out);
        return CP_READ_FAILED;
      }
      if (fwrite(buffer, 1, chunk, f_out) != chunk)
      {
        log_error("Error writing orphan file %s\n", dir_path);
        fclose(f_out);
        return CP_NOSPACE;
      }
      offset += chunk;
      remaining -= chunk;
    }
  }

  fclose(f_out);
  chmod(dir_path, 0666);
  if (chown(dir_path, uid, gid)) {}
  return CP_OK;
}

static int restore_file(disk_t *disk, const partition_t *partition,
    dir_data_t *dir_data, const file_node_t *node,
    const char *dest_dir, uid_t uid, gid_t gid)
{
  file_info_t fi;
  copy_file_t res;
  char relative_path[4096];

  if (node->orphan || (node->backup_restored && node->cluster_list != NULL))
    return restore_orphan(disk, partition, node, dest_dir, uid, gid);

  memset(&fi, 0, sizeof(fi));
  TD_INIT_LIST_HEAD(&fi.list);
  fi.name = node->name;
  fi.st_ino = (uint32_t)node->first_sector;
  fi.st_size = node->size;
  fi.st_mode = node->type == NODE_DIR ? LINUX_S_IFDIR : LINUX_S_IFREG;
  fi.td_mtime = node->mtime;
  fi.td_atime = node->mtime;
  fi.td_ctime = node->mtime;
  fi.status = node->deleted ? FILE_STATUS_DELETED : 0;

  {
    const file_node_t *p = node;
    char *parts[256];
    int count = 0;
    int i;

    relative_path[0] = '/';
    relative_path[1] = '\0';

    while (p && p->parent)
    {
      parts[count++] = p->name;
      p = p->parent;
    }
    if (count > 1)
    {
      for (i = count - 2; i >= 0; i--)
      {
        strcat(relative_path, parts[i]);
        if (i > 0)
          strcat(relative_path, "/");
      }
    }

    {
      char *dup_path = strdup(relative_path);
      strncpy(dir_data->current_directory, dup_path, DIR_NAME_LEN - 1);
      dir_data->current_directory[DIR_NAME_LEN - 1] = '\0';
      free(dup_path);
    }
  }

  if (dir_data->copy_file)
    res = dir_data->copy_file(disk, partition, dir_data, &fi);
  else
    res = CP_OK;

  if (res == CP_OK)
  {
    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s%s", dest_dir, relative_path);
    chmod(full_path, 0666);
    if (chown(full_path, uid, gid)) {}
  }

  return res;
}

static void restore_node_recursive(scan_tree_t *tree, disk_t *disk,
    const partition_t *partition, dir_data_t *dir_data,
    const file_node_t *dir, const char *dest_dir,
    uint64_t *ok_count, uint64_t *fail_count, uint64_t *total_count,
    uid_t uid, gid_t gid)
{
  struct td_list_head *pos;
  td_list_for_each(pos, &dir->children)
  {
    file_node_t *child = td_list_entry(pos, file_node_t, siblings);

    if (child->type == NODE_DIR)
    {
      if (child->marked)
      {
        char dir_path[4096];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", dest_dir, child->name);
        mkdir_recursive(dir_path);
      }
      restore_node_recursive(tree, disk, partition, dir_data,
          child, dest_dir, ok_count, fail_count, total_count, uid, gid);
    }
    else if (child->marked)
    {
      copy_file_t res;
      uint64_t processed;
      {
        int _res_int;
        _res_int = restore_file(disk, partition, dir_data, child, dest_dir,
            uid, gid);
        res = (copy_file_t)_res_int;
      }
      if (res == CP_OK)
        (*ok_count)++;
      else
        (*fail_count)++;

      processed = *ok_count + *fail_count;

      if (g_restorer_progress)
      {
        int pct;
        pct = (*total_count > 0) ? (int)(processed * 100 / *total_count) : 0;
        if (pct > 100) pct = 100;
        g_restorer_progress(pct, child->name, (int)*total_count, (int)processed);
      }
      if (g_restorer_file)
        g_restorer_file(child->name, (res == CP_OK));

      if (res != CP_OK)
      {
        char size_buf[32];
        tree_format_size(child->size, size_buf, sizeof(size_buf));
        log_error("Failed: %s (%s) - error %d\n",
            child->name, size_buf, (int)res);
      }
    }
  }
}

unsigned char *read_file_bytes(scan_tree_t *tree, disk_t *disk,
    const partition_t *partition, file_node_t *node, size_t *out_size)
{
  unsigned char *result;
  char *cap_buf;
  size_t cap_size;

  *out_size = 0;
  if (node == NULL || node->size == 0)
    return NULL;

  if (node->cluster_list != NULL && node->cluster_count > 0 &&
      node->cluster_size > 0)
  {
    size_t allocated;
    size_t used;
    uint32_t ci;
    uint64_t remaining;
    allocated = (size_t)(node->size < (64 * 1024 * 1024) ?
        node->size : (64 * 1024 * 1024));
    result = (unsigned char *)MALLOC(allocated);
    if (result == NULL)
      return NULL;
    used = 0;
    remaining = node->size;
    for (ci = 0; ci < node->cluster_count && remaining > 0; ci++)
    {
      uint64_t cluster_offset;
      uint64_t cluster_bytes;
      uint64_t chunk;
      cluster_offset = partition->part_offset + node->cluster_list[ci];
      cluster_bytes = node->cluster_size;
      if (cluster_bytes > remaining)
        cluster_bytes = remaining;
      while (cluster_bytes > 0)
      {
        chunk = cluster_bytes > 65536 ? 65536 : cluster_bytes;
        if (used + chunk > allocated)
        {
          size_t new_alloc = allocated * 2;
          unsigned char *new_buf;
          if (new_alloc < used + chunk)
            new_alloc = used + chunk;
          new_buf = (unsigned char *)realloc(result, new_alloc);
          if (new_buf == NULL)
            break;
          result = new_buf;
          allocated = new_alloc;
        }
        if ((unsigned int)disk->pread(disk, result + used,
            (unsigned int)chunk, cluster_offset) != (unsigned int)chunk)
        {
          free(result);
          return NULL;
        }
        used += (size_t)chunk;
        remaining -= chunk;
        cluster_offset += chunk;
        cluster_bytes -= chunk;
      }
    }
    *out_size = used;
    return result;
  }

  if (node->orphan && node->first_sector != 0 && node->num_sectors > 0)
  {
    uint64_t offset;
    uint64_t remaining;
    uint64_t maxRead = 64 * 1024 * 1024;
    size_t allocated;
    size_t used;
    offset = partition->part_offset + node->first_sector;
    remaining = (uint64_t)node->num_sectors * disk->sector_size;
    if (remaining > maxRead)
      remaining = maxRead;
    if (remaining == 0)
      remaining = 4096;
    allocated = (size_t)remaining;
    result = (unsigned char *)MALLOC(allocated);
    if (result != NULL)
    {
      used = 0;
      while (remaining > 0)
      {
        unsigned int chunk;
        chunk = (unsigned int)(remaining > 65536 ? 65536 : remaining);
        if ((unsigned int)disk->pread(disk, result + used, chunk, offset) != chunk)
        {
          free(result);
          return NULL;
        }
        used += chunk;
        offset += chunk;
        remaining -= chunk;
      }
      *out_size = used;
      return result;
    }
  }

  set_memory_capture();
  restore_file_node(tree, disk, partition, "/", node);
  clear_memory_capture();

  cap_buf = get_capture_buffer();
  cap_size = get_capture_size();
  if (cap_buf == NULL || cap_size == 0)
    return NULL;

  result = (unsigned char *)MALLOC(cap_size);
  if (result == NULL)
  {
    free(cap_buf);
    return NULL;
  }
  memcpy(result, cap_buf, cap_size);
  free(cap_buf);
  *out_size = cap_size;
  return result;
}

int restore_file_node(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
    const char *dest_dir, file_node_t *node)
{
  dir_data_t dir_data;
  struct stat dest_st;
  uid_t uid = 0;
  gid_t gid = 0;
  copy_file_t res;

  memset(&dir_data, 0, sizeof(dir_data));

  if (init_fs_for_restore(disk, partition, &dir_data, dest_dir) != 0)
    return -1;

  mkdir_recursive(dest_dir);

  if (stat(dest_dir, &dest_st) == 0)
  {
    uid = dest_st.st_uid;
    gid = dest_st.st_gid;
  }

  dir_data.display = NULL;

  {
    int _res_int;
    _res_int = restore_file(disk, partition, &dir_data, node,
        dest_dir, uid, gid);
    res = (copy_file_t)_res_int;
  }

  if (dir_data.close)
    dir_data.close(&dir_data);
  free(dir_data.local_dir);

  return (res == CP_OK) ? 0 : -1;
}

int restore_files(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
    const char *dest_dir)
{
  dir_data_t dir_data;
  uint64_t ok_count = 0;
  uint64_t fail_count = 0;
  uint64_t total_count;
  struct stat dest_st;
  uid_t uid = 0;
  gid_t gid = 0;

  memset(&dir_data, 0, sizeof(dir_data));

  if (init_fs_for_restore(disk, partition, &dir_data, dest_dir) != 0)
    return -1;

  mkdir_recursive(dest_dir);

  if (stat(dest_dir, &dest_st) == 0)
  {
    uid = dest_st.st_uid;
    gid = dest_st.st_gid;
  }

  total_count = tree_count_marked(tree->root, NULL);

  dir_data.display = NULL;

  if (g_restorer_progress)
    g_restorer_progress(0, "", (int)total_count, 0);

  restore_node_recursive(tree, disk, partition, &dir_data,
      tree->root, dest_dir, &ok_count, &fail_count, &total_count,
      uid, gid);

  if (g_restorer_progress)
    g_restorer_progress(100, "", (int)total_count, (int)total_count);

  if (dir_data.close)
    dir_data.close(&dir_data);
  free(dir_data.local_dir);

  return 0;
}
