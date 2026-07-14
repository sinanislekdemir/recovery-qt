/*

    File: pscanner.c

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
#include <time.h>
#include "types.h"
#include "common.h"
#include "intrf.h"
#include "dir_common.h"
#include "dir.h"
#include "dirn.h"
#include "fat_dir.h"
#include "fat_common.h"
#include "exfat_dir.h"
#include "ext2_dir.h"
#include "ntfs_dir.h"
#include "rfs_dir.h"
#include "fat.h"
#include "ntfs.h"
#include "log.h"
#include "photorec_nc.h"
#include "hdaccess.h"
#include "progress_cb.h"
#if defined(HAVE_LIBNTFS)
#include <ntfs/volume.h>
#include <ntfs/attrib.h>
#endif
#if defined(HAVE_LIBNTFS3G)
#include <ntfs-3g/volume.h>
#include <ntfs-3g/attrib.h>
#endif
#ifdef HAVE_ICONV_H
#include <iconv.h>
#endif
#include "ntfs_udl.h"
#include "ntfs_inc.h"
#include "filegen.h"
#if defined(HAVE_LIBEXT2FS)
#include <ext2fs/ext2fs.h>
#include "ext2_inc.h"
#endif

#define MAX_DIR_DEPTH 256
#define FAT_BATCH_CLUSTERS 64

static uint64_t g_deleted_count = 0;
static uint64_t g_total_count = 0;

static void update_progress(const char *path)
{
  if (g_scanner_progress)
    g_scanner_progress(g_deleted_count, g_total_count, path);
}

static void indx_progress_cb(const char *msg, uint64_t current,
    uint64_t total, uint64_t found)
{
  if (g_scanner_indx_progress)
    g_scanner_indx_progress(msg, current, total, found);
}

static int is_fat_type(const partition_t *partition)
{
  switch (partition->upart_type)
  {
    case UP_FAT12:
    case UP_FAT16:
    case UP_FAT32:
      return 1;
    default:
      break;
  }
  return is_part_fat(partition);
}

static int is_ntfs_type(const partition_t *partition)
{
  if (partition->upart_type == UP_NTFS)
    return 1;
  return is_part_ntfs(partition);
}

static int is_ext2_type(const partition_t *partition)
{
  switch (partition->upart_type)
  {
    case UP_EXT2:
    case UP_EXT3:
    case UP_EXT4:
      return 1;
    default:
      break;
  }
  return 0;
}

static dir_partition_t scanner_init_fs(disk_t *disk, const partition_t *partition,
    dir_data_t *dir_data)
{
  if (is_fat_type(partition))
  {
    if (dir_partition_fat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
      return DIR_PART_OK;
  }
  else if (is_ntfs_type(partition))
  {
    if (dir_partition_ntfs_init(disk, partition, dir_data, 0, 0) == DIR_PART_OK)
      return DIR_PART_OK;
    if (dir_partition_exfat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
      return DIR_PART_OK;
  }
  else if (is_ext2_type(partition))
  {
    if (dir_partition_ext2_init(disk, partition, dir_data, 0) == DIR_PART_OK)
      return DIR_PART_OK;
    if (dir_partition_reiser_init(disk, partition, dir_data, 0) == DIR_PART_OK)
      return DIR_PART_OK;
  }
  switch (partition->upart_type)
  {
    case UP_FAT12:
    case UP_FAT16:
    case UP_FAT32:
      return dir_partition_fat_init(disk, partition, dir_data, 0);
    case UP_EXT4:
    case UP_EXT3:
    case UP_EXT2:
      return dir_partition_ext2_init(disk, partition, dir_data, 0);
    case UP_RFS:
    case UP_RFS2:
    case UP_RFS3:
      return dir_partition_reiser_init(disk, partition, dir_data, 0);
    case UP_NTFS:
      return dir_partition_ntfs_init(disk, partition, dir_data, 0, 0);
    case UP_EXFAT:
      return dir_partition_exfat_init(disk, partition, dir_data, 0);
    default:
      break;
  }
  if (dir_partition_ext2_init(disk, partition, dir_data, 0) == DIR_PART_OK)
    return DIR_PART_OK;
  if (dir_partition_fat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
    return DIR_PART_OK;
  if (dir_partition_ntfs_init(disk, partition, dir_data, 0, 0) == DIR_PART_OK)
    return DIR_PART_OK;
  if (dir_partition_exfat_init(disk, partition, dir_data, 0) == DIR_PART_OK)
    return DIR_PART_OK;
  return DIR_PART_ENOIMP;
}

static int is_valid_inode(const file_info_t *file, unsigned int depth,
    unsigned long int inode_known[])
{
  unsigned int i;
  for (i = 0; i < depth; i++)
  {
    if (inode_known[i] == file->st_ino)
      return 0;
  }
  return 1;
}

static int scan_dir_recursive(scan_tree_t *tree, disk_t *disk,
    const partition_t *partition, dir_data_t *dir_data,
    const unsigned long int inode, const char *path,
    unsigned int depth, unsigned long int inode_known[])
{
  file_info_t dir_list;
  struct td_list_head *file_walker;

  if (depth >= MAX_DIR_DEPTH)
    return 0;

  TD_INIT_LIST_HEAD(&dir_list.list);
  if (dir_data->get_dir(disk, partition, dir_data, inode, &dir_list) != 0)
    return -1;

  inode_known[depth] = inode;

  td_list_for_each(file_walker, &dir_list.list)
  {
    const file_info_t *fi = td_list_entry_const(file_walker, const file_info_t, list);
    int deleted = (fi->status & FILE_STATUS_DELETED) != 0;
    int is_dir = LINUX_S_ISDIR(fi->st_mode) != 0;
    char full_path[4096];

    if (is_dir && fi->name[0] == '.' && (fi->name[1] == '\0' ||
        (fi->name[1] == '.' && fi->name[2] == '\0')))
      continue;

    if (strcmp(path, "/") == 0)
      snprintf(full_path, sizeof(full_path), "/%s", fi->name);
    else
      snprintf(full_path, sizeof(full_path), "%s/%s", path, fi->name);

    {
      const uint64_t num_sectors = fi->st_size > 0
          ? (fi->st_size + disk->sector_size - 1) / disk->sector_size : 0;
      tree_add_path(tree, full_path, is_dir,
          fi->st_size, fi->st_ino, num_sectors,
          fi->td_mtime, disk->sector_size, deleted);
    }

    g_total_count++;
    if (deleted)
      g_deleted_count++;

    if (g_total_count % 500 == 0)
      update_progress(path);

    if (is_dir && is_valid_inode(fi, depth + 1, inode_known))
    {
      scan_dir_recursive(tree, disk, partition, dir_data,
          fi->st_ino, full_path, depth + 1, inode_known);
    }
  }

  delete_list_file(&dir_list);
  return 0;
}

static int is_likely_fat_dir(const unsigned char *buffer, unsigned int size)
{
  const struct msdos_dir_entry *de;
  unsigned int i;

  if (size < 64)
    return 0;

  de = (const struct msdos_dir_entry *)buffer;

  for (i = 0; i < 4 && (i + 1) * 32 <= size; i++)
  {
    unsigned int j;
    if (de[i].name[0] == 0)
      return (i > 0);
    if (de[i].name[0] == 0xe5)
      continue;
    if (de[i].name[0] < 0x20 && de[i].name[0] != 0x05)
      return 0;
    if (de[i].attr == 0 && de[i].name[0] == ' ')
      return 0;
    if ((de[i].attr & ~(ATTR_RO | ATTR_HIDDEN | ATTR_SYS |
        ATTR_VOLUME | ATTR_DIR | ATTR_ARCH)) != 0)
      return 0;
    for (j = 0; j < 8 && de[i].name[j] != ' '; j++)
    {
      unsigned char c = de[i].name[j];
      if (c < 0x20 && c != 0x05)
        return 0;
      if (c > 0x7e)
        return 0;
    }
    if (de[i].name[0] == '.' && (de[i].name[1] == ' ' ||
        (de[i].name[1] == '.' && de[i].name[2] == ' ')))
      return 1;
  }
  return 1;
}

static int scanner_deep_fat(scan_tree_t *tree, disk_t *disk,
    const partition_t *partition)
{
  struct fat_boot_sector fat_boot;
  uint64_t start_data;
  uint64_t part_size;
  unsigned long int fat_length;
  unsigned long int no_of_cluster;
  unsigned int sectors_per_cluster;
  unsigned int cluster_size;
  unsigned int sector_size;
  unsigned long int cluster;
  unsigned long int free_clusters;
  unsigned long int scanned;
  uint32_t *fat_table = NULL;
  unsigned char *batch_buffer = NULL;
  unsigned int batch_count;
  uint64_t fat_offset;

  if (disk->pread(disk, &fat_boot, sizeof(fat_boot),
      partition->part_offset) != (int)sizeof(fat_boot))
    return -1;

  sector_size = fat_sector_size(&fat_boot);
  if (sector_size == 0)
    return -1;

  sectors_per_cluster = fat_boot.sectors_per_cluster;
  if (sectors_per_cluster < 1)
    return -1;

  cluster_size = sectors_per_cluster * sector_size;
  fat_length = le16(fat_boot.fat_length) > 0 ?
      le16(fat_boot.fat_length) : le32(fat_boot.fat32_length);
  part_size = fat_sectors(&fat_boot) > 0 ?
      fat_sectors(&fat_boot) : le32(fat_boot.total_sect);
  start_data = (uint64_t)(le16(fat_boot.reserved) +
      fat_boot.fats * fat_length) * sector_size;
  no_of_cluster = ((uint64_t)part_size * sector_size - start_data) /
      cluster_size;

  fat_offset = partition->part_offset +
      (uint64_t)le16(fat_boot.reserved) * sector_size;
  fat_table = NULL;
  if (fat_length > 0 && fat_length <= (1024UL * 1024 * 1024 / sector_size))
  {
    fat_table = (uint32_t *)MALLOC((size_t)(fat_length * sector_size));
  }
  if (fat_table)
  {
    if ((unsigned int)disk->pread(disk, fat_table,
        fat_length * sector_size, fat_offset) != fat_length * sector_size)
    {
      free(fat_table);
      fat_table = NULL;
    }
  }

  if (no_of_cluster > 100000000UL)
  {
    log_warning("scanner_deep_fat: cluster count too large (%lu), skipping\n",
        no_of_cluster);
    free(fat_table);
    return -1;
  }

  if (fat_table)
  {
    unsigned long int max_fat_clusters;
    max_fat_clusters = (unsigned long int)fat_length * sector_size /
        sizeof(uint32_t);
    if (no_of_cluster + 2 > max_fat_clusters)
    {
      free(fat_table);
      fat_table = NULL;
    }
  }

  free_clusters = 0;
  scanned = 0;

  batch_buffer = NULL;
  batch_count = 0;

  for (cluster = 2; cluster < no_of_cluster + 2; cluster++)
  {
    unsigned int next;
    int is_free;

    if (fat_table)
    {
      next = le32(fat_table[cluster]);
      is_free = (next == 0);
    }
    else
    {
      next = get_next_cluster(disk, partition, partition->upart_type,
          le16(fat_boot.reserved), cluster);
      is_free = (next == 0);
    }

    if (is_free)
    {
      if (batch_buffer == NULL)
      {
        batch_count = 0;
        batch_buffer = (unsigned char *)MALLOC(
            (size_t)FAT_BATCH_CLUSTERS * cluster_size);
      }

      if (batch_buffer && batch_count < FAT_BATCH_CLUSTERS)
      {
        uint64_t offset;
        unsigned int buf_off;
        buf_off = batch_count * cluster_size;
        offset = partition->part_offset + start_data +
            (uint64_t)(cluster - 2) * cluster_size;
        if ((unsigned int)disk->pread(disk, batch_buffer + buf_off,
            cluster_size, offset) == cluster_size)
        {
          batch_count++;
        }
      }

      free_clusters++;
    }
    else if (batch_count > 0)
    {
      unsigned int b;
      for (b = 0; b < batch_count; b++)
      {
        unsigned char *buf;
        buf = batch_buffer + (unsigned int)b * cluster_size;
        if (is_likely_fat_dir(buf, cluster_size))
        {
          file_info_t dir_list;
          struct td_list_head *file_walker;

          TD_INIT_LIST_HEAD(&dir_list.list);
          dir_fat_aux(buf, cluster_size, FLAG_LIST_DELETED, &dir_list);

          td_list_for_each(file_walker, &dir_list.list)
          {
            const file_info_t *fi = td_list_entry_const(file_walker,
                const file_info_t, list);
            int is_dir_de;
            char full_path[4096];

            if (fi->name[0] == '.' && (fi->name[1] == '\0' ||
                (fi->name[1] == '.' && fi->name[2] == '\0')))
              continue;

            is_dir_de = LINUX_S_ISDIR(fi->st_mode) != 0;
            snprintf(full_path, sizeof(full_path),
                "/Deep Scan Results/%s", fi->name);

            {
              const uint64_t num_sectors = fi->st_size > 0 ?
                  (fi->st_size + sector_size - 1) / sector_size : 0;
              tree_add_path(tree, full_path, is_dir_de,
                  fi->st_size, fi->st_ino, num_sectors,
                  fi->td_mtime, sector_size, 1);
            }

            g_total_count++;
            g_deleted_count++;
          }
          delete_list_file(&dir_list);
        }
      }
      free(batch_buffer);
      batch_buffer = NULL;
      batch_count = 0;
    }

    scanned++;

    if (scanned % 10000 == 0)
    {
#ifdef HAVE_NCURSES
      char progress_msg[256];
      snprintf(progress_msg, sizeof(progress_msg),
          "FAT deep scan: cluster %lu/%lu", scanned, no_of_cluster);
      update_progress(progress_msg);
#endif
    }
  }

  if (batch_buffer)
  {
    unsigned int b;
    for (b = 0; b < batch_count; b++)
    {
      unsigned char *buf;
      buf = batch_buffer + (unsigned int)b * cluster_size;
      if (is_likely_fat_dir(buf, cluster_size))
      {
        file_info_t dir_list;
        struct td_list_head *file_walker;

        TD_INIT_LIST_HEAD(&dir_list.list);
        dir_fat_aux(buf, cluster_size, FLAG_LIST_DELETED, &dir_list);

        td_list_for_each(file_walker, &dir_list.list)
        {
          const file_info_t *fi = td_list_entry_const(file_walker,
              const file_info_t, list);
          int is_dir_de;
          char full_path[4096];

          if (fi->name[0] == '.' && (fi->name[1] == '\0' ||
              (fi->name[1] == '.' && fi->name[2] == '\0')))
            continue;

          is_dir_de = LINUX_S_ISDIR(fi->st_mode) != 0;
          snprintf(full_path, sizeof(full_path),
              "/Deep Scan Results/%s", fi->name);

          {
            const uint64_t num_sectors = fi->st_size > 0 ?
                (fi->st_size + sector_size - 1) / sector_size : 0;
            tree_add_path(tree, full_path, is_dir_de,
                fi->st_size, fi->st_ino, num_sectors,
                fi->td_mtime, sector_size, 1);
          }

          g_total_count++;
          g_deleted_count++;
        }
        delete_list_file(&dir_list);
      }
    }
    free(batch_buffer);
  }

  free(fat_table);

  log_info("FAT deep scan: %lu free clusters, %lu found entries\n",
      (unsigned long)free_clusters, (unsigned long)g_deleted_count);

  return 0;
}

#if defined(HAVE_LIBEXT2FS)
static int scanner_deep_ext(scan_tree_t *tree, disk_t *disk,
    const partition_t *partition, dir_data_t *dir_data)
{
  struct ext2_dir_struct *ls;
  ext2_filsys fs;
  ext2_inode_scan scan;
  ext2_ino_t ino;
  struct ext2_inode inode;
  unsigned long long total_inodes;
  unsigned long long scanned;
  unsigned long long found;

  (void)partition;

  ls = (struct ext2_dir_struct *)dir_data->private_dir_data;
  if (ls == NULL || ls->current_fs == NULL)
    return -1;
  fs = ls->current_fs;
  if (fs->super == NULL)
  {
    log_warning("scanner_deep_ext: super is NULL, skipping EXT inode scan\n");
    return -1;
  }

  total_inodes = fs->super->s_inodes_count;
  if (total_inodes == 0)
    total_inodes = 1;

  if (ext2fs_open_inode_scan(fs, 0, &scan) != 0)
  {
    log_warning("scanner_deep_ext: ext2fs_open_inode_scan failed\n");
    return -1;
  }

  scanned = 0;
  found = 0;

  while (ext2fs_get_next_inode(scan, &ino, &inode) == 0)
  {
    int is_reg;
    int is_dir;
    int is_symlink;
    int has_valid_metadata;
    char full_path[4096];
    uint64_t num_sectors;
    uint64_t file_size;
    unsigned int sector_size;

    if (ino >= (ext2_ino_t)total_inodes)
      break;
    if (scanned >= total_inodes)
      break;

    scanned++;
    if (g_scanner_cancel && g_scanner_cancel())
      break;
    if (scanned % 50000 == 0)
    {
      char msg[256];
      snprintf(msg, sizeof(msg),
          "EXT inode deep scan: %llu/%llu (%llu found)",
          scanned, total_inodes, found);
      update_progress(msg);
    }

    if (ino < EXT2_FIRST_INO(fs->super))
      continue;

    is_reg = LINUX_S_ISREG(inode.i_mode) ? 1 : 0;
    is_dir = LINUX_S_ISDIR(inode.i_mode) ? 1 : 0;
    is_symlink = LINUX_S_ISLNK(inode.i_mode) ? 1 : 0;

    if (!is_reg && !is_dir && !is_symlink)
      continue;

    has_valid_metadata = 0;
    file_size = inode.i_size | ((uint64_t)inode.osd2.linux2.l_i_file_acl_high << 32);
    if (file_size == 0 && inode.i_blocks == 0 && inode.i_dtime == 0)
      continue;

    if (file_size > 0 || inode.i_blocks > 0 || inode.i_dtime > 0)
      has_valid_metadata = 1;
    if (!has_valid_metadata)
      continue;

    sector_size = disk->sector_size;
    if (file_size > 0)
      num_sectors = (file_size + sector_size - 1) / sector_size;
    else
      num_sectors = ((uint64_t)inode.i_blocks * 512 + sector_size - 1) / sector_size;

    snprintf(full_path, sizeof(full_path),
        "/Deep Scan Results/inode_%lu.%s",
        (unsigned long)ino, is_dir ? "dir" : "file");

    tree_add_path(tree, full_path, is_dir, file_size,
        (uint64_t)ino, num_sectors, inode.i_mtime, sector_size, 1);

    g_total_count++;
    g_deleted_count++;
    found++;
  }

  ext2fs_close_inode_scan(scan);

  log_info("EXT inode deep scan: %llu freed inodes with metadata found\n", found);

  return 0;
}
#endif

int scanner_run(scan_tree_t *tree, disk_t *disk, const partition_t *partition, int deep)
{
  dir_data_t dir_data;
  dir_partition_t res;
  unsigned long int inode_known[MAX_DIR_DEPTH];
  int is_fat;
  int is_ntfs;

  memset(&dir_data, 0, sizeof(dir_data));
  memset(inode_known, 0, sizeof(inode_known));

  res = scanner_init_fs(disk, partition, &dir_data);
  log_info("scanner_run: scanner_init_fs returned %d\n", (int)res);
  if (res != DIR_PART_OK)
  {
    log_info("scanner_run: FS not detected, returning -1\n");
    return -1;
  }

  log_info("scanner_run: FS detected, starting recursive walk\n");

  dir_data.param |= FLAG_LIST_DELETED;
  dir_data.verbose = 0;

  is_fat = is_fat_type(partition);
  is_ntfs = is_ntfs_type(partition);
  if (!is_ntfs)
    is_ntfs = is_part_ntfs(partition);

  dir_data.display = NULL;

  g_deleted_count = 0;
  g_total_count = 0;

  scan_dir_recursive(tree, disk, partition, &dir_data,
      dir_data.current_inode, "/", 0, inode_known);

  if (g_scanner_cancel && g_scanner_cancel())
    return 1;

#if defined(HAVE_LIBNTFS) || defined(HAVE_LIBNTFS3G)
  if (is_ntfs)
  {
    struct ntfs_dir_struct *ls;
    ls = (struct ntfs_dir_struct *)dir_data.private_dir_data;
    if (ls != NULL && ls->vol != NULL)
    {
      file_info_t mft_list;
      struct td_list_head *file_walker;

      TD_INIT_LIST_HEAD(&mft_list.list);

      if (g_scanner_progress)
        g_scanner_progress(g_deleted_count, g_total_count, "MFT scan");

      {
        extern file_check_list_t file_check_list;
        if (file_check_list.list.next == &file_check_list.list)
        {
          extern file_enable_t array_file_enable[];
          file_enable_t *fe;
          for (fe = array_file_enable; fe->file_hint != NULL; fe++)
            fe->enable = fe->file_hint->enable_by_default;
          init_file_stats(array_file_enable);
        }
      }

      scan_disk(ls->vol, &mft_list);

      td_list_for_each(file_walker, &mft_list.list)
      {
        const file_info_t *fi = td_list_entry_const(file_walker, const file_info_t, list);
        char full_path[4096];
        int is_dir;
        const uint64_t num_sectors = fi->st_size > 0
            ? (fi->st_size + disk->sector_size - 1) / disk->sector_size : 0;

        is_dir = LINUX_S_ISDIR(fi->st_mode) != 0;

        if (fi->name[0] == '/')
          snprintf(full_path, sizeof(full_path), "/ORPHAN%s", fi->name);
        else
          snprintf(full_path, sizeof(full_path), "/ORPHAN/%s", fi->name);

        tree_add_path(tree, full_path, is_dir,
            fi->st_size, fi->st_ino, num_sectors,
            fi->td_mtime, disk->sector_size, 1);
        {
          file_node_t *fn;
          fn = tree_find_path(tree, full_path);
          if (fn)
          {
            fn->orphan = 1;
            ntfs_fill_clusters(fn, ls->vol, fi->st_ino);
            if (fn->cluster_list != NULL && fn->cluster_count > 0)
            {
              unsigned char *buf;
              unsigned int read_size;
              read_size = (unsigned int)(fn->cluster_size < 4096 ?
                  fn->cluster_size : 4096);
              buf = (unsigned char *)MALLOC(read_size);
              if (buf)
              {
                const file_hint_t *hint;
                if (disk->pread(disk, buf, read_size,
                    partition->part_offset + fn->cluster_list[0]) == (int)read_size)
                {
                  hint = carver_check_header(buf, read_size, 0, NULL, 0);
                  if (hint && hint->extension)
                  {
                    char new_name[4096];
                    char *dot;
                    dot = strrchr(fn->name, '.');
                    if (dot)
                      snprintf(new_name, sizeof(new_name), "%.*s.%s",
                          (int)(dot - fn->name), fn->name, hint->extension);
                    else
                      snprintf(new_name, sizeof(new_name), "%s.%s",
                          fn->name, hint->extension);
                    free(fn->name);
                    fn->name = strdup(new_name);
                  }
                }
                free(buf);
              }
            }
          }
          else
            log_warning("MFT scan: tree_find_path returned NULL for %s\n",
                full_path);
        }

        g_total_count++;
        g_deleted_count++;

        if (g_total_count % 500 == 0)
          update_progress("MFT scan");
      }

      delete_list_file(&mft_list);
    }
  }
#endif

#if defined(HAVE_LIBEXT2FS)
  if (is_ext2_type(partition))
  {
    struct ext2_dir_struct *ls;
    ls = (struct ext2_dir_struct *)dir_data.private_dir_data;
    if (ls != NULL && ls->current_fs != NULL)
    {
      if (g_scanner_progress)
        g_scanner_progress(g_deleted_count, g_total_count, "EXT inode scan");
      scanner_deep_ext(tree, disk, partition, &dir_data);
    }
  }
#endif

  if (!deep)
  {
    log_info("scanner_run: fast scan complete, %llu deleted files found\n", (unsigned long long)g_deleted_count);
    if (dir_data.close)
      dir_data.close(&dir_data);
    return 0;
  }

  if (is_fat)
  {
    if (g_scanner_progress)
      g_scanner_progress(g_deleted_count, g_total_count, "FAT deep scan");
    scanner_deep_fat(tree, disk, partition);
  }

#if defined(HAVE_LIBNTFS) || defined(HAVE_LIBNTFS3G)
  if (is_ntfs)
  {
    struct ntfs_dir_struct *ls;
    ls = (struct ntfs_dir_struct *)dir_data.private_dir_data;
    if (ls != NULL && ls->vol != NULL)
    {
      if (g_scanner_progress)
        g_scanner_progress(g_deleted_count, g_total_count, "INDX deep scan");
      scanner_deep_ntfs(tree, disk, partition, ls->vol, indx_progress_cb);
    }
  }
#endif

  if (!is_fat && !is_ntfs)
    carver_run(tree, disk, partition, NULL, 0);

  if (g_scanner_progress)
    g_scanner_progress(g_deleted_count, g_total_count, "Scan complete");

  if (dir_data.close)
    dir_data.close(&dir_data);

  return 0;
}
