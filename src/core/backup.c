/*
    File: backup.c

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
#include <time.h>
#include <errno.h>
#include "types.h"
#include "common.h"
#include "intrf.h"
#include "dir_common.h"
#include "dir.h"
#include "fat_dir.h"
#include "fat_common.h"
#include "exfat_dir.h"
#include "ext2_dir.h"
#include "ntfs_dir.h"
#include "rfs_dir.h"
#include "fat.h"
#include "exfat.h"
#include "ntfs.h"
#include "log.h"
#include "hdaccess.h"
#include "recovery.h"
#include "backup.h"
#if defined(HAVE_LIBNTFS)
#include <ntfs/volume.h>
#include <ntfs/attrib.h>
#include <ntfs/mft.h>
#elif defined(HAVE_LIBNTFS3G)
#include <ntfs-3g/volume.h>
#include <ntfs-3g/attrib.h>
#include <ntfs-3g/mft.h>
#endif
#if defined(HAVE_LIBEXT2FS)
#include <ext2fs/ext2fs.h>
#endif
#include "ntfs_inc.h"
#include "ext2_inc.h"

#define BACKUP_MAX_CLUSTERS_PER_FILE (1024 * 1024)

static uint64_t g_backup_processed = 0;
static uint64_t g_backup_total = 0;
static uint64_t g_backup_extra = 0;
static uint64_t g_last_update = 0;

static int write_le32(FILE *f, uint32_t v) {
  unsigned char b[4];
  b[0] = (unsigned char)(v & 0xFF);
  b[1] = (unsigned char)((v >> 8) & 0xFF);
  b[2] = (unsigned char)((v >> 16) & 0xFF);
  b[3] = (unsigned char)((v >> 24) & 0xFF);
  return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}

static int write_le64(FILE *f, uint64_t v) {
  unsigned char b[8];
  unsigned int i;
  for (i = 0; i < 8; i++) {
    b[i] = (unsigned char)(v & 0xFF);
    v >>= 8;
  }
  return fwrite(b, 1, 8, f) == 8 ? 0 : -1;
}

static int write_le16(FILE *f, uint16_t v) {
  unsigned char b[2];
  b[0] = (unsigned char)(v & 0xFF);
  b[1] = (unsigned char)((v >> 8) & 0xFF);
  return fwrite(b, 1, 2, f) == 2 ? 0 : -1;
}

static uint32_t read_le32(const unsigned char *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_le64(const unsigned char *p) {
  uint64_t v;
  unsigned int i;
  v = 0;
  for (i = 0; i < 8; i++)
    v |= ((uint64_t)p[i]) << (i * 8);
  return v;
}

static uint16_t read_le16(const unsigned char *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int is_EOC_fat(const unsigned int cluster, const upart_type_t upart_type) {
  if (upart_type == UP_FAT12)
    return ((cluster & 0x0ff8) == (unsigned)0x0FF8);
  else if (upart_type == UP_FAT16)
    return ((cluster & 0x0fff8) == (unsigned)0xFFF8);
  else
    return ((cluster & 0xffffff8) == (unsigned)0x0FFFFFF8);
}

static int fat_collect_clusters(disk_t *disk, const partition_t *partition, const struct fat_boot_sector *fat_header,
                                unsigned long int first_cluster, uint64_t **clusters_out, uint32_t *count_out) {
  unsigned long int cluster;
  unsigned long int no_of_cluster;
  unsigned long int fat_length;
  unsigned int sectors_per_cluster;
  unsigned int sector_size;
  uint64_t start_data;
  uint64_t part_size;
  uint32_t capacity;
  uint32_t count;
  uint64_t *list;
  int fat_offset;

  sector_size = fat_sector_size(fat_header);
  sectors_per_cluster = fat_header->sectors_per_cluster;
  fat_length = le16(fat_header->fat_length) > 0 ? le16(fat_header->fat_length) : le32(fat_header->fat32_length);
  part_size = fat_sectors(fat_header) > 0 ? fat_sectors(fat_header) : le32(fat_header->total_sect);
  start_data = (uint64_t)(le16(fat_header->reserved) + fat_header->fats * fat_length) * sector_size;
  no_of_cluster =
      ((uint64_t)part_size * sector_size - start_data) / (unsigned long int)(sectors_per_cluster * sector_size);

  fat_offset = le16(fat_header->reserved);

  capacity = 256;
  list = (uint64_t *)MALLOC(capacity * sizeof(uint64_t));
  if (list == NULL)
    return -1;
  count = 0;
  cluster = first_cluster;

  while (cluster >= 2 && cluster <= no_of_cluster + 2 && count < BACKUP_MAX_CLUSTERS_PER_FILE) {
    unsigned int next;
    uint64_t cluster_offset;

    if (count >= capacity) {
      uint64_t *new_list;
      uint32_t new_cap;
      new_cap = capacity * 2;
      new_list = (uint64_t *)realloc(list, new_cap * sizeof(uint64_t));
      if (new_list == NULL) {
        free(list);
        return -1;
      }
      list = new_list;
      capacity = new_cap;
    }

    cluster_offset = start_data + (uint64_t)(cluster - 2) * sectors_per_cluster * sector_size;
    list[count++] = cluster_offset;

    next = get_next_cluster(disk, partition, partition->upart_type, fat_offset, cluster);
    if (is_EOC_fat(next, partition->upart_type))
      break;
    cluster = next;
  }

  *clusters_out = list;
  *count_out = count;
  return 0;
}

static int exfat_collect_clusters(disk_t *disk, const partition_t *partition,
                                  const struct exfat_super_block *exfat_header, unsigned long int first_cluster,
                                  uint64_t **clusters_out, uint32_t *count_out) {
  unsigned long int cluster;
  unsigned long int total_clusters;
  uint32_t capacity;
  uint32_t count;
  uint64_t *list;
  unsigned int cluster_shift;
  uint64_t cluster_block_base;
  uint64_t fat_offset;

  cluster_shift = exfat_header->block_per_clus_bits + exfat_header->blocksize_bits;
  total_clusters = le32(exfat_header->total_clusters);
  cluster_block_base = (uint64_t)le32(exfat_header->clus_blocknr) << exfat_header->blocksize_bits;
  fat_offset = (uint64_t)le32(exfat_header->fat_blocknr) << exfat_header->blocksize_bits;

  capacity = 256;
  list = (uint64_t *)MALLOC(capacity * sizeof(uint64_t));
  if (list == NULL)
    return -1;
  count = 0;
  cluster = first_cluster;

  while (cluster >= 2 && cluster <= total_clusters + 2 && count < BACKUP_MAX_CLUSTERS_PER_FILE) {
    uint64_t cluster_offset;
    unsigned int next;

    if (count >= capacity) {
      uint64_t *new_list;
      uint32_t new_cap;
      new_cap = capacity * 2;
      new_list = (uint64_t *)realloc(list, new_cap * sizeof(uint64_t));
      if (new_list == NULL) {
        free(list);
        return -1;
      }
      list = new_list;
      capacity = new_cap;
    }

    cluster_offset = cluster_block_base + (uint64_t)(cluster - 2) * ((uint64_t)1 << cluster_shift);
    list[count++] = cluster_offset;

    next = exfat_get_next_cluster(disk, partition, fat_offset, cluster);
    if (next == 0xFFFFFFFF || next == 0)
      break;
    cluster = next;
  }

  *clusters_out = list;
  *count_out = count;
  return 0;
}

#if defined(HAVE_LIBNTFS) || defined(HAVE_LIBNTFS3G)
static int ntfs_collect_clusters(ntfs_volume *vol, unsigned long int mft_ref, uint64_t **clusters_out,
                                 uint32_t *count_out) {
  ntfs_inode *inode;
  ntfs_attr *attr;
  uint32_t capacity;
  uint32_t count;
  uint64_t *list;
  s64 fsize;
  VCN vcn;
  unsigned int cluster_size;

  inode = ntfs_inode_open(vol, mft_ref);
  if (!inode)
    return -1;

  attr = ntfs_attr_open(inode, AT_DATA, NULL, 0);
  if (!attr) {
    ntfs_inode_close(inode);
    return -1;
  }

  fsize = attr->data_size;
  cluster_size = (unsigned int)vol->cluster_size;

  capacity = (fsize > 0) ? (uint32_t)((fsize + cluster_size - 1) / cluster_size) + 1 : 256;
  if (capacity > BACKUP_MAX_CLUSTERS_PER_FILE)
    capacity = (uint32_t)BACKUP_MAX_CLUSTERS_PER_FILE;
  if (capacity < 1)
    capacity = 256;
  list = (uint64_t *)MALLOC(capacity * sizeof(uint64_t));
  if (list == NULL) {
    ntfs_attr_close(attr);
    ntfs_inode_close(inode);
    return -1;
  }

  count = 0;
  vcn = 0;
  while (count < capacity && (s64)(vcn * cluster_size) < fsize) {
    s64 lcn;

    if (ntfs_attr_map_runlist(attr, vcn) != 0)
      break;
    lcn = ntfs_attr_vcn_to_lcn(attr, vcn);
    if (lcn < 0) {
      vcn++;
      continue;
    }
    list[count] = (uint64_t)lcn * cluster_size;
    count++;
    vcn++;
  }

  ntfs_attr_close(attr);
  ntfs_inode_close(inode);

  if (count == 0) {
    free(list);
    return -1;
  }

  *clusters_out = list;
  *count_out = count;
  return 0;
}
#endif

#if defined(HAVE_LIBEXT2FS)

static int ext2_read_blocks(ext2_filsys fs, blk64_t block, unsigned int indirect_level, uint64_t **list,
                            uint32_t *capacity, uint32_t *count) {
  unsigned int blk_size;
  unsigned char *buf;
  unsigned int nblocks_per_block;
  unsigned int i;

  blk_size = (unsigned int)fs->blocksize;
  nblocks_per_block = blk_size / sizeof(blk64_t);
  buf = (unsigned char *)MALLOC(blk_size);
  if (buf == NULL)
    return -1;

  if (io_channel_read_blk64(fs->io, block, 1, buf) != 0) {
    free(buf);
    return -1;
  }

  for (i = 0; i < nblocks_per_block; i++) {
    blk64_t child_block;
    blk64_t tmp64;
    unsigned int ofs;
    ofs = i * sizeof(blk64_t);
    memcpy(&tmp64, buf + ofs, sizeof(blk64_t));
    child_block = ext2fs_le64_to_cpu(tmp64);
    if (child_block == 0)
      continue;

    if (*count >= *capacity) {
      uint64_t *new_list;
      uint32_t new_cap;
      new_cap = *capacity * 2;
      new_list = (uint64_t *)realloc(*list, new_cap * sizeof(uint64_t));
      if (new_list == NULL) {
        free(buf);
        return -1;
      }
      *list = new_list;
      *capacity = new_cap;
    }

    if (indirect_level > 0) {
      if (ext2_read_blocks(fs, child_block, indirect_level - 1, list, capacity, count) != 0) {
        free(buf);
        return -1;
      }
    } else {
      (*list)[(*count)++] = (uint64_t)child_block * blk_size;
    }
  }

  free(buf);
  return 0;
}

static int ext2_collect_blocks(ext2_filsys fs, unsigned long int ino, uint64_t **clusters_out, uint32_t *count_out) {
  struct ext2_inode inode;
  uint32_t capacity;
  uint32_t count;
  uint64_t *list;
  unsigned int blk_size;
  unsigned int i;

  if (ext2fs_read_inode(fs, ino, &inode) != 0)
    return -1;

  blk_size = (unsigned int)fs->blocksize;
  capacity = 256;
  list = (uint64_t *)MALLOC(capacity * sizeof(uint64_t));
  if (list == NULL)
    return -1;
  count = 0;

  for (i = 0; i < EXT2_NDIR_BLOCKS; i++) {
    if (inode.i_block[i] == 0)
      continue;
    if (count >= capacity) {
      uint64_t *new_list;
      uint32_t new_cap;
      new_cap = capacity * 2;
      new_list = (uint64_t *)realloc(list, new_cap * sizeof(uint64_t));
      if (new_list == NULL) {
        free(list);
        return -1;
      }
      list = new_list;
      capacity = new_cap;
    }
    list[count++] = (uint64_t)inode.i_block[i] * blk_size;
  }

  if (inode.i_block[EXT2_IND_BLOCK] != 0) {
    if (ext2_read_blocks(fs, inode.i_block[EXT2_IND_BLOCK], 0, &list, &capacity, &count) != 0) {
      free(list);
      return -1;
    }
  }

  if (inode.i_block[EXT2_DIND_BLOCK] != 0) {
    if (ext2_read_blocks(fs, inode.i_block[EXT2_DIND_BLOCK], 1, &list, &capacity, &count) != 0) {
      free(list);
      return -1;
    }
  }

  if (inode.i_block[EXT2_TIND_BLOCK] != 0) {
    if (ext2_read_blocks(fs, inode.i_block[EXT2_TIND_BLOCK], 2, &list, &capacity, &count) != 0) {
      free(list);
      return -1;
    }
  }

  if (count == 0 && inode.i_blocks == 0) {
    free(list);
    return -1;
  }

  *clusters_out = list;
  *count_out = count;
  return 0;
}
#endif

static int write_backup_header(FILE *f, const pbackup_header_t *hdr, const char *model) {
  unsigned int model_len;
  unsigned int pad_len;
  unsigned int i;

  if (write_le32(f, hdr->magic) != 0)
    return -1;
  if (write_le32(f, hdr->version) != 0)
    return -1;
  if (write_le32(f, hdr->flags) != 0)
    return -1;
  if (write_le32(f, (uint32_t)hdr->fs_type) != 0)
    return -1;
  if (write_le64(f, hdr->disk_size) != 0)
    return -1;
  if (write_le32(f, hdr->sector_size) != 0)
    return -1;
  if (write_le64(f, hdr->part_offset) != 0)
    return -1;
  if (write_le64(f, hdr->part_size) != 0)
    return -1;
  if (write_le32(f, hdr->cluster_bytes) != 0)
    return -1;
  if (write_le64(f, hdr->data_offset) != 0)
    return -1;
  if (write_le64(f, hdr->created) != 0)
    return -1;
  if (write_le32(f, hdr->file_count) != 0)
    return -1;

  model_len = (unsigned int)strlen(model);
  if (model_len > 255)
    model_len = 255;
  if (write_le16(f, (uint16_t)model_len) != 0)
    return -1;
  if (write_le16(f, 0) != 0)
    return -1;

  if (model_len > 0) {
    if (fwrite(model, 1, model_len, f) != model_len)
      return -1;
  }

  pad_len = (unsigned int)((PBACKUP_HEADER_SIZE + model_len + 7) & ~7) - (PBACKUP_HEADER_SIZE + model_len);
  for (i = 0; i < pad_len; i++) {
    if (fputc(0, f) == EOF)
      return -1;
  }

  return 0;
}

static int write_backup_entry(FILE *f, const char *path, int is_dir, uint64_t file_size, time_t mtime, int no_clusters,
                              const uint64_t *cluster_list, uint32_t cluster_count) {
  uint16_t path_len;
  unsigned char flags;

  path_len = (uint16_t)strlen(path);
  flags = 0;
  if (is_dir)
    flags |= PBACKUP_FILE_FLAG_DIR;
  if (no_clusters)
    flags |= PBACKUP_FILE_FLAG_NO_CLUST;

  if (write_le16(f, path_len) != 0)
    return -1;
  if (fputc(flags, f) == EOF)
    return -1;
  if (write_le64(f, file_size) != 0)
    return -1;
  if (write_le64(f, (uint64_t)mtime) != 0)
    return -1;
  if (write_le32(f, cluster_count) != 0)
    return -1;
  if (write_le32(f, 0) != 0)
    return -1;

  if (path_len > 0) {
    if (fwrite(path, 1, path_len, f) != path_len)
      return -1;
  }

  if (!no_clusters && cluster_count > 0 && cluster_list != NULL) {
    uint32_t j;
    for (j = 0; j < cluster_count; j++) {
      if (write_le64(f, cluster_list[j]) != 0)
        return -1;
    }
  }

  return 0;
}

typedef struct {
  char *path;
  unsigned char flags;
  uint64_t file_size;
  time_t mtime;
  uint32_t cluster_count;
  uint64_t *cluster_list;
  struct td_list_head list;
} backup_entry_t;

static void free_backup_entries(struct td_list_head *head) {
  struct td_list_head *pos, *tmp;
  td_list_for_each_safe(pos, tmp, head) {
    backup_entry_t *be = td_list_entry(pos, backup_entry_t, list);
    if (be->cluster_list)
      free(be->cluster_list);
    if (be->path)
      free(be->path);
    td_list_del(&be->list);
    free(be);
  }
}

static int compare_path(const void *a, const void *b) {
  const backup_entry_t *ea;
  const backup_entry_t *eb;
  ea = *(const backup_entry_t *const *)a;
  eb = *(const backup_entry_t *const *)b;
  return strcmp(ea->path, eb->path);
}

static int collect_clusters_fs(disk_t *disk, const partition_t *partition, dir_data_t *dir_data, const file_info_t *fi,
                               uint64_t **clusters_out, uint32_t *count_out, int *no_clusters) {
  int fs_type;

  *clusters_out = NULL;
  *count_out = 0;
  *no_clusters = 0;

  if (is_part_fat(partition))
    fs_type = 1;
  else if (partition->upart_type >= UP_FAT12 && partition->upart_type <= UP_FAT32)
    fs_type = 1;
  else if (is_part_ntfs(partition) || partition->upart_type == UP_NTFS)
    fs_type = 2;
  else if (partition->upart_type == UP_EXFAT)
    fs_type = 3;
  else if (partition->upart_type >= UP_EXT2 && partition->upart_type <= UP_EXT4)
    fs_type = 4;
  else
    fs_type = 0;

  log_info("collect_clusters_fs: upart_type=%d fs_type=%d\n", (int)partition->upart_type, fs_type);

  switch (fs_type) {
  case 1: {
    struct fat_dir_struct *fls;
    int clust_ret;
    fls = (struct fat_dir_struct *)dir_data->private_dir_data;
    if (fls && fls->boot_sector) {
      clust_ret = fat_collect_clusters(disk, partition, fls->boot_sector, fi->st_ino, clusters_out, count_out);
      log_info("collect_clusters fat: ino=%lu ret=%d count=%u\n", (unsigned long)fi->st_ino, clust_ret, *count_out);
      return clust_ret;
    }
    log_info("collect_clusters fat: no boot_sector\n");
    break;
  }
  case 2:
#if defined(HAVE_LIBNTFS) || defined(HAVE_LIBNTFS3G)
  {
    struct ntfs_dir_struct *nls;
    int clust_ret;
    nls = (struct ntfs_dir_struct *)dir_data->private_dir_data;
    if (nls && nls->vol) {
      clust_ret = ntfs_collect_clusters(nls->vol, fi->st_ino, clusters_out, count_out);
      log_info("collect_clusters ntfs: mft=%lu ret=%d count=%u\n", (unsigned long)fi->st_ino, clust_ret, *count_out);
      return clust_ret;
    }
    log_info("collect_clusters ntfs: no vol\n");
  }
#endif
  break;
  case 3: {
    struct exfat_dir_struct *els;
    int clust_ret;
    els = (struct exfat_dir_struct *)dir_data->private_dir_data;
    if (els && els->boot_sector) {
      clust_ret = exfat_collect_clusters(disk, partition, els->boot_sector, fi->st_ino, clusters_out, count_out);
      log_info("collect_clusters exfat: ino=%lu ret=%d count=%u\n", (unsigned long)fi->st_ino, clust_ret, *count_out);
      return clust_ret;
    }
    log_info("collect_clusters exfat: no boot_sector\n");
    break;
  }
  case 4:
#if defined(HAVE_LIBEXT2FS)
  {
    struct ext2_dir_struct *exls;
    int clust_ret;
    exls = (struct ext2_dir_struct *)dir_data->private_dir_data;
    if (exls && exls->current_fs) {
      clust_ret = ext2_collect_blocks(exls->current_fs, fi->st_ino, clusters_out, count_out);
      log_info("collect_clusters ext2: ino=%lu ret=%d count=%u\n", (unsigned long)fi->st_ino, clust_ret, *count_out);
      return clust_ret;
    }
    log_info("collect_clusters ext2: no current_fs\n");
  }
#endif
  break;
  default:
    log_info("collect_clusters: unhandled fs_type=%d for ino=%lu\n", fs_type, (unsigned long)fi->st_ino);
    break;
  }

  log_info("collect_clusters: falling back to no_clusters\n");
  *no_clusters = 1;
  return -1;
}

static int backup_walk_dir(scan_tree_t *tree, disk_t *disk, const partition_t *partition, dir_data_t *dir_data,
                           unsigned long int inode, const char *path, unsigned int depth,
                           unsigned long int inode_known[], FILE *f_out, uint32_t *written_count,
                           const char *root_model) {
  file_info_t dir_list;
  struct td_list_head *file_walker;

  (void)tree;
  (void)root_model;

  if (depth >= 256)
    return 0;

  TD_INIT_LIST_HEAD(&dir_list.list);
  if (dir_data->get_dir(disk, partition, dir_data, inode, &dir_list) != 0)
    return -1;

  inode_known[depth] = inode;

  td_list_for_each(file_walker, &dir_list.list) {
    const file_info_t *fi;
    int is_dir;
    int deleted_flag;
    char full_path[PBACKUP_MAX_PATH];
    int no_clusters;
    uint64_t *cluster_list;
    uint32_t cluster_count;

    fi = td_list_entry_const(file_walker, const file_info_t, list);
    deleted_flag = (fi->status & FILE_STATUS_DELETED) != 0;
    is_dir = LINUX_S_ISDIR(fi->st_mode) != 0;

    if (is_dir && fi->name[0] == '.' && (fi->name[1] == '\0' || (fi->name[1] == '.' && fi->name[2] == '\0')))
      continue;

    if (deleted_flag)
      continue;

    if (strcmp(path, "/") == 0)
      snprintf(full_path, sizeof(full_path), "/%s", fi->name);
    else
      snprintf(full_path, sizeof(full_path), "%s/%s", path, fi->name);

    g_backup_processed += fi->st_size;
    g_backup_extra++;

    if (!is_dir) {
      int clust_ret;
      clust_ret = collect_clusters_fs(disk, partition, dir_data, fi, &cluster_list, &cluster_count, &no_clusters);
      if (clust_ret != 0 && !no_clusters) {
        log_warning("backup_walk_dir: cluster collect failed for %s (size=%llu)\n", full_path,
                    (unsigned long long)fi->st_size);
        no_clusters = 1;
        cluster_list = NULL;
        cluster_count = 0;
      }

      write_backup_entry(f_out, full_path, 0, fi->st_size, fi->td_mtime, no_clusters, cluster_list, cluster_count);
      (*written_count)++;

      if (cluster_list)
        free(cluster_list);
    } else {
      int valid_inode;
      unsigned int k;
      valid_inode = 1;
      for (k = 0; k < depth + 1; k++) {
        if (inode_known[k] == fi->st_ino) {
          valid_inode = 0;
          break;
        }
      }
      write_backup_entry(f_out, full_path, 1, 0, 0, 1, NULL, 0);
      (*written_count)++;

      if (valid_inode) {
        backup_walk_dir(tree, disk, partition, dir_data, fi->st_ino, full_path, depth + 1, inode_known, f_out,
                        written_count, root_model);
      }
    }
  }

  delete_list_file(&dir_list);
  return 0;
}

static dir_partition_t backup_init_fs(disk_t *disk, const partition_t *partition, dir_data_t *dir_data) {
  dir_partition_t result;
  log_info("backup_init_fs: upart_type=%d, part_type_i386=0x%02x\n", (int)partition->upart_type,
           partition->part_type_i386);

  if (is_part_fat(partition)) {
    log_info("backup_init_fs: matched FAT via partition type code, "
             "trying fat_init\n");
    result = dir_partition_fat_init(disk, partition, dir_data, 0);
    log_info("backup_init_fs: fat_init returned %d\n", (int)result);
    return result;
  }
  if (is_ntfs(partition)) {
    log_info("backup_init_fs: matched NTFS via is_ntfs(), "
             "trying ntfs_init\n");
    result = dir_partition_ntfs_init(disk, partition, dir_data, 0, 0);
    log_info("backup_init_fs: ntfs_init returned %d\n", (int)result);
    return result;
  }
  switch (partition->upart_type) {
  case UP_FAT12:
  case UP_FAT16:
  case UP_FAT32:
    log_info("backup_init_fs: matched FAT via upart_type, "
             "trying fat_init\n");
    result = dir_partition_fat_init(disk, partition, dir_data, 0);
    log_info("backup_init_fs: fat_init returned %d\n", (int)result);
    return result;
  case UP_EXT2:
  case UP_EXT3:
  case UP_EXT4:
    log_info("backup_init_fs: matched ext via upart_type, "
             "trying ext2_init\n");
    result = dir_partition_ext2_init(disk, partition, dir_data, 0);
    log_info("backup_init_fs: ext2_init returned %d\n", (int)result);
    return result;
  case UP_RFS:
  case UP_RFS2:
  case UP_RFS3:
    log_info("backup_init_fs: matched RFS via upart_type, "
             "trying reiser_init\n");
    result = dir_partition_reiser_init(disk, partition, dir_data, 0);
    log_info("backup_init_fs: reiser_init returned %d\n", (int)result);
    return result;
  case UP_NTFS:
    log_info("backup_init_fs: matched NTFS via upart_type, "
             "trying ntfs_init\n");
    result = dir_partition_ntfs_init(disk, partition, dir_data, 0, 0);
    log_info("backup_init_fs: ntfs_init returned %d\n", (int)result);
    return result;
  case UP_EXFAT:
    log_info("backup_init_fs: matched exFAT via upart_type, "
             "trying exfat_init\n");
    result = dir_partition_exfat_init(disk, partition, dir_data, 0);
    log_info("backup_init_fs: exfat_init returned %d\n", (int)result);
    return result;
  default:
    break;
  }

  log_info("backup_init_fs: no filesystem type matched "
           "(upart_type=%d, part_type_i386=0x%02x)\n",
           (int)partition->upart_type, partition->part_type_i386);
  return DIR_PART_ENOIMP;
}

static uint32_t count_live_files(dir_data_t *dir_data, disk_t *disk, const partition_t *partition,
                                 unsigned long int inode, const char *path, unsigned int depth,
                                 unsigned long int inode_known[], uint64_t *total_size) {
  file_info_t dir_list;
  struct td_list_head *file_walker;
  uint32_t count;

  if (depth >= 256)
    return 0;

  TD_INIT_LIST_HEAD(&dir_list.list);
  if (dir_data->get_dir(disk, partition, dir_data, inode, &dir_list) != 0)
    return 0;

  inode_known[depth] = inode;
  count = 0;

  td_list_for_each(file_walker, &dir_list.list) {
    const file_info_t *fi;
    int is_dir;
    int deleted_flag;

    fi = td_list_entry_const(file_walker, const file_info_t, list);
    deleted_flag = (fi->status & FILE_STATUS_DELETED) != 0;
    is_dir = LINUX_S_ISDIR(fi->st_mode) != 0;

    if (is_dir && fi->name[0] == '.' && (fi->name[1] == '\0' || (fi->name[1] == '.' && fi->name[2] == '\0')))
      continue;

    if (deleted_flag)
      continue;

    count++;

    if (!is_dir)
      *total_size += fi->st_size;

    g_backup_processed++;

    if (is_dir) {
      int valid_inode;
      unsigned int k;
      valid_inode = 1;
      for (k = 0; k < depth + 1; k++) {
        if (inode_known[k] == fi->st_ino) {
          valid_inode = 0;
          break;
        }
      }
      if (valid_inode) {
        char next_path[PBACKUP_MAX_PATH];
        if (strcmp(path, "/") == 0)
          snprintf(next_path, sizeof(next_path), "/%s", fi->name);
        else
          snprintf(next_path, sizeof(next_path), "%s/%s", path, fi->name);
        count += count_live_files(dir_data, disk, partition, fi->st_ino, next_path, depth + 1, inode_known, total_size);
      }
    }
  }

  delete_list_file(&dir_list);
  return count;
}

static int backup_walk_live(disk_t *disk, const partition_t *partition, dir_data_t *dir_data, unsigned long int inode,
                            const char *path, unsigned int depth, unsigned long int inode_known[],
                            backup_entry_t **path_array, uint32_t *path_count, uint32_t path_capacity) {
  file_info_t dir_list;
  struct td_list_head *file_walker;

  if (depth >= 256)
    return 0;

  TD_INIT_LIST_HEAD(&dir_list.list);
  if (dir_data->get_dir(disk, partition, dir_data, inode, &dir_list) != 0)
    return -1;

  inode_known[depth] = inode;

  td_list_for_each(file_walker, &dir_list.list) {
    const file_info_t *fi;
    int is_dir;
    int deleted_flag;
    char full_path[PBACKUP_MAX_PATH];
    backup_entry_t *be;

    fi = td_list_entry_const(file_walker, const file_info_t, list);
    deleted_flag = (fi->status & FILE_STATUS_DELETED) != 0;
    is_dir = LINUX_S_ISDIR(fi->st_mode) != 0;

    if (is_dir && fi->name[0] == '.' && (fi->name[1] == '\0' || (fi->name[1] == '.' && fi->name[2] == '\0')))
      continue;

    if (deleted_flag)
      continue;

    if (strcmp(path, "/") == 0)
      snprintf(full_path, sizeof(full_path), "/%s", fi->name);
    else
      snprintf(full_path, sizeof(full_path), "%s/%s", path, fi->name);

    if (*path_count >= path_capacity)
      return -1;

    be = (backup_entry_t *)MALLOC(sizeof(backup_entry_t));
    if (be == NULL)
      return -1;
    memset(be, 0, sizeof(backup_entry_t));
    be->path = strdup(full_path);
    if (be->path == NULL) {
      free(be);
      return -1;
    }
    be->flags = is_dir ? PBACKUP_FILE_FLAG_DIR : 0;
    be->file_size = fi->st_size;
    be->mtime = fi->td_mtime;
    be->cluster_count = 0;
    be->cluster_list = NULL;
    path_array[(*path_count)++] = be;

    g_backup_processed++;

    if (is_dir) {
      int valid_inode;
      unsigned int k;
      valid_inode = 1;
      for (k = 0; k < depth + 1; k++) {
        if (inode_known[k] == fi->st_ino) {
          valid_inode = 0;
          break;
        }
      }
      if (valid_inode) {
        int child_ret;
        child_ret = backup_walk_live(disk, partition, dir_data, fi->st_ino, full_path, depth + 1, inode_known,
                                     path_array, path_count, path_capacity);
        if (child_ret != 0)
          return -1;
      }
    }
  }

  delete_list_file(&dir_list);
  return 0;
}

static backup_entry_t *find_live_entry(backup_entry_t **sorted_array, uint32_t count, const char *path) {
  uint32_t lo, hi;
  if (path == NULL || count == 0)
    return NULL;
  lo = 0;
  hi = count;
  while (lo < hi) {
    uint32_t mid;
    int cmp;
    mid = lo + (hi - lo) / 2;
    cmp = strcmp(path, sorted_array[mid]->path);
    if (cmp == 0)
      return sorted_array[mid];
    else if (cmp < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  return NULL;
}

static int backup_restore_build_tree(scan_tree_t *tree, struct td_list_head *backup_entries,
                                     backup_entry_t **live_sorted, uint32_t live_count, uint32_t backup_total,
                                     unsigned int sector_size, unsigned int cluster_bytes) {
  struct td_list_head *pos, *tmp;
  uint32_t processed;
  uint32_t found_deleted;
  uint32_t found_modified;

  processed = 0;
  found_deleted = 0;
  found_modified = 0;

  td_list_for_each_safe(pos, tmp, backup_entries) {
    backup_entry_t *be;
    backup_entry_t *live;
    int is_dir;
    int is_deleted;
    int is_modified;

    be = td_list_entry(pos, backup_entry_t, list);
    is_dir = (be->flags & PBACKUP_FILE_FLAG_DIR) != 0;
    live = find_live_entry(live_sorted, live_count, be->path);

    if (live == NULL) {
      is_deleted = 1;
      is_modified = 0;
      found_deleted++;
    } else if (live->file_size != be->file_size || live->mtime != be->mtime) {
      is_deleted = 0;
      is_modified = 1;
      found_modified++;
    } else {
      is_deleted = 0;
      is_modified = 0;
    }

    processed++;

    if (is_deleted || is_modified) {
      uint64_t num_sectors;
      file_node_t *node;
      uint32_t i;
      int no_cl;

      no_cl = (be->flags & PBACKUP_FILE_FLAG_NO_CLUST) != 0;
      num_sectors = be->file_size > 0 ? (be->file_size + sector_size - 1) / sector_size : 0;

      node = tree_add_path(tree, be->path, is_dir, be->file_size, 0, num_sectors, be->mtime, sector_size, is_deleted);
      if (node != NULL) {
        node->backup_restored = 1;
        node->backup_modified = is_modified && !is_deleted;
        if (!is_dir && !no_cl && be->cluster_count > 0 && be->cluster_list != NULL) {
          node->cluster_list = (uint64_t *)MALLOC(be->cluster_count * sizeof(uint64_t));
          if (node->cluster_list != NULL) {
            node->cluster_count = be->cluster_count;
            node->cluster_size = cluster_bytes;
            for (i = 0; i < be->cluster_count; i++)
              node->cluster_list[i] = be->cluster_list[i];
          } else {
            node->cluster_count = 0;
            node->cluster_size = 0;
          }
        }
      }
    }

    if (be->cluster_list)
      free(be->cluster_list);
    if (be->path)
      free(be->path);
    td_list_del(&be->list);
    free(be);
  }

  log_info("backup_restore_build_tree: %u entries processed (%u deleted, %u modified)\n", processed, found_deleted,
           found_modified);

  return 0;
}

int backup_create(disk_t *disk, const partition_t *partition, const char *dest_dir) {
  dir_data_t dir_data;
  pbackup_header_t hdr;
  uint32_t written_count;
  FILE *f_out;
  char file_path[PBACKUP_MAX_PATH];
  unsigned long int inode_known[256];
  char model[256];
  const char *model_str;
  time_t now;
  const char *fsname;
  unsigned int cluster_bytes;
  uint64_t data_offset;

  memset(&dir_data, 0, sizeof(dir_data));
  memset(inode_known, 0, sizeof(inode_known));

  {
    dir_partition_t init_res;
    const char *fs_label;
    init_res = backup_init_fs(disk, partition, &dir_data);
    if (init_res != DIR_PART_OK) {
      if (init_res == DIR_PART_EIO) {
        const char *label;
        label = "volume";
        if (is_part_fat(partition))
          label = "FAT volume";
        else if (is_ntfs(partition))
          label = "NTFS volume";
        else if (partition->upart_type == UP_EXFAT)
          label = "exFAT volume";
        else if (partition->upart_type >= UP_EXT2 && partition->upart_type <= UP_EXT4)
          label = "ext volume";
        log_error("backup_create: I/O error opening %s\n", label);
      } else if (init_res == DIR_PART_ENOSYS) {
        log_error("backup_create: filesystem library not available\n");
      } else {
        fs_label = "";
        if (partition->fsname[0] != '\0')
          fs_label = partition->fsname;
        else if (partition->upart_type == UP_UNK)
          fs_label = "Unknown";
        log_error("backup_create: unsupported filesystem "
                  "(upart_type=%d, part_type_i386=0x%02x, "
                  "fsname=\"%s\")\n",
                  (int)partition->upart_type, partition->part_type_i386, fs_label);
      }
      return -1;
    }
  }

  now = time(NULL);
  snprintf(file_path, sizeof(file_path), "%s/backup_%lld.dsk", dest_dir, (long long)now);

  f_out = fopen(file_path, "wb");
  if (f_out == NULL) {
    log_error("backup_create: cannot create %s: %s\n", file_path, strerror(errno));
    if (dir_data.close)
      dir_data.close(&dir_data);
    return -1;
  }

  model_str = disk->model ? disk->model : "";
  strncpy(model, model_str, sizeof(model) - 1);
  model[sizeof(model) - 1] = '\0';

  cluster_bytes = 0;
  data_offset = 0;
  fsname = "";

  if (is_part_fat(partition)) {
    struct fat_dir_struct *fls;
    fls = (struct fat_dir_struct *)dir_data.private_dir_data;
    if (fls && fls->boot_sector) {
      unsigned int sector_size;
      unsigned int sectors_per_cluster;
      unsigned long int fat_length;
      sector_size = fat_sector_size(fls->boot_sector);
      sectors_per_cluster = fls->boot_sector->sectors_per_cluster;
      fat_length = le16(fls->boot_sector->fat_length) > 0 ? le16(fls->boot_sector->fat_length)
                                                          : le32(fls->boot_sector->fat32_length);
      cluster_bytes = sectors_per_cluster * sector_size;
      data_offset = (uint64_t)(le16(fls->boot_sector->reserved) + fls->boot_sector->fats * fat_length) * sector_size;
      fsname = "FAT";
    }
  }
#if defined(HAVE_LIBNTFS) || defined(HAVE_LIBNTFS3G)
  if (cluster_bytes == 0 && (is_part_ntfs(partition) || partition->upart_type == UP_NTFS)) {
    struct ntfs_dir_struct *nls;
    nls = (struct ntfs_dir_struct *)dir_data.private_dir_data;
    if (nls && nls->vol) {
      cluster_bytes = (unsigned int)nls->vol->cluster_size;
      data_offset = 0;
      fsname = "NTFS";
    }
  }
#endif
  if (cluster_bytes == 0 && partition->upart_type == UP_EXFAT) {
    struct exfat_dir_struct *els;
    els = (struct exfat_dir_struct *)dir_data.private_dir_data;
    if (els && els->boot_sector) {
      cluster_bytes = (unsigned int)(1 << (els->boot_sector->block_per_clus_bits + els->boot_sector->blocksize_bits));
      data_offset = (uint64_t)le32(els->boot_sector->clus_blocknr) << els->boot_sector->blocksize_bits;
      fsname = "exFAT";
    }
  }
#if defined(HAVE_LIBEXT2FS)
  if (cluster_bytes == 0 && partition->upart_type >= UP_EXT2 && partition->upart_type <= UP_EXT4) {
    struct ext2_dir_struct *exls;
    exls = (struct ext2_dir_struct *)dir_data.private_dir_data;
    if (exls && exls->current_fs) {
      cluster_bytes = (unsigned int)exls->current_fs->blocksize;
      data_offset = 0;
      fsname = "ext";
    }
  }
#endif

  log_info("backup_create: fs=%s cluster_bytes=%u data_offset=%llu\n", fsname[0] != '\0' ? fsname : "unknown",
           cluster_bytes, (unsigned long long)data_offset);

  memset(&hdr, 0, sizeof(hdr));
  hdr.magic = PBACKUP_MAGIC;
  hdr.version = PBACKUP_VERSION;
  hdr.flags = 0;
  hdr.fs_type = (int32_t)partition->upart_type;
  hdr.disk_size = disk->disk_size;
  hdr.sector_size = disk->sector_size;
  hdr.part_offset = partition->part_offset;
  hdr.part_size = partition->part_size;
  hdr.cluster_bytes = cluster_bytes;
  hdr.data_offset = data_offset;
  hdr.created = (uint64_t)now;
  hdr.file_count = 0;

  if (write_backup_header(f_out, &hdr, model) != 0) {
    log_error("backup_create: failed to write header\n");
    fclose(f_out);
    if (dir_data.close)
      dir_data.close(&dir_data);
    return -1;
  }

  g_backup_processed = 0;
  g_backup_total = 0;
  g_last_update = 0;
  g_backup_extra = 0;
  written_count = 0;
  write_backup_entry(f_out, "/", 1, 0, 0, 1, NULL, 0);
  written_count++;

  {
    int ret;
    unsigned long int walk_inodes[256];
    memset(walk_inodes, 0, sizeof(walk_inodes));
    ret = backup_walk_dir(NULL, disk, partition, &dir_data, dir_data.current_inode, "/", 0, walk_inodes, f_out,
                          &written_count, model);
    if (ret != 0) {
      log_error("backup_create: walk failed\n");
      fclose(f_out);
      remove(file_path);
      if (dir_data.close)
        dir_data.close(&dir_data);
      return -1;
    }
  }

  if (written_count <= 1) {
    log_error("backup_create: no files indexed (written_count=%u)\n", (unsigned int)written_count);
    fclose(f_out);
    remove(file_path);
    if (dir_data.close)
      dir_data.close(&dir_data);
    return -1;
  }

  {
    uint32_t fc;
    long pos;
    pos = ftell(f_out);
    fc = written_count;
    fseek(f_out, 64, SEEK_SET);
    write_le32(f_out, fc);
    fseek(f_out, pos, SEEK_SET);
  }

  fclose(f_out);

  if (dir_data.close)
    dir_data.close(&dir_data);

  log_info("backup_create: wrote %u entries to %s\n", (unsigned int)written_count, file_path);

  return 0;
}

int backup_restore(scan_tree_t *tree, disk_t *disk, const partition_t *partition, const char *path) {
  FILE *f_in;
  pbackup_header_t hdr;
  unsigned char header_buf[PBACKUP_HEADER_SIZE + 1024];
  struct td_list_head backup_entries;
  dir_data_t dir_data;
  uint32_t backup_total;
  uint32_t i;
  unsigned long int inode_known[256];
  uint32_t live_count;
  uint32_t live_capacity;
  backup_entry_t **live_array;
  int ret;

  f_in = fopen(path, "rb");
  log_info("backup_restore: opening %s\n", path);
  if (f_in == NULL) {
    log_error("backup_restore: cannot open %s\n", path);
    return -1;
  }

  memset(header_buf, 0, sizeof(header_buf));
  {
    size_t nread;
    nread = fread(header_buf, 1, PBACKUP_HEADER_SIZE, f_in);
    if (nread < PBACKUP_HEADER_SIZE) {
      log_error("backup_restore: %s is too short "
                "(read %zu of %d bytes)\n",
                path, nread, PBACKUP_HEADER_SIZE);
      fclose(f_in);
      return -1;
    }

    hdr.magic = read_le32(header_buf + 0);
    hdr.version = read_le32(header_buf + 4);
    hdr.flags = read_le32(header_buf + 8);
    hdr.fs_type = (int32_t)read_le32(header_buf + 12);
    hdr.disk_size = read_le64(header_buf + 16);
    hdr.sector_size = read_le32(header_buf + 24);
    hdr.part_offset = read_le64(header_buf + 28);
    hdr.part_size = read_le64(header_buf + 36);
    hdr.cluster_bytes = read_le32(header_buf + 44);
    hdr.data_offset = read_le64(header_buf + 48);
    hdr.created = read_le64(header_buf + 56);
    hdr.file_count = read_le32(header_buf + 64);
    hdr.model_len = read_le16(header_buf + 68);

    if (hdr.magic != PBACKUP_MAGIC) {
      fclose(f_in);
      return -1;
    }

    if (hdr.version != PBACKUP_VERSION) {
      fclose(f_in);
      return -1;
    }

    {
      int64_t size_diff;
      uint64_t orig_size;
      orig_size = (hdr.part_offset == 0) ? hdr.disk_size : hdr.part_size;
      if (orig_size == 0)
        orig_size = hdr.disk_size;
      size_diff = (int64_t)disk->disk_size - (int64_t)orig_size;
      if (size_diff < 0)
        size_diff = -size_diff;
      if (orig_size > 0 && (uint64_t)size_diff > orig_size / 2) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Disk size mismatch.\nBackup: %llu MB, Current: %llu MB",
                 (unsigned long long)(orig_size / (1024 * 1024)),
                 (unsigned long long)(disk->disk_size / (1024 * 1024)));
        fclose(f_in);
        return -1;
      }
    }
  }

  log_info("backup_restore: header OK, disk_size=%llu part_offset=%llu file_count=%u\n",
           (unsigned long long)hdr.disk_size, (unsigned long long)hdr.part_offset, hdr.file_count);

  {
    unsigned int model_len;
    unsigned int pad_len;
    unsigned int header_total;
    model_len = hdr.model_len;
    if (model_len > 512)
      model_len = 512;
    header_total = PBACKUP_HEADER_SIZE + model_len;
    pad_len = (unsigned int)((header_total + 7) & ~7) - header_total;
    if (header_total < sizeof(header_buf)) {
      fseek(f_in, PBACKUP_HEADER_SIZE, SEEK_SET);
      if (fread(header_buf + PBACKUP_HEADER_SIZE, 1, header_total - PBACKUP_HEADER_SIZE, f_in) == 0) {
      }
    }
    fseek(f_in, (long)header_total + (long)pad_len, SEEK_SET);
  }

  TD_INIT_LIST_HEAD(&backup_entries);

  backup_total = hdr.file_count;
  log_info("backup_restore: loading %u entries from .dsk\n", backup_total);
  g_backup_processed = 0;
  g_backup_total = (uint64_t)backup_total;
  for (i = 0; i < backup_total; i++) {
    backup_entry_t *be;
    unsigned char entry_buf[32];
    unsigned char flags;
    uint16_t path_len;
    uint32_t cluster_count;
    unsigned int j;

    if (i % 500 == 0) {
    }

    if (fread(entry_buf, 1, 27, f_in) != 27)
      break;

    path_len = read_le16(entry_buf);
    flags = entry_buf[2];

    be = (backup_entry_t *)MALLOC(sizeof(backup_entry_t));
    if (be == NULL)
      break;
    memset(be, 0, sizeof(backup_entry_t));

    be->flags = flags;
    be->file_size = read_le64(entry_buf + 3);
    be->mtime = (time_t)read_le64(entry_buf + 11);
    be->cluster_count = read_le32(entry_buf + 19);

    if (path_len > PBACKUP_MAX_PATH - 1)
      path_len = PBACKUP_MAX_PATH - 1;
    if (path_len > 0) {
      be->path = (char *)MALLOC(path_len + 1);
      if (be->path == NULL) {
        free(be);
        break;
      }
      if (fread(be->path, 1, path_len, f_in) != path_len) {
        free(be->path);
        free(be);
        break;
      }
      be->path[path_len] = '\0';
    } else {
      be->path = strdup("");
    }

    cluster_count = be->cluster_count;
    if (cluster_count > 0 && !(flags & PBACKUP_FILE_FLAG_NO_CLUST)) {
      uint64_t *clist;
      int read_ok = 1;
      clist = (uint64_t *)MALLOC(cluster_count * sizeof(uint64_t));
      if (clist == NULL) {
        free(be->path);
        free(be);
        break;
      }
      for (j = 0; j < cluster_count; j++) {
        unsigned char cl_buf[8];
        if (fread(cl_buf, 1, 8, f_in) != 8) {
          read_ok = 0;
          break;
        }
        clist[j] = read_le64(cl_buf);
      }
      if (!read_ok) {
        free(clist);
        free(be->path);
        free(be);
        break;
      }
      be->cluster_list = clist;
      be->cluster_count = cluster_count;
    } else {
      be->cluster_list = NULL;
      be->cluster_count = 0;
    }

    td_list_add_tail(&be->list, &backup_entries);
  }

  fclose(f_in);

  log_info("backup_restore: loaded %u entries from %s\n", backup_total, path);

  memset(&dir_data, 0, sizeof(dir_data));
  {
    dir_partition_t init_res;
    init_res = backup_init_fs(disk, partition, &dir_data);
    if (init_res != DIR_PART_OK) {
      if (init_res == DIR_PART_EIO) {
        log_error("backup_restore: I/O error opening "
                  "live filesystem\n");
      } else if (init_res == DIR_PART_ENOSYS) {
        log_error("backup_restore: filesystem library "
                  "not available\n");
      } else {
        log_error("backup_restore: cannot open live "
                  "filesystem\n");
      }
      free_backup_entries(&backup_entries);
      return -1;
    }
  }

  g_backup_processed = 0;
  g_backup_total = 1;
  {
    unsigned long int count_inodes[256];
    uint64_t dummy_size;
    memset(count_inodes, 0, sizeof(count_inodes));
    dummy_size = 0;
    live_capacity =
        count_live_files(&dir_data, disk, partition, dir_data.current_inode, "/", 0, count_inodes, &dummy_size);
  }
  live_capacity += live_capacity / 2 + backup_total + 10000;
  live_array = (backup_entry_t **)MALLOC(live_capacity * sizeof(backup_entry_t *));
  if (live_array == NULL) {
    free_backup_entries(&backup_entries);
    if (dir_data.close)
      dir_data.close(&dir_data);
    return -1;
  }

  live_count = 0;
  memset(inode_known, 0, sizeof(inode_known));
  g_backup_processed = 0;
  g_backup_total = live_capacity;
  log_info("backup_restore: walking live FS, capacity=%u\n", live_capacity);
  ret = backup_walk_live(disk, partition, &dir_data, dir_data.current_inode, "/", 0, inode_known, live_array,
                         &live_count, live_capacity);
  if (dir_data.close)
    dir_data.close(&dir_data);

  if (ret != 0) {
    uint32_t fi;
    for (fi = 0; fi < live_count; fi++) {
      if (live_array[fi]->path)
        free(live_array[fi]->path);
      free(live_array[fi]);
    }
    free(live_array);
    free_backup_entries(&backup_entries);
    return -1;
  }

  {
    backup_entry_t **sorted;
    sorted = (backup_entry_t **)MALLOC(live_count * sizeof(backup_entry_t *));
    if (sorted != NULL) {
      uint32_t si;
      for (si = 0; si < live_count; si++)
        sorted[si] = live_array[si];
      qsort(sorted, live_count, sizeof(backup_entry_t *), compare_path);
      log_info("backup_restore: building tree, live=%u backup=%u\n", live_count, backup_total);
      backup_restore_build_tree(tree, &backup_entries, sorted, live_count, backup_total, disk->sector_size,
                                hdr.cluster_bytes);
      free(sorted);
    }

    {
      uint32_t fi;
      for (fi = 0; fi < live_count; fi++) {
        if (live_array[fi]->path)
          free(live_array[fi]->path);
        free(live_array[fi]);
      }
    }
    free(live_array);
  }

  return 0;
}
