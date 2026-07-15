/*
    File: carver.c

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
#include "log.h"
#include "hdaccess.h"
#include "recovery.h"
#include "filegen.h"
#include "progress.h"

extern file_enable_t array_file_enable[];
extern file_check_list_t file_check_list;
extern const file_hint_t file_hint_txt;
extern const file_hint_t file_hint_fasttxt;
extern const file_hint_t file_hint_ts;
extern const file_hint_t file_hint_xfs;
extern const file_hint_t file_hint_dovecot;
extern const file_hint_t file_hint_mft;
extern const file_hint_t file_hint_mp3;

#define CARVER_CHUNK_SIZE (512 * 1024)
#define CARVER_CHUNK_OVERLAP (64 * 1024)
#define CARVER_MAX_GAP (4 * 1024 * 1024)
#define CARVER_MIN_SIZE 512
#define MAX_ORPHAN_FILES 10000000
#define CARVER_EXT_STATS_MAX 64
#define CARVER_TOP_SHOW 6
#define CARVER_MAX_MATCHES 32

uint64_t g_carver_resume_offset = 0;

static file_stat_t *g_file_stats = NULL;
static unsigned int g_carved_count = 0;
static uint64_t g_carved_size = 0;

typedef struct {
  const char *ext;
  unsigned int count;
  uint64_t total_size;
} carver_ext_stat_t;

typedef struct {
  const file_hint_t *hint;
  const file_check_t *file_check;
  unsigned int score;
  int has_data_check;
  int has_file_check;
} carver_match_t;

static carver_ext_stat_t g_ext_stats[CARVER_EXT_STATS_MAX];
static unsigned int g_ext_count = 0;
static unsigned int g_false_positive_skipped = 0;
static unsigned int g_footer_trimmed = 0;
static unsigned int g_empty_sectors_skipped = 0;

static void carver_init(const char *ext_filter)
{
  file_enable_t *fe;
  for (fe = array_file_enable; fe->file_hint != NULL; fe++)
    fe->enable = fe->file_hint->enable_by_default;
  for (fe = array_file_enable; fe->file_hint != NULL; fe++)
  {
    if (fe->file_hint == &file_hint_dovecot ||
        fe->file_hint == &file_hint_mft ||
        fe->file_hint == &file_hint_mp3)
      fe->enable = 0;
  }
  if (ext_filter && ext_filter[0] && strcmp(ext_filter, "*") != 0)
  {
    char *filter_copy;
    char *token;
    char *saveptr;
    filter_copy = strdup(ext_filter);
    for (fe = array_file_enable; fe->file_hint != NULL; fe++)
      fe->enable = 0;
    token = strtok_r(filter_copy, ",", &saveptr);
    while (token != NULL)
    {
      while (*token == ' ')
        token++;
      for (fe = array_file_enable; fe->file_hint != NULL; fe++)
      {
        const char *ext;
        ext = fe->file_hint->extension;
        if (ext && strcmp(token, ext) == 0)
          fe->enable = 1;
      }
      token = strtok_r(NULL, ",", &saveptr);
    }
    free(filter_copy);
  }
  g_file_stats = init_file_stats(array_file_enable);
}

static void carver_deinit(void)
{
  free_header_check();
  free(g_file_stats);
  g_file_stats = NULL;
}

static int carver_sector_constant(const unsigned char *sector,
    unsigned int size)
{
  unsigned int i;
  unsigned char val;
  if (size < 16)
    return 0;
  val = sector[0];
  if (val != 0x00 && val != 0xFF)
    return 0;
  for (i = 1; i < size; i++)
  {
    if (sector[i] != val)
      return 0;
  }
  return 1;
}

static unsigned int carver_match_score(const file_check_t *fc,
    const file_recovery_t *file_recovery_new, unsigned int sector_aligned)
{
  unsigned int score;
  score = fc->length * 3;
  if (fc->length <= 1)
    score = 1;
  if (file_recovery_new->data_check != NULL)
    score += 5;
  if (file_recovery_new->file_check != NULL)
    score += 5;
  if (sector_aligned)
    score += 2;
  return score;
}

static int carver_check_footer(const unsigned char *buffer,
    unsigned int buffer_size, const file_hint_t *hint)
{
  const char *ext;
  unsigned int i;
  if (buffer_size < 128)
    return -1;
  ext = hint->extension;
  if (ext == NULL)
    return -1;
  if (strcmp(ext, "png") == 0 || strcmp(ext, "mng") == 0 ||
      strcmp(ext, "jng") == 0)
  {
    static const unsigned char iend_sig[4] = { 'I', 'E', 'N', 'D' };
    static const unsigned char mend_sig[4] = { 'M', 'E', 'N', 'D' };
    for (i = 0; i + 8 < buffer_size; i++)
    {
      if (memcmp(buffer + i + 4, iend_sig, 4) == 0)
      {
        if (buffer[i] == 0 && buffer[i + 1] == 0 &&
            buffer[i + 2] == 0 && buffer[i + 3] == 0)
          return (int)i + 12;
      }
      if (memcmp(buffer + i + 4, mend_sig, 4) == 0)
        return (int)i + 12;
    }
    return -1;
  }
  if (strcmp(ext, "jpg") == 0)
  {
    unsigned int soi_count;
    unsigned int j;
    soi_count = 0;
    for (j = 0; j + 3 < buffer_size; j++)
    {
      if (buffer[j] == 0xff && buffer[j + 1] == 0xd8 &&
          buffer[j + 2] == 0xff)
      {
        soi_count++;
        if (soi_count > 1)
          return -1;
      }
    }
    if (soi_count <= 1)
    {
      for (i = buffer_size - 2; i > 0; i--)
      {
        if (buffer[i] == 0xd9 && buffer[i - 1] == 0xff)
          return (int)i + 1;
      }
    }
    return -1;
  }
  if (strcmp(ext, "zip") == 0)
  {
    for (i = 0; i + 4 < buffer_size; i++)
    {
      if (buffer[i] == 'P' && buffer[i + 1] == 'K' &&
          buffer[i + 2] == 0x05 && buffer[i + 3] == 0x06)
      {
        unsigned int comment_len;
        if (i + 22 > buffer_size)
          return (int)i + 22;
        comment_len = (unsigned int)buffer[i + 20] +
                      ((unsigned int)buffer[i + 21] << 8);
        return (int)i + 22 + (int)comment_len;
      }
    }
    return -1;
  }
  if (strcmp(ext, "gif") == 0)
  {
    for (i = 0; i < buffer_size; i++)
    {
      if (buffer[i] == 0x3B)
        return (int)i + 1;
    }
    return -1;
  }
  if (strcmp(ext, "pdf") == 0)
  {
    unsigned int j;
    for (j = buffer_size; j > 5; j--)
    {
      i = j - 5;
      if (buffer[i] == '%' && buffer[i + 1] == '%' &&
          buffer[i + 2] == 'E' && buffer[i + 3] == 'O' &&
          buffer[i + 4] == 'F')
        return (int)i + 5;
    }
    return -1;
  }
  if (strcmp(ext, "ogg") == 0)
  {
    static const unsigned char oggs_sig[4] = { 'O', 'g', 'g', 'S' };
    int last_oggs;
    last_oggs = -1;
    for (i = 0; i + 4 < buffer_size; i++)
    {
      if (memcmp(buffer + i, oggs_sig, 4) == 0)
        last_oggs = (int)i;
    }
    if (last_oggs >= 0 && (unsigned int)last_oggs + 27 < buffer_size)
      return last_oggs + 27;
    return -1;
  }
  if (strcmp(ext, "bmp") == 0)
  {
    uint32_t file_size;
    if (buffer_size < 6)
      return -1;
    file_size = (uint32_t)buffer[2] | ((uint32_t)buffer[3] << 8) |
                ((uint32_t)buffer[4] << 16) | ((uint32_t)buffer[5] << 24);
    if (file_size > 0)
      return (int)file_size;
    return -1;
  }
  if (strcmp(ext, "riff") == 0 || strcmp(ext, "wav") == 0 ||
      strcmp(ext, "avi") == 0)
  {
    uint32_t riff_size;
    if (buffer_size < 8)
      return -1;
    riff_size = (uint32_t)buffer[4] | ((uint32_t)buffer[5] << 8) |
                ((uint32_t)buffer[6] << 16) | ((uint32_t)buffer[7] << 24);
    if (riff_size > 0)
      return (int)riff_size + 8;
    return -1;
  }
  if (strcmp(ext, "mpg") == 0)
  {
    for (i = buffer_size - 4; i > 0; i--)
    {
      if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 &&
          buffer[i + 2] == 0x01 && buffer[i + 3] == 0xb9)
        return (int)i + 4;
    }
    return -1;
  }
  if (strcmp(ext, "flv") == 0)
  {
    uint32_t flv_size;
    if (buffer_size < 9)
      return -1;
    flv_size = (uint32_t)buffer[5] | ((uint32_t)buffer[6] << 8) |
               ((uint32_t)buffer[7] << 16) | ((uint32_t)buffer[8] << 24);
    if (flv_size > 9)
      return (int)flv_size;
    return -1;
  }
  if (strcmp(ext, "wdp") == 0)
  {
    uint32_t wdp_size;
    if (buffer_size < 16)
      return -1;
    wdp_size = (uint32_t)buffer[12] | ((uint32_t)buffer[13] << 8) |
               ((uint32_t)buffer[14] << 16) | ((uint32_t)buffer[15] << 24);
    if (wdp_size > 16)
      return (int)wdp_size;
    return -1;
  }
  return -1;
}

static int carver_text_validate(const unsigned char *buffer,
    unsigned int buffer_size)
{
  unsigned int i;
  unsigned int printable;
  unsigned int nulls;
  unsigned int sample;
  sample = buffer_size;
  if (sample > 512)
    sample = 512;
  printable = 0;
  nulls = 0;
  for (i = 0; i < sample; i++)
  {
    unsigned char c;
    c = buffer[i];
    if (c >= 0x20 && c <= 0x7E)
      printable++;
    if (c == '\n' || c == '\r')
      return 1;
    if (c == 0x00)
      nulls++;
    if (nulls > 4)
      return 0;
  }
  if (printable < (sample / 4))
    return 0;
  return 1;
}

static file_stat_t *carver_find_stat(const file_hint_t *hint)
{
  file_stat_t *fs;
  for (fs = g_file_stats; fs->file_hint != NULL; fs++)
  {
    if (fs->file_hint == hint)
      return fs;
  }
  return NULL;
}

const file_hint_t *carver_check_header(const unsigned char *buffer,
    const unsigned int buffer_size, const uint64_t offset,
    file_stat_t *in_file_stat, uint64_t current_file_size)
{
  const struct td_list_head *tmpl;
  file_recovery_t file_recovery;
  file_recovery_t file_recovery_new;
  carver_match_t matches[CARVER_MAX_MATCHES];
  unsigned int match_count;
  unsigned int best_idx;
  unsigned int best_score;
  unsigned int sector_aligned;
  unsigned int m;

  reset_file_recovery(&file_recovery);
  file_recovery.blocksize = 512;
  file_recovery.location.start = offset;
  file_recovery.file_stat = in_file_stat;
  file_recovery.file_check = (in_file_stat != NULL)
      ? &file_check_size : NULL;
  file_recovery.file_size = (in_file_stat != NULL)
      ? current_file_size : 0;

  reset_file_recovery(&file_recovery_new);
  file_recovery_new.blocksize = 512;
  file_recovery_new.location.start = offset;

  sector_aligned = ((offset % 512) == 0) ? 1 : 0;
  match_count = 0;

  td_list_for_each(tmpl, &file_check_list.list)
  {
    const struct td_list_head *tmp;
    const file_check_list_t *pos;
    const file_check_t *fc;

    pos = td_list_entry_const(tmpl, const file_check_list_t, list);
    if ((unsigned int)buffer_size <= pos->offset)
      continue;
    td_list_for_each(tmp, &pos->file_checks[buffer[pos->offset]].list)
    {
      fc = td_list_entry_const(tmp, const file_check_t, list);
      if (fc->length < 3)
        continue;
      if ((fc->length == 0 ||
           memcmp(buffer + fc->offset, fc->value,
             fc->length) == 0) &&
          fc->header_check(buffer, buffer_size, 0,
            &file_recovery, &file_recovery_new) != 0)
      {
        const file_hint_t *match_hint;
        unsigned int score;
        int is_text;

        match_hint = fc->file_stat->file_hint;
        is_text = (match_hint == &file_hint_txt ||
                   match_hint == &file_hint_fasttxt) ? 1 : 0;
        if (is_text && !carver_text_validate(buffer, buffer_size))
        {
          g_false_positive_skipped++;
          continue;
        }
        score = carver_match_score(fc, &file_recovery_new, sector_aligned);
        if (is_text)
        {
          if (score < 5)
            score = 0;
          else if (score < 10)
            score = score / 2;
        }
        if (match_count < CARVER_MAX_MATCHES)
        {
          matches[match_count].hint = match_hint;
          matches[match_count].file_check = fc;
          matches[match_count].score = score;
          matches[match_count].has_data_check =
              (file_recovery_new.data_check != NULL) ? 1 : 0;
          matches[match_count].has_file_check =
              (file_recovery_new.file_check != NULL) ? 1 : 0;
          match_count++;
        }
      }
    }
  }

  if (match_count == 0)
    return NULL;

  best_idx = 0;
  best_score = matches[0].score;
  for (m = 1; m < match_count; m++)
  {
    if (matches[m].score > best_score)
    {
      best_score = matches[m].score;
      best_idx = m;
    }
  }

  if (match_count > 1)
  {
    log_info("carver: offset=%llu multiple=%u best=%s score=%u\n",
        (unsigned long long)offset, match_count,
        matches[best_idx].hint->description, best_score);
  }

  return matches[best_idx].hint;
}

static unsigned int carver_ext_stat_index(const char *ext)
{
  unsigned int i;
  if (ext == NULL)
    ext = "bin";
  for (i = 0; i < g_ext_count; i++)
  {
    if (g_ext_stats[i].ext && strcmp(g_ext_stats[i].ext, ext) == 0)
      return i;
  }
  if (g_ext_count < CARVER_EXT_STATS_MAX)
  {
    g_ext_stats[g_ext_count].ext = ext;
    g_ext_stats[g_ext_count].count = 0;
    g_ext_stats[g_ext_count].total_size = 0;
    return g_ext_count++;
  }
  return CARVER_EXT_STATS_MAX;
}

static void carver_ext_stat_sort(void)
{
  unsigned int i;
  unsigned int j;
  for (i = 0; i < g_ext_count; i++)
  {
    unsigned int best = i;
    for (j = i + 1; j < g_ext_count; j++)
    {
      if (g_ext_stats[j].count > g_ext_stats[best].count)
        best = j;
    }
    if (best != i)
    {
      carver_ext_stat_t tmp;
      tmp = g_ext_stats[i];
      g_ext_stats[i] = g_ext_stats[best];
      g_ext_stats[best] = tmp;
    }
  }
}

static void carver_ext_stat_add(const char *ext, uint64_t size)
{
  unsigned int idx;
  idx = carver_ext_stat_index(ext);
  if (idx < CARVER_EXT_STATS_MAX)
  {
    g_ext_stats[idx].count++;
    g_ext_stats[idx].total_size += size;
  }
}

static void carver_add_entry(scan_tree_t *tree, uint64_t part_offset,
    uint64_t offset, uint64_t size, unsigned int sector_size,
    const file_hint_t *hint)
{
  char name[64];
  char path[192];
  char ext_upper[32];
  const char *ext;
  unsigned int ei;
  uint64_t relative_offset;
  uint64_t num_sectors;
  file_node_t *node;

  if (g_carved_count >= MAX_ORPHAN_FILES)
    return;
  if (size < CARVER_MIN_SIZE)
    return;

  relative_offset = offset - part_offset;
  num_sectors = (size + sector_size - 1) / sector_size;

  ext = hint->extension;
  if (ext == NULL)
    ext = "bin";
  for (ei = 0; ei < sizeof(ext_upper) - 1 && ext[ei]; ei++)
  {
    char c;
    c = ext[ei];
    if (c >= 'a' && c <= 'z')
      c = (char)(c - 'a' + 'A');
    ext_upper[ei] = c;
  }
  ext_upper[ei] = '\0';

  snprintf(name, sizeof(name), "file_%010u.%s", g_carved_count, ext);
  snprintf(path, sizeof(path), "/ORPHAN/%s/%s", ext_upper, name);

  node = tree_add_path(tree, path, 0, size, relative_offset, num_sectors, 0,
      sector_size, 1);
  if (node)
    node->orphan = 1;
  g_carved_count++;
  g_carved_size += size;
  carver_ext_stat_add(hint->extension, size);
}

static int carver_scan(scan_tree_t *tree, disk_t *disk,
    const partition_t *partition, int deep_scan)
{
  const unsigned int sector_size = disk->sector_size;
  uint64_t part_offset;
  uint64_t part_sectors;
  uint64_t chunk_pos;
  uint64_t in_file_start;
  const file_hint_t *in_file_hint;
  file_stat_t *in_file_stat;
  uint64_t in_file_footer_at;
  int in_file;
  int cancelled;
  unsigned char *buffer;
  uint64_t total_bytes;
  unsigned int ext_i;
  unsigned int i;

  part_offset = partition->part_offset;
  part_sectors = partition->part_size / sector_size;
  total_bytes = (uint64_t)part_sectors * sector_size;
  in_file_start = 0;
  in_file_hint = NULL;
  in_file_stat = NULL;
  in_file_footer_at = 0;
  in_file = 0;
  cancelled = 0;

  g_carved_count = 0;
  g_carved_size = 0;
  g_ext_count = 0;
  g_false_positive_skipped = 0;
  g_footer_trimmed = 0;
  g_empty_sectors_skipped = 0;
  for (ext_i = 0; ext_i < CARVER_EXT_STATS_MAX; ext_i++)
  {
    g_ext_stats[ext_i].ext = NULL;
    g_ext_stats[ext_i].count = 0;
    g_ext_stats[ext_i].total_size = 0;
  }

  buffer = (unsigned char *)MALLOC(CARVER_CHUNK_SIZE);
  if (buffer == NULL)
    return -1;

  if (g_carver_progress)
    g_carver_progress(0, total_bytes, 0, 0);

  if (g_carver_resume_offset > 0)
  {
    chunk_pos = g_carver_resume_offset / sector_size;
    chunk_pos = (chunk_pos / (CARVER_CHUNK_SIZE / sector_size))
                * (CARVER_CHUNK_SIZE / sector_size);
    if (chunk_pos >= part_sectors)
      chunk_pos = 0;
    log_info("Carving resume: starting from sector %llu / %llu\n",
        (unsigned long long)chunk_pos, (unsigned long long)part_sectors);
  }
  else
  {
    chunk_pos = 0;
  }

  for (; chunk_pos < part_sectors;
      chunk_pos += CARVER_CHUNK_SIZE / sector_size)
  {
    unsigned int chunk_size;
    unsigned int chunk_sectors;
    unsigned int skip_sectors;
    uint64_t read_offset;

    chunk_size = CARVER_CHUNK_SIZE;
    if ((uint64_t)(part_sectors - chunk_pos) * sector_size < CARVER_CHUNK_SIZE)
      chunk_size = (unsigned int)((uint64_t)(part_sectors - chunk_pos) * sector_size);
    if (chunk_pos == 0)
    {
      read_offset = part_offset;
      skip_sectors = 0;
    }
    else
    {
      read_offset = part_offset + chunk_pos * (uint64_t)sector_size
                    - (uint64_t)CARVER_CHUNK_OVERLAP;
      skip_sectors = CARVER_CHUNK_OVERLAP / sector_size;
    }

    if ((unsigned int)disk->pread(disk, buffer, chunk_size, read_offset)
        != chunk_size)
      break;

    chunk_sectors = chunk_size / sector_size;

    for (i = skip_sectors; i < chunk_sectors; i++)
    {
      const unsigned char *sector_buf;
      unsigned int buf_remaining;
      uint64_t abs_offset;
      const file_hint_t *hint;
      unsigned int sector_scan_mode;
      unsigned int byte_off;

      sector_buf = buffer + (uint64_t)i * sector_size;
      buf_remaining = chunk_size - (unsigned int)((uint64_t)i * sector_size);
      abs_offset = read_offset + (uint64_t)i * sector_size;

      if (carver_sector_constant(sector_buf, sector_size))
      {
        g_empty_sectors_skipped++;
        sector_scan_mode = 0;
      }
      else
      {
        sector_scan_mode = 1;
      }

      if (sector_scan_mode == 0)
      {
        uint64_t gap_limit;
        if (!in_file)
          continue;
        gap_limit = in_file_hint ? in_file_hint->max_filesize : CARVER_MAX_GAP;
        if (gap_limit == 0)
          gap_limit = CARVER_MAX_GAP;
        if (in_file_footer_at > 0 &&
            abs_offset + sector_size >= in_file_footer_at)
        {
          uint64_t file_size;
          file_size = in_file_footer_at - in_file_start;
          if (file_size >= CARVER_MIN_SIZE)
          {
            carver_add_entry(tree, part_offset, in_file_start,
                file_size, sector_size, in_file_hint);
            g_footer_trimmed++;
          }
          in_file = 0;
          in_file_hint = NULL;
          in_file_stat = NULL;
          in_file_footer_at = 0;
          continue;
        }
        if (abs_offset + sector_size - in_file_start > gap_limit)
        {
          uint64_t file_size;
          file_size = abs_offset + sector_size - in_file_start;
          if (file_size > gap_limit)
            file_size = gap_limit;
          carver_add_entry(tree, part_offset, in_file_start,
              file_size, sector_size, in_file_hint);
          in_file = 0;
          in_file_hint = NULL;
          in_file_stat = NULL;
          in_file_footer_at = 0;
        }
        continue;
      }

      for (byte_off = 0;
           byte_off < sector_size && buf_remaining >= sector_size;
           deep_scan ? (byte_off++, buf_remaining--)
                     : (byte_off += sector_size, buf_remaining -= sector_size))
      {
        const unsigned char *byte_buf;
        uint64_t byte_abs_offset;

        byte_buf = sector_buf + byte_off;
        byte_abs_offset = abs_offset + byte_off;

        hint = carver_check_header(byte_buf, buf_remaining, byte_abs_offset,
            in_file ? in_file_stat : NULL,
            in_file ? byte_abs_offset - in_file_start : 0);

        if (hint)
        {
          int skip_match;
          skip_match = 0;
          if (in_file && in_file_hint && in_file_hint == hint)
          {
            if (in_file_footer_at > 0)
            {
              if (byte_abs_offset < in_file_footer_at)
                skip_match = 1;
            }
            else
            {
              uint64_t dist;
              dist = byte_abs_offset - in_file_start;
              if (dist < CARVER_CHUNK_SIZE / 8)
                skip_match = 1;
            }
          }

          if (in_file && in_file_hint && !skip_match)
          {
            uint64_t diff;
            uint64_t gap_limit;
            int footer_at;

            diff = byte_abs_offset - in_file_start;
            gap_limit = in_file_hint->max_filesize;
            if (gap_limit == 0)
              gap_limit = CARVER_MAX_GAP;
            if (diff > gap_limit)
              diff = gap_limit;

            if (in_file_footer_at > 0)
            {
              uint64_t footer_diff;
              footer_diff = in_file_footer_at - in_file_start;
              if (footer_diff < diff)
              {
                diff = footer_diff;
                g_footer_trimmed++;
              }
            }
            else
            {
              footer_at = carver_check_footer(byte_buf, buf_remaining,
                  in_file_hint);
              if (footer_at > 0 && (uint64_t)footer_at < diff)
              {
                diff = (uint64_t)footer_at;
                g_footer_trimmed++;
              }
            }

            if (diff >= CARVER_MIN_SIZE)
              carver_add_entry(tree, part_offset, in_file_start,
                  diff, sector_size, in_file_hint);
          }

          if (!skip_match)
          {
            in_file_start = byte_abs_offset;
            in_file_hint = hint;
            in_file_stat = carver_find_stat(hint);
            in_file_footer_at = 0;
            in_file = 1;

            {
              int footer_at;
              footer_at = carver_check_footer(byte_buf, buf_remaining, hint);
              if (footer_at > 0)
                in_file_footer_at = byte_abs_offset + (uint64_t)footer_at;
            }
          }
          continue;
        }

        if (!in_file)
          continue;

        if (in_file_footer_at > 0 &&
            byte_abs_offset >= in_file_footer_at)
        {
          uint64_t file_size;
          file_size = in_file_footer_at - in_file_start;
          if (file_size >= CARVER_MIN_SIZE)
          {
            carver_add_entry(tree, part_offset, in_file_start,
                file_size, sector_size, in_file_hint);
            g_footer_trimmed++;
          }
          in_file = 0;
          in_file_hint = NULL;
          in_file_stat = NULL;
          in_file_footer_at = 0;
          continue;
        }

        {
          uint64_t gap_limit;
          gap_limit = in_file_hint ? in_file_hint->max_filesize : CARVER_MAX_GAP;
          if (gap_limit == 0)
            gap_limit = CARVER_MAX_GAP;
          if (byte_abs_offset - in_file_start > gap_limit)
          {
            uint64_t file_size;
            file_size = byte_abs_offset - in_file_start;
            if (file_size > gap_limit)
              file_size = gap_limit;
            carver_add_entry(tree, part_offset, in_file_start,
                file_size, sector_size, in_file_hint);
            in_file = 0;
            in_file_hint = NULL;
            in_file_stat = NULL;
            in_file_footer_at = 0;
          }
        }
      }
    }

    if (g_carver_progress)
    {
      uint64_t scanned_bytes;
      scanned_bytes = chunk_pos * sector_size;
      g_carver_progress(scanned_bytes, total_bytes, g_carved_count,
          g_carved_size);
    }
    if (g_checkpoint_progress)
    {
      uint64_t scanned_bytes;
      scanned_bytes = chunk_pos * sector_size;
      g_checkpoint_progress(scanned_bytes, g_carved_count);
    }
    if (g_session_save_cb)
    {
      uint64_t scanned_bytes;
      scanned_bytes = chunk_pos * sector_size;
      g_session_save_cb(scanned_bytes, (uint64_t)g_carved_count);
    }
    if (g_carver_cancel && g_carver_cancel())
    {
      cancelled = 1;
      break;
    }
  }

  if (in_file && in_file_hint)
  {
    uint64_t part_end;
    uint64_t file_size;
    uint64_t gap_limit;

    part_end = part_offset + part_sectors * sector_size;
    file_size = part_end - in_file_start;
    if (in_file_footer_at > 0 &&
        in_file_footer_at - in_file_start < file_size)
      file_size = in_file_footer_at - in_file_start;
    gap_limit = in_file_hint->max_filesize;
    if (gap_limit > 0 && file_size > gap_limit)
      file_size = gap_limit;
    carver_add_entry(tree, part_offset, in_file_start,
        file_size, sector_size, in_file_hint);
  }

  free(buffer);

  log_info("Carving complete: %u orphan files found\n", g_carved_count);
  log_info("  Partition size:        %llu.%02llu GB\n",
      (unsigned long long)(total_bytes / (1024*1024*1024)),
      (unsigned long long)((total_bytes % (1024*1024*1024)) * 100 / (1024*1024*1024)));
  log_info("  Recovered size:        %llu.%02llu GB\n",
      (unsigned long long)(g_carved_size / (1024*1024*1024)),
      (unsigned long long)((g_carved_size % (1024*1024*1024)) * 100 / (1024*1024*1024)));
  log_info("  Empty sectors skipped: %u\n", g_empty_sectors_skipped);
  log_info("  Text false positives:  %u\n", g_false_positive_skipped);
  log_info("  Footer-trimmed files:  %u\n", g_footer_trimmed);
  if (g_ext_count > 0)
  {
    unsigned int gi;
    carver_ext_stat_sort();
    log_info("  Extension breakdown:\n");
    for (gi = 0; gi < g_ext_count && gi < 12; gi++)
    {
      uint64_t s;
      const char *e;
      e = g_ext_stats[gi].ext ? g_ext_stats[gi].ext : "bin";
      s = g_ext_stats[gi].total_size;
      log_info("    %-6s  %5u files  %8llu.%02llu MB\n",
          e, g_ext_stats[gi].count,
          (unsigned long long)(s / (1024*1024)),
          (unsigned long long)((s % (1024*1024)) * 100 / (1024*1024)));
    }
  }
  log_flush();

  if (g_carver_progress)
    g_carver_progress(total_bytes, total_bytes, g_carved_count, g_carved_size);

  return (int)g_carved_count;
}

int carver_run(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
    const char *ext_filter, int deep_scan)
{
  int result;
  carver_init(ext_filter);
  result = carver_scan(tree, disk, partition, deep_scan);
  carver_deinit();
  g_carver_resume_offset = 0;
  return result;
}
