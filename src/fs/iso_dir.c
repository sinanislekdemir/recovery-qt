/*

    File: iso_dir.c

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include <errno.h>
#include "types.h"
#include "common.h"
#include "iso9660.h"
#include "lang.h"
#include "intrf.h"
#include "dir.h"
#include "iso_dir.h"
#include "log.h"
#include "setdate.h"

#define ISO_BLOCK_SIZE_DFLT 2048
#define ISO_PVD_LBA 16
#define ISO_MAX_DIR_SIZE (16*1024*1024)
#define ISO_FLAG_HIDDEN 0x01
#define ISO_FLAG_DIR 0x02
#define ISO_FLAG_MULTI 0x80

static int iso_dir(disk_t *disk, const partition_t *partition, dir_data_t *dir_data, const unsigned long int first_inode, file_info_t *dir_list);
static copy_file_t iso_copy(disk_t *disk, const partition_t *partition, dir_data_t *dir_data, const file_info_t *file);
static void dir_partition_iso_close(dir_data_t *dir_data);

static time_t iso_date_to_unix(const unsigned char *p)
{
  const int year = 1900 + p[0];
  const unsigned int mon = p[1];
  const unsigned int day = p[2];
  int y;
  long era;
  unsigned int yoe;
  unsigned int doy;
  unsigned int doe;
  long days;
  time_t t;
  if(mon < 1 || mon > 12 || day < 1 || day > 31)
    return (time_t)0;
  y = (mon <= 2 ? year - 1 : year);
  era = (y >= 0 ? y : y - 399) / 400;
  yoe = (unsigned int)(y - era * 400);
  doy = (153 * (mon + (mon > 2 ? (unsigned int)-3 : 9)) + 2) / 5 + day - 1;
  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  days = era * 146097L + (long)doe - 719468L;
  t = (time_t)days * 86400 + (time_t)p[3] * 3600 + (time_t)p[4] * 60 + p[5];
  t -= (time_t)((signed char)p[6]) * 15 * 60;
  return t;
}

/* Convert an UCS-2 big-endian Joliet identifier to UTF-8 */
static void iso_ucs2be_to_utf8(char *dst, const unsigned int dst_size, const unsigned char *src, const unsigned int src_len)
{
  unsigned int i;
  unsigned int j = 0;
  for(i = 0; i + 1 < src_len; i += 2)
  {
    const unsigned int wc = ((unsigned int)src[i] << 8) | src[i+1];
    if(wc == 0)
      break;
    if(wc < 0x80)
    {
      if(j + 1 >= dst_size)
	break;
      dst[j++] = (char)wc;
    }
    else if(wc < 0x800)
    {
      if(j + 2 >= dst_size)
	break;
      dst[j++] = (char)(0xc0 | (wc >> 6));
      dst[j++] = (char)(0x80 | (wc & 0x3f));
    }
    else
    {
      if(j + 3 >= dst_size)
	break;
      dst[j++] = (char)(0xe0 | (wc >> 12));
      dst[j++] = (char)(0x80 | ((wc >> 6) & 0x3f));
      dst[j++] = (char)(0x80 | (wc & 0x3f));
    }
  }
  dst[j] = '\0';
}

/* Look for a Rock Ridge SUSP "NM" (alternate name) entry in the system use area */
static int iso_rockridge_name(const unsigned char *su, unsigned int su_len, char *dst, const unsigned int dst_size)
{
  unsigned int i = 0;
  unsigned int j = 0;
  int found = 0;
  /* Skip a possible SUSP "SP" offset indicator */
  if(su_len >= 7 && su[0] == 'S' && su[1] == 'P' && su[2] == 7)
    i = 7 + su[6];
  while(i + 4 <= su_len)
  {
    const unsigned int len = su[i+2];
    if(len < 4 || i + len > su_len)
      break;
    if(su[i] == 'N' && su[i+1] == 'M' && len >= 5)
    {
      const unsigned int flags = su[i+4];
      unsigned int k;
      if((flags & 0x06) != 0)
      {	/* CURRENT or PARENT flag: no usable name */
	return 0;
      }
      for(k = 5; k < len && j + 1 < dst_size; k++)
	dst[j++] = (char)su[i+k];
      found = 1;
      if((flags & 0x01) == 0)
	break;	/* name not continued in another NM entry */
    }
    i += len;
  }
  if(found)
    dst[j] = '\0';
  return found;
}

static void iso_name_from_record(const unsigned char *record, const unsigned int rec_len, const unsigned int joliet, char *dst, const unsigned int dst_size)
{
  const unsigned int name_len = record[32];
  const unsigned char *name = &record[33];
  if(name_len == 1 && name[0] == 0)
  {
    strncpy(dst, ".", dst_size);
    return;
  }
  if(name_len == 1 && name[0] == 1)
  {
    strncpy(dst, "..", dst_size);
    return;
  }
  if(joliet)
  {
    iso_ucs2be_to_utf8(dst, dst_size, name, name_len);
  }
  else
  {
    unsigned int su_off = 33 + name_len;
    if((su_off & 1) != 0)
      su_off++;	/* padding byte after an even-length identifier field */
    if(su_off < rec_len &&
	iso_rockridge_name(&record[su_off], rec_len - su_off, dst, dst_size) > 0)
    {
      /* Rock Ridge alternate name used */
    }
    else
    {
      unsigned int i;
      unsigned int j = 0;
      for(i = 0; i < name_len && j + 1 < dst_size; i++)
      {
	if(name[i] == ';')
	  break;	/* strip the ISO9660 ";version" suffix */
	dst[j++] = (char)name[i];
      }
      /* strip a trailing dot left by an empty extension */
      if(j > 1 && dst[j-1] == '.')
	j--;
      dst[j] = '\0';
    }
  }
  if(dst[0] == '\0')
    strncpy(dst, "?", dst_size);
}

static int iso_parse_dir_block(const unsigned char *buffer, const unsigned int size, const unsigned int block_size, const unsigned int joliet, file_info_t *dir_list)
{
  unsigned int offset = 0;
  uint64_t multi_size = 0;
  uint32_t multi_extent = 0;
  while(offset + 33 < size)
  {
    const unsigned char *record = &buffer[offset];
    const unsigned int rec_len = record[0];
    unsigned int name_len;
    if(rec_len == 0)
    {	/* records don't cross block boundaries, skip the padding */
      offset = ((offset / block_size) + 1) * block_size;
      continue;
    }
    if(offset + rec_len > size || rec_len < 34)
      break;
    name_len = record[32];
    if(33 + name_len > rec_len)
      break;
    {
      const uint32_t extent = le32(*(const uint32_t *)&record[2]) + record[1];
      const uint32_t data_len = le32(*(const uint32_t *)&record[10]);
      const unsigned int flags = record[25];
      if((flags & ISO_FLAG_MULTI) != 0)
      {	/* non-final extent of a multi-extent file */
	if(multi_size == 0)
	  multi_extent = extent;
	multi_size += data_len;
      }
      else
      {
	file_info_t *new_file = (file_info_t *)MALLOC(sizeof(*new_file));
	new_file->name = (char *)MALLOC(DIR_NAME_LEN);
	iso_name_from_record(record, rec_len, joliet, new_file->name, DIR_NAME_LEN);
	new_file->st_ino = (multi_size > 0 ? multi_extent : extent);
	new_file->st_mode = ((flags & ISO_FLAG_DIR) != 0 ?
	    LINUX_S_IFDIR | LINUX_S_IRUGO | LINUX_S_IXUGO :
	    LINUX_S_IFREG | LINUX_S_IRUGO);
	new_file->st_uid = 0;
	new_file->st_gid = 0;
	new_file->st_size = multi_size + data_len;
	new_file->td_atime = iso_date_to_unix(&record[18]);
	new_file->td_ctime = new_file->td_atime;
	new_file->td_mtime = new_file->td_atime;
	new_file->status = 0;
	td_list_add_tail(&new_file->list, &dir_list->list);
	multi_size = 0;
	multi_extent = 0;
      }
    }
    offset += rec_len;
  }
  return 0;
}

static int iso_dir(disk_t *disk, const partition_t *partition, dir_data_t *dir_data, const unsigned long int first_inode, file_info_t *dir_list)
{
  const struct iso_dir_struct *ls = (const struct iso_dir_struct *)dir_data->private_dir_data;
  const unsigned int block_size = ls->block_size;
  uint32_t extent;
  uint32_t dir_size;
  unsigned char *buffer;
  if(first_inode == 0 || (uint32_t)first_inode == ls->root_extent)
  {
    extent = ls->root_extent;
    dir_size = ls->root_size;
  }
  else
  {
    unsigned char *first_block = (unsigned char *)MALLOC(block_size);
    extent = (uint32_t)first_inode;
    /* Read the "." record of the directory to get its real data length */
    if((unsigned int)disk->pread(disk, first_block, block_size,
	  partition->part_offset + (uint64_t)extent * block_size) != block_size)
    {
      log_error("ISO: Can't read directory block at extent %lu\n", first_inode);
      free(first_block);
      return -1;
    }
    if(first_block[0] < 34)
    {
      free(first_block);
      return -1;
    }
    dir_size = le32(*(const uint32_t *)&first_block[10]);
    free(first_block);
  }
  if(dir_size == 0 || dir_size > ISO_MAX_DIR_SIZE)
    return -1;
  dir_size = ((dir_size + block_size - 1) / block_size) * block_size;
  buffer = (unsigned char *)MALLOC(dir_size);
  if((unsigned int)disk->pread(disk, buffer, dir_size,
	partition->part_offset + (uint64_t)extent * block_size) != dir_size)
  {
    log_error("ISO: Can't read directory content at extent %u\n", extent);
    free(buffer);
    return -1;
  }
  iso_parse_dir_block(buffer, dir_size, block_size, ls->joliet, dir_list);
  free(buffer);
  return 0;
}

static copy_file_t iso_copy(disk_t *disk, const partition_t *partition, dir_data_t *dir_data, const file_info_t *file)
{
  const struct iso_dir_struct *ls = (const struct iso_dir_struct *)dir_data->private_dir_data;
  const unsigned int block_size = ls->block_size;
  char *new_file;
  FILE *f_out;
  unsigned char *buffer;
  uint64_t file_size = file->st_size;
  uint64_t offset;
  f_out = fopen_local(&new_file, dir_data->local_dir, dir_data->current_directory);
  if(!f_out)
  {
    log_critical("Can't create file %s: \n", new_file);
    free(new_file);
    return CP_CREATE_FAILED;
  }
  buffer = (unsigned char *)MALLOC(64 * block_size);
  offset = partition->part_offset + (uint64_t)file->st_ino * block_size;
  while(file_size > 0)
  {
    unsigned int toread = 64 * block_size;
    if(toread > file_size)
      toread = (unsigned int)file_size;
    if((unsigned int)disk->pread(disk, buffer, toread, offset) != toread)
    {
      log_error("iso_copy: Can't read data at offset %llu\n",
	  (long long unsigned)offset);
      fclose(f_out);
      set_date(new_file, file->td_atime, file->td_mtime);
      free(new_file);
      free(buffer);
      return CP_READ_FAILED;
    }
    if(fwrite(buffer, 1, toread, f_out) != toread)
    {
      log_error("iso_copy: failed to write data %s\n", strerror(errno));
      fclose(f_out);
      set_date(new_file, file->td_atime, file->td_mtime);
      free(new_file);
      free(buffer);
      return CP_NOSPACE;
    }
    file_size -= toread;
    offset += toread;
  }
  fclose(f_out);
  set_date(new_file, file->td_atime, file->td_mtime);
  free(new_file);
  free(buffer);
  return CP_OK;
}

static void dir_partition_iso_close(dir_data_t *dir_data)
{
  struct iso_dir_struct *ls = (struct iso_dir_struct *)dir_data->private_dir_data;
  free(ls);
}

dir_partition_t dir_partition_iso_init(disk_t *disk, const partition_t *partition, dir_data_t *dir_data, const int verbose)
{
  static const unsigned char iso_magic[6] = { 0x01, 'C', 'D', '0', '0', '1' };
  struct iso_dir_struct *ls;
  unsigned char *vd;
  unsigned int block_size;
  uint32_t root_extent;
  uint32_t root_size;
  unsigned int joliet = 0;
  unsigned int lba;
  vd = (unsigned char *)MALLOC(ISO_BLOCK_SIZE_DFLT);
  /* Primary Volume Descriptor at LBA 16 */
  if(disk->pread(disk, vd, ISO_BLOCK_SIZE_DFLT,
	partition->part_offset + (uint64_t)ISO_PVD_LBA * ISO_BLOCK_SIZE_DFLT) != ISO_BLOCK_SIZE_DFLT)
  {
    free(vd);
    return DIR_PART_EIO;
  }
  if(memcmp(vd, iso_magic, sizeof(iso_magic)) != 0)
  {
    free(vd);
    return DIR_PART_ENOSYS;
  }
  {
    const struct iso_primary_descriptor *pvd = (const struct iso_primary_descriptor *)vd;
    block_size = le16(pvd->logical_block_size_le);
    if(block_size < 512 || block_size > 4096 || (block_size & (block_size - 1)) != 0)
      block_size = ISO_BLOCK_SIZE_DFLT;
    root_extent = le32(*(const uint32_t *)&pvd->root_directory_record[2]);
    root_size = le32(*(const uint32_t *)&pvd->root_directory_record[10]);
  }
  /* Look for a Joliet Supplementary Volume Descriptor */
  for(lba = ISO_PVD_LBA + 1; lba < ISO_PVD_LBA + 32; lba++)
  {
    if(disk->pread(disk, vd, ISO_BLOCK_SIZE_DFLT,
	  partition->part_offset + (uint64_t)lba * ISO_BLOCK_SIZE_DFLT) != ISO_BLOCK_SIZE_DFLT)
      break;
    if(vd[0] == 0xff)
      break;	/* Volume Descriptor Set Terminator */
    if(vd[0] == 0x02 && memcmp(&vd[1], "CD001", 5) == 0 &&
	vd[88] == 0x25 && vd[89] == 0x2f &&
	(vd[90] == 0x40 || vd[90] == 0x43 || vd[90] == 0x45))
    {
      const struct iso_primary_descriptor *svd = (const struct iso_primary_descriptor *)vd;
      root_extent = le32(*(const uint32_t *)&svd->root_directory_record[2]);
      root_size = le32(*(const uint32_t *)&svd->root_directory_record[10]);
      joliet = 1;
      break;
    }
  }
  free(vd);
  if(root_extent == 0 || root_size == 0)
    return DIR_PART_EIO;
  if(verbose > 0)
    log_info("ISO9660%s: block_size=%u root_extent=%u root_size=%u\n",
	joliet ? " (Joliet)" : "", block_size, root_extent, root_size);
  ls = (struct iso_dir_struct *)MALLOC(sizeof(*ls));
  ls->block_size = block_size;
  ls->joliet = joliet;
  ls->root_extent = root_extent;
  ls->root_size = root_size;
  strncpy(dir_data->current_directory, "/", sizeof(dir_data->current_directory));
  dir_data->current_inode = root_extent;
  dir_data->param = 0;
  dir_data->verbose = verbose;
  dir_data->capabilities = 0;
  dir_data->copy_file = &iso_copy;
  dir_data->close = &dir_partition_iso_close;
  dir_data->local_dir = NULL;
  dir_data->private_dir_data = ls;
  dir_data->get_dir = &iso_dir;
  return DIR_PART_OK;
}
