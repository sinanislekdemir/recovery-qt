// SPDX-License-Identifier: GPL-2.0-or-later
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
#include "log.h"
#include "recovery.h"
#include "session.h"

#define SESSION_MAX_PATH 4096
#define SESSION_MAX_MODEL 256
#define SESSION_SAVE_INTERVAL 15
#define SESSION_HEADER_END 116

session_save_ctx_t *g_active_save_ctx = NULL;

int session_save_checkpoint(uint64_t progress1, uint64_t progress2)
{
  time_t now;

  if (!g_active_save_ctx)
    return 0;

  now = time(NULL);
  if (now - g_active_save_ctx->last_save_time < SESSION_SAVE_INTERVAL)
    return 0;

  g_active_save_ctx->last_save_time = now;

  log_info("session_save_checkpoint: saving checkpoint (p1=%llu, p2=%llu)\n",
      (unsigned long long)progress1, (unsigned long long)progress2);

  return session_save(
      g_active_save_ctx->filepath,
      g_active_save_ctx->tree,
      g_active_save_ctx->disk,
      g_active_save_ctx->partition,
      g_active_save_ctx->op_type,
      g_active_save_ctx->ext_filter,
      progress1, progress2,
      g_active_save_ctx->resume_phase,
      g_active_save_ctx->resume_offset,
      0 /* incomplete */);
}

static int write_le32(FILE *f, uint32_t v)
{
  unsigned char b[4];
  b[0] = (unsigned char)(v & 0xFF);
  b[1] = (unsigned char)((v >> 8) & 0xFF);
  b[2] = (unsigned char)((v >> 16) & 0xFF);
  b[3] = (unsigned char)((v >> 24) & 0xFF);
  return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}

static int write_le64(FILE *f, uint64_t v)
{
  unsigned char b[8];
  unsigned int i;
  for (i = 0; i < 8; i++)
  {
    b[i] = (unsigned char)(v & 0xFF);
    v >>= 8;
  }
  return fwrite(b, 1, 8, f) == 8 ? 0 : -1;
}

static int write_le16(FILE *f, uint16_t v)
{
  unsigned char b[2];
  b[0] = (unsigned char)(v & 0xFF);
  b[1] = (unsigned char)((v >> 8) & 0xFF);
  return fwrite(b, 1, 2, f) == 2 ? 0 : -1;
}

static uint32_t read_le32(const unsigned char *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
      ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_le64(const unsigned char *p)
{
  uint64_t v;
  unsigned int i;
  v = 0;
  for (i = 0; i < 8; i++)
    v |= ((uint64_t)p[i]) << (i * 8);
  return v;
}

static uint16_t read_le16(const unsigned char *p)
{
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int session_write_node_rec(FILE *f, const file_node_t *node,
    const file_node_t *root, char *path_buf, size_t path_buf_size)
{
  uint16_t path_len;
  unsigned char flags;
  uint32_t i;
  struct td_list_head *pos;

  tree_get_path(node, root, path_buf, path_buf_size);
  path_len = (uint16_t)strlen(path_buf);
  if (path_len == 0)
    return 0;

  flags = 0;
  if (node->type == NODE_DIR) flags |= 0x01;
  if (node->marked)           flags |= 0x02;
  if (node->deleted)          flags |= 0x04;
  if (node->orphan)           flags |= 0x08;
  if (node->expanded)         flags |= 0x10;
  if (node->backup_restored)  flags |= 0x20;
  if (node->backup_modified)  flags |= 0x40;

  if (write_le16(f, path_len) != 0)                  return -1;
  if (fputc(flags, f) == EOF)                        return -1;
  if (write_le64(f, node->size) != 0)                return -1;
  if (write_le64(f, node->first_sector) != 0)        return -1;
  if (write_le64(f, node->num_sectors) != 0)         return -1;
  if (write_le64(f, (uint64_t)node->mtime) != 0)     return -1;
  if (write_le32(f, node->sector_size) != 0)         return -1;
  if (write_le32(f, node->cluster_count) != 0)       return -1;
  if (write_le32(f, node->cluster_size) != 0)        return -1;

  if (fwrite(path_buf, 1, path_len, f) != path_len)
    return -1;

  for (i = 0; i < node->cluster_count; i++)
  {
    if (write_le64(f, node->cluster_list[i]) != 0)
      return -1;
  }

  if (node->type == NODE_DIR)
  {
    td_list_for_each(pos, &node->children)
    {
      file_node_t *child;
      child = td_list_entry(pos, file_node_t, siblings);
      if (session_write_node_rec(f, child, root,
          path_buf, path_buf_size) != 0)
        return -1;
    }
  }

  return 0;
}

static int session_write_tree(FILE *f, const scan_tree_t *tree)
{
  char path_buf[SESSION_MAX_PATH];
  struct td_list_head *pos;

  if (!tree || !tree->root)
    return 0;

  td_list_for_each(pos, &tree->root->children)
  {
    file_node_t *child;
    child = td_list_entry(pos, file_node_t, siblings);
    if (session_write_node_rec(f, child, tree->root,
        path_buf, sizeof(path_buf)) != 0)
      return -1;
  }

  return 0;
}

static int session_read_tree(FILE *f, scan_tree_t *tree)
{
  char path_buf[SESSION_MAX_PATH];

  for (;;)
  {
    uint16_t path_len;
    unsigned char flags;
    int is_dir, marked, deleted, orphan, expanded;
    int backup_restored, backup_modified;
    uint64_t size, first_sector, num_sectors;
    uint64_t mtime64;
    time_t mtime;
    unsigned int sector_size;
    uint32_t cluster_count, cluster_size;
    file_node_t *node;
    uint32_t i;

    {
      unsigned char buf2[2];
      if (fread(buf2, 1, 2, f) != 2)
        break;
      path_len = read_le16(buf2);
    }
    if (path_len == 0xFFFF)
      break;

    {
      int c;
      c = fgetc(f);
      if (c == EOF)
        break;
      flags = (unsigned char)c;
    }

    {
      unsigned char buf8[8];
      unsigned char buf4[4];
      if (fread(buf8, 1, 8, f) != 8) break;
      size = read_le64(buf8);
      if (fread(buf8, 1, 8, f) != 8) break;
      first_sector = read_le64(buf8);
      if (fread(buf8, 1, 8, f) != 8) break;
      num_sectors = read_le64(buf8);
      if (fread(buf8, 1, 8, f) != 8) break;
      mtime64 = read_le64(buf8);
      mtime = (time_t)mtime64;
      if (fread(buf4, 1, 4, f) != 4) break;
      sector_size = read_le32(buf4);
      if (fread(buf4, 1, 4, f) != 4) break;
      cluster_count = read_le32(buf4);
      if (fread(buf4, 1, 4, f) != 4) break;
      cluster_size = read_le32(buf4);
    }

    if (path_len >= sizeof(path_buf))
      break;
    if (fread(path_buf, 1, path_len, f) != path_len)
      break;
    path_buf[path_len] = '\0';

    is_dir           = (flags & 0x01) ? 1 : 0;
    marked           = (flags & 0x02) ? 1 : 0;
    deleted          = (flags & 0x04) ? 1 : 0;
    orphan           = (flags & 0x08) ? 1 : 0;
    expanded         = (flags & 0x10) ? 1 : 0;
    backup_restored  = (flags & 0x20) ? 1 : 0;
    backup_modified  = (flags & 0x40) ? 1 : 0;

    tree_add_path(tree, path_buf, is_dir, size,
        first_sector, num_sectors, mtime, sector_size, deleted);

    node = tree_find_path(tree, path_buf);
    if (node)
    {
      node->marked = marked;
      node->orphan = orphan;
      node->expanded = expanded;
      node->backup_restored = backup_restored;
      node->backup_modified = backup_modified;

      if (cluster_count > 0 && !is_dir)
      {
        node->cluster_list = (uint64_t *)MALLOC(
            cluster_count * sizeof(uint64_t));
        if (node->cluster_list)
        {
          node->cluster_count = cluster_count;
          node->cluster_size = cluster_size;
          for (i = 0; i < cluster_count; i++)
          {
            unsigned char cbuf[8];
            if (fread(cbuf, 1, 8, f) != 8)
              break;
            node->cluster_list[i] = read_le64(cbuf);
          }
        }
        else
        {
          for (i = 0; i < cluster_count; i++)
          {
            unsigned char cbuf[8];
            if (fread(cbuf, 1, 8, f) != 8)
              break;
          }
          node->cluster_count = 0;
          node->cluster_size = 0;
        }
      }
    }
    else
    {
      for (i = 0; i < cluster_count; i++)
      {
        unsigned char cbuf[8];
        if (fread(cbuf, 1, 8, f) != 8)
          break;
      }
    }
  }

  return 0;
}

int session_save(const char *filepath, const scan_tree_t *tree,
    const disk_t *disk, const partition_t *partition,
    int op_type, const char *ext_filter,
    uint64_t progress1, uint64_t progress2,
    int resume_phase, uint64_t resume_offset,
    int flags)
{
  FILE *f;
  uint16_t dev_len, model_len, filter_len;
  const char *model_str, *filter_str;
  int ret;

  if (!filepath || !tree || !disk || !partition)
    return -1;

  f = fopen(filepath, "wb");
  if (!f)
  {
    log_error("session_save: cannot open %s: %s\n",
        filepath, strerror(errno));
    return -1;
  }

  if (fwrite("RECOVRY_SES", 1, 12, f) != 12) { fclose(f); return -1; }
  if (write_le32(f, SESSION_VERSION) != 0)   { fclose(f); return -1; }
  if (write_le32(f, (uint32_t)flags) != 0)   { fclose(f); return -1; }
  if (write_le32(f, (uint32_t)op_type) != 0) { fclose(f); return -1; }
  if (write_le64(f, (uint64_t)time(NULL)) != 0)    { fclose(f); return -1; }
  if (write_le64(f, progress1) != 0)               { fclose(f); return -1; }
  if (write_le64(f, progress2) != 0)               { fclose(f); return -1; }
  if (write_le32(f, (uint32_t)resume_phase) != 0)  { fclose(f); return -1; }
  if (write_le64(f, resume_offset) != 0)           { fclose(f); return -1; }

  if (write_le32(f, disk->sector_size) != 0)       { fclose(f); return -1; }
  if (write_le64(f, disk->disk_size) != 0)         { fclose(f); return -1; }
  if (write_le64(f, partition->part_offset) != 0)  { fclose(f); return -1; }
  if (write_le64(f, partition->part_size) != 0)    { fclose(f); return -1; }
  if (write_le32(f, (uint32_t)partition->upart_type) != 0)    { fclose(f); return -1; }
  if (write_le32(f, partition->part_type_i386) != 0)          { fclose(f); return -1; }
  if (write_le32(f, (flags & SESSION_FLAG_LUKS_DECRYPTED) ? 1 : 0) != 0) { fclose(f); return -1; }
  if (write_le64(f, (flags & SESSION_FLAG_LUKS_DECRYPTED) ? partition->part_offset : 0) != 0) { fclose(f); return -1; }

  dev_len = (uint16_t)strlen(disk->device ? disk->device : "");
  model_str = disk->model ? disk->model : "";
  if (strlen(model_str) > SESSION_MAX_MODEL)
    model_str = "";
  model_len = (uint16_t)strlen(model_str);
  filter_str = ext_filter ? ext_filter : "";
  filter_len = (uint16_t)strlen(filter_str);

  if (write_le16(f, dev_len) != 0)      { fclose(f); return -1; }
  if (write_le16(f, model_len) != 0)    { fclose(f); return -1; }
  if (write_le16(f, filter_len) != 0)   { fclose(f); return -1; }
  if (write_le16(f, 0) != 0)            { fclose(f); return -1; }

  if (dev_len > 0 && disk->device)
  {
    if (fwrite(disk->device, 1, dev_len, f) != dev_len)
      { fclose(f); return -1; }
  }
  if (model_len > 0)
  {
    if (fwrite(model_str, 1, model_len, f) != model_len)
      { fclose(f); return -1; }
  }
  if (filter_len > 0)
  {
    if (fwrite(filter_str, 1, filter_len, f) != filter_len)
      { fclose(f); return -1; }
  }

  {
    long str_end;
    int pad;
    int i;
    str_end = SESSION_HEADER_END + dev_len + model_len + filter_len;
    pad = (int)(((unsigned long)str_end + 7) & ~7) - str_end;
    if (pad < 0) pad = 0;
    for (i = 0; i < pad; i++)
    {
      if (fputc(0, f) == EOF)
        { fclose(f); return -1; }
    }
  }

  ret = session_write_tree(f, tree);
  if (ret == 0)
  {
    if (write_le16(f, 0xFFFF) != 0)
      ret = -1;
  }

  fclose(f);

  if (ret != 0)
  {
    log_error("session_save: write error for %s\n", filepath);
    remove(filepath);
    return -1;
  }

  log_info("session_save: %s saved (%s)\n", filepath,
      (flags & SESSION_FLAG_COMPLETED) ? "completed" : "in progress");
  return 0;
}

session_info_t *session_load(const char *filepath)
{
  FILE *f;
  session_info_t *info;
  unsigned char header_buf[128];
  uint16_t dev_len, model_len, filter_len;
  scan_tree_t *tree;

  if (!filepath)
    return NULL;

  f = fopen(filepath, "rb");
  if (!f)
  {
    log_error("session_load: cannot open %s: %s\n",
        filepath, strerror(errno));
    return NULL;
  }

  if (fread(header_buf, 1, 12, f) != 12)
    { fclose(f); return NULL; }
  if (memcmp(header_buf, "RECOVRY_SES", 12) != 0)
  {
    log_error("session_load: %s: bad magic\n", filepath);
    fclose(f);
    return NULL;
  }

  info = (session_info_t *)MALLOC(sizeof(session_info_t));
  if (!info)
    { fclose(f); return NULL; }
  memset(info, 0, sizeof(session_info_t));

  info->tree = NULL;

  {
    unsigned char buf4[4];
    unsigned char buf8[8];

    if (fread(buf4, 1, 4, f) != 4) { session_free(info); fclose(f); return NULL; }
    info->version = read_le32(buf4);
    if (info->version != SESSION_VERSION)
    {
      log_error("session_load: %s: bad version %u\n", filepath, info->version);
      session_free(info);
      fclose(f);
      return NULL;
    }

    if (fread(buf4, 1, 4, f) != 4) { session_free(info); fclose(f); return NULL; }
    info->flags = read_le32(buf4);

    if (fread(buf4, 1, 4, f) != 4) { session_free(info); fclose(f); return NULL; }
    info->op_type = read_le32(buf4);

    if (fread(buf8, 1, 8, f) != 8) { session_free(info); fclose(f); return NULL; }
    info->timestamp = read_le64(buf8);

    if (fread(buf8, 1, 8, f) != 8) { session_free(info); fclose(f); return NULL; }
    info->progress1 = read_le64(buf8);

    if (fread(buf8, 1, 8, f) != 8) { session_free(info); fclose(f); return NULL; }
    info->progress2 = read_le64(buf8);

    if (fread(buf4, 1, 4, f) != 4) { session_free(info); fclose(f); return NULL; }
    info->resume_phase = (int)read_le32(buf4);

    if (fread(buf8, 1, 8, f) != 8) { session_free(info); fclose(f); return NULL; }
    info->resume_offset = read_le64(buf8);

    if (fread(buf4, 1, 4, f) != 4) { session_free(info); fclose(f); return NULL; }
    info->sector_size = read_le32(buf4);

    if (fread(buf8, 1, 8, f) != 8) { session_free(info); fclose(f); return NULL; }
    info->disk_size = read_le64(buf8);

    if (fread(buf8, 1, 8, f) != 8) { session_free(info); fclose(f); return NULL; }
    info->part_offset = read_le64(buf8);

    if (fread(buf8, 1, 8, f) != 8) { session_free(info); fclose(f); return NULL; }
    info->part_size = read_le64(buf8);

    if (fread(buf4, 1, 4, f) != 4) { session_free(info); fclose(f); return NULL; }
    info->upart_type = read_le32(buf4);

    if (fread(buf4, 1, 4, f) != 4) { session_free(info); fclose(f); return NULL; }
    info->part_type_i386 = read_le32(buf4);

    if (fread(buf4, 1, 4, f) != 4) { session_free(info); fclose(f); return NULL; }
    info->encrypted = read_le32(buf4);

    if (fread(buf8, 1, 8, f) != 8) { session_free(info); fclose(f); return NULL; }
    info->luks_offset = read_le64(buf8);

    {
      unsigned char buf2[2];
      if (fread(buf2, 1, 2, f) != 2) { session_free(info); fclose(f); return NULL; }
      dev_len = read_le16(buf2);
      if (fread(buf2, 1, 2, f) != 2) { session_free(info); fclose(f); return NULL; }
      model_len = read_le16(buf2);
      if (fread(buf2, 1, 2, f) != 2) { session_free(info); fclose(f); return NULL; }
      filter_len = read_le16(buf2);
      /* skip reserved */
      if (fread(buf2, 1, 2, f) != 2) { session_free(info); fclose(f); return NULL; }
    }
  }

  /* Strings follow immediately after the 116-byte fixed header */

  /* Read device path */
  if (dev_len > 0)
  {
    info->device_path = (char *)MALLOC(dev_len + 1);
    if (!info->device_path) { session_free(info); fclose(f); return NULL; }
    if (fread(info->device_path, 1, dev_len, f) != dev_len)
      { session_free(info); fclose(f); return NULL; }
    info->device_path[dev_len] = '\0';
  }
  else
  {
    info->device_path = strdup("");
    if (!info->device_path) { session_free(info); fclose(f); return NULL; }
  }

  /* Read model */
  if (model_len > 0)
  {
    info->model = (char *)MALLOC(model_len + 1);
    if (!info->model) { session_free(info); fclose(f); return NULL; }
    if (fread(info->model, 1, model_len, f) != model_len)
      { session_free(info); fclose(f); return NULL; }
    info->model[model_len] = '\0';
  }
  else
  {
    info->model = strdup("");
    if (!info->model) { session_free(info); fclose(f); return NULL; }
  }

  /* Read ext filter */
  if (filter_len > 0)
  {
    info->ext_filter = (char *)MALLOC(filter_len + 1);
    if (!info->ext_filter) { session_free(info); fclose(f); return NULL; }
    if (fread(info->ext_filter, 1, filter_len, f) != filter_len)
      { session_free(info); fclose(f); return NULL; }
    info->ext_filter[filter_len] = '\0';
  }
  else
  {
    info->ext_filter = strdup("");
    if (!info->ext_filter) { session_free(info); fclose(f); return NULL; }
  }

  /* Skip to 8-byte aligned position after strings */
  {
    long current_pos;
    long target;
    int pad;
    int i;
    current_pos = ftell(f);
    if (current_pos < 0) { session_free(info); fclose(f); return NULL; }
    target = SESSION_HEADER_END + dev_len + model_len + filter_len;
    target = (target + 7) & ~7;
    pad = (int)(target - current_pos);
    if (pad > 0)
    {
      for (i = 0; i < pad; i++)
      {
        if (fgetc(f) == EOF)
          { session_free(info); fclose(f); return NULL; }
      }
    }
  }

  /* Build tree from entries */
  tree = tree_new();
  if (!tree) { session_free(info); fclose(f); return NULL; }

  if (session_read_tree(f, tree) != 0)
  {
    log_error("session_load: tree read error for %s\n", filepath);
    tree_free(tree);
    session_free(info);
    fclose(f);
    return NULL;
  }

  info->tree = tree;

  fclose(f);
  log_info("session_load: %s loaded\n", filepath);
  return info;
}

void session_free(session_info_t *info)
{
  if (!info)
    return;
  if (info->tree)
    tree_free(info->tree);
  if (info->device_path)
    free(info->device_path);
  if (info->model)
    free(info->model);
  if (info->ext_filter)
    free(info->ext_filter);
  free(info);
}
