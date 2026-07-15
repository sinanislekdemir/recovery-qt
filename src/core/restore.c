/*
    File: restore.c

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
#include "fat_dir.h"
#include "ext2_dir.h"
#include "ntfs_dir.h"
#include "exfat_dir.h"
#include "rfs_dir.h"
#include "fat.h"
#include "ntfs.h"
#include "log.h"
#include "hdaccess.h"
#include "recovery.h"
#include "progress.h"

/*
 * ============================================================================
 * RESTORE ARCHITECTURE (prestore.c)
 *
 * Two public entry points:
 *   restore_files()       - batch restore of all marked nodes in scan_tree_t
 *   restore_file_node()   - restore a single node (used for preview capture)
 *   read_file_bytes()     - read file data into malloc'd buffer (for preview)
 *
 * Three restore data paths for a single file (restore_file):
 *   1. FS-AWARE RESTORE: Normal file with filesystem metadata.
 *      Uses dir_data->copy_file() — the FS-specific driver (fat/ntfs/ext2)
 *      reads metadata and copies data from the source partition.
 *      Called from restore_file() via the dir_data dispatch.
 *
 *   2. ORPHAN RESTORE (raw sector reads): Orphan/backup-restored files
 *      without FS metadata. Data read directly from disk via two sub-paths:
 *        a. cluster_list: For EXT FS / backup files with cluster chains.
 *           Reads each cluster sequentially from part_offset + cluster_list[ci].
 *        b. first_sector/num_sectors: For carved files with known sector range.
 *           Reads from part_offset + first_sector in 64KB chunks.
 *
 *   3. MEMORY-CAPTURE PREVIEW: Normal FS files previewed via read_file_bytes().
 *      Sets a memory capture hook, runs restore_file_node() which triggers
 *      copy_file() to write data, captures the output to a malloc'd buffer.
 *      This is a hack: it does a full "restore" just to capture bytes.
 *
 * DUPLICATION: Path building from parent chain appears THREE times:
 *   - restore_orphan() lines 170-185: builds path from parent→root chain
 *   - restore_file() lines 299-320: same parent chain walk for relative_path
 *   - read_file_bytes() uses restore_file_node which calls restore_file
 *     which builds the path again
 *
 *   Also: restore_orphan() and read_file_bytes() share near-identical
 *   cluster_list reading loops and sector-reading loops. The only difference
 *   is restore_orphan writes to FILE* while read_file_bytes writes to buffer.
 *
 * ============================================================================
 */

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

/*
 * init_fs_for_restore: Detects filesystem type and initializes the matching
 * dir_data driver. Used by both restore_files() and restore_file_node().
 *
 * Uses a table-driven approach with three ordered phases:
 *   1. Runtime type-detect test (is_part_fat, is_part_ntfs)
 *   2. Partition type code match (upart_type switch)
 *   3. Fallbacks (try ext2 → fat → ntfs in order)
 */
static int init_fs_for_restore(disk_t *disk, const partition_t *partition,
    dir_data_t *dir_data, const char *local_dir)
{
  typedef int (*fs_init_4_fn)(disk_t *, const partition_t *, dir_data_t *, int);

  struct {
    int (*test)(const partition_t *);
    fs_init_4_fn init;
  } detect_phase[] = {
    { is_part_fat,  dir_partition_fat_init },
  };

  struct {
    unsigned int utype;
    fs_init_4_fn init;
  } utype_phase[] = {
    { UP_FAT12, dir_partition_fat_init },
    { UP_FAT16, dir_partition_fat_init },
    { UP_FAT32, dir_partition_fat_init },
    { UP_EXFAT, dir_partition_exfat_init },
    { UP_EXT4,  dir_partition_ext2_init },
    { UP_EXT3,  dir_partition_ext2_init },
    { UP_EXT2,  dir_partition_ext2_init },
  };

  fs_init_4_fn fallback_phase[] = {
    dir_partition_ext2_init,
    dir_partition_fat_init,
  };

  size_t i;
  int success;

  success = 0;

  /* Phase 1: run-time filesystem detection */
  if (is_part_ntfs(partition))
  {
    if (dir_partition_ntfs_init(disk, partition, dir_data, 0, 0) == DIR_PART_OK
        || dir_partition_exfat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
      success = 1;
  }
  for (i = 0; !success && i < sizeof(detect_phase)/sizeof(detect_phase[0]); i++)
  {
    if (detect_phase[i].test(partition) &&
        detect_phase[i].init(disk, partition, dir_data, 0) == DIR_PART_OK)
      success = 1;
  }

  /* Phase 2: partition type code match */
  if (!success && partition->upart_type == UP_NTFS)
  {
    if (dir_partition_ntfs_init(disk, partition, dir_data, 0, 0) == DIR_PART_OK)
      success = 1;
  }
  for (i = 0; !success && i < sizeof(utype_phase)/sizeof(utype_phase[0]); i++)
  {
    if (partition->upart_type == utype_phase[i].utype &&
        utype_phase[i].init(disk, partition, dir_data, 0) == DIR_PART_OK)
      success = 1;
  }

  /* Phase 3: fallback — try all drivers in order */
  for (i = 0; !success && i < sizeof(fallback_phase)/sizeof(fallback_phase[0]); i++)
  {
    if (fallback_phase[i](disk, partition, dir_data, 0) == DIR_PART_OK)
      success = 1;
  }
  if (!success && dir_partition_ntfs_init(disk, partition, dir_data, 0, 0) == DIR_PART_OK)
    success = 1;

  if (success)
  {
    dir_data->local_dir = strdup(local_dir);
    return 0;
  }
  return -1;
}

/*
 * restore_orphan: Raw sector restore for files without FS metadata.
 * Two data sources:
 *   1. node->cluster_list (EXT/backup): reads clusters sequentially
 *   2. node->first_sector (carved): reads contiguous sector range
 *
 * DUPLICATION: Path building from parent chain (lines 218-233) is functionally
 * identical to the path building in restore_file() (lines 344-364) and the
 * path display in BrowserWidget::setupUi() currentChanged handler.
 * Consider a common tree_get_path() utility in ptree.c.
 */
static int restore_orphan(disk_t *disk, const partition_t *partition,
    const file_node_t *node, const char *dest_dir, uid_t uid, gid_t gid)
{
  char full_path[4096];
  char dir_path[4096];
  char *last_slash;
  FILE *f_out;
  unsigned char buffer[65536];
  uint64_t offset;
  uint64_t remaining;

  tree_get_path(node, NULL, full_path, sizeof(full_path));

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

/*
 * restore_file: Single-file restore dispatcher.
 * Orphan/backup files → restore_orphan() (raw sector reads)
 * Normal files       → dir_data->copy_file() (FS-specific driver)
 *
 * Builds a file_info_t from file_node_t metadata, constructs the relative
 * path from the parent chain, sets dir_data->current_directory so the
 * FS driver knows which directory to read from.
 */
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
    char path_buf[4096];
    char *dup_path;

    tree_get_path(node->parent ? node->parent : node, NULL,
        path_buf, sizeof(path_buf));
    if (path_buf[0] == '\0')
      snprintf(relative_path, sizeof(relative_path), "/");
    else
      snprintf(relative_path, sizeof(relative_path), "%s", path_buf);

    dup_path = strdup(relative_path);
    strncpy(dir_data->current_directory, dup_path, DIR_NAME_LEN - 1);
    dir_data->current_directory[DIR_NAME_LEN - 1] = '\0';
    free(dup_path);
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

/*
 * restore_node_recursive: Walks the file_node_t tree recursively.
 * For marked directories: creates destination directory.
 * For marked files: calls restore_file(), reports progress via
 *   g_restorer_progress (percentage + current file) and
 *   g_restorer_file (per-file ok/fail callback).
 *
 * Called by restore_files() starting from tree->root.
 */
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

/*
 * read_file_bytes: Read raw file content into a malloc'd buffer for preview.
 * THREE DATA PATHS (evaluated in order):
 *
 * PATH 1 (cluster_list): File has EXT/backup cluster chain.
 *   Allocates up to 64 MB, reads each cluster sequentially from disk.
 *   Each cluster read at part_offset + cluster_list[ci], max 64KB per chunk.
 *   Grows buffer via realloc() if needed.
 *
 * PATH 2 (orphan + first_sector): Carved file with known sector range.
 *   Reads from part_offset + first_sector, max 64 MB.
 *   Fixed 64KB read chunks.
 *
 * PATH 3 (normal FS file): Falls back to memory_capture mechanism.
 *   Sets a global memory capture hook via set_memory_capture(),
 *   runs restore_file_node() which calls the FS-specific copy_file() driver,
 *   captures all bytes written to the output file into a memory buffer,
 *   then clears the hook and copies the buffer into a malloc'd result.
 *   WARNING: This is expensive - a full FS restore is run just to capture
 *   bytes. The captured data goes through the full filesystem driver stack.
 *
 * DUPLICATION: The cluster-loop in PATH 1 is structurally identical to
 * the cluster loop in restore_orphan() - same chunk size (64KB), same
 * offset calculation, same pread() calls. Only the destination differs
 * (FILE* vs buffer). Consider extracting a common read_clusters() helper.
 */
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

/*
 * restore_file_node: Single-node restore entry point.
 * Used for: (a) single-file preview capture via read_file_bytes()
 *          (b) Restorer::start() with onlyNode parameter
 *
 * Initializes FS, creates dest dir, calls restore_file() for one node.
 */
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

/*
 * restore_files: Batch restore of all marked nodes in the tree.
 * Main entry point called by Restorer C++ wrapper (src/wrappers/restorer.cpp).
 *
 * Flow: init_fs → mkdir → count marked → progress(0%) → recursive walk →
 * progress(100%) → cleanup.
 *
 * The total_count from tree_count_marked() may not match actual restored
 * count because restore_node_recursive() only restores marked FILES
 * (ignoring marked directories from the count for progress calculation).
 */
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
