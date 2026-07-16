/*

    File: core_test_win.c

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
/*
 * Headless Windows console harness to exercise the C core without the Qt
 * GUI. Runs under wine or a real Windows machine:
 *
 *   core-test.exe <image_path> [--luks-pass PASS] [--restore-to DIR]
 *                 [--dump FILE] [--dump-out FILE]
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "common.h"
#include "hdaccess.h"
#include "log.h"
#include "fnctdsk.h"
#include "recovery.h"
#include "luksdec.h"
#include "fat.h"
#include "ntfs.h"
#include "ext2.h"
#include "exfat.h"
#include "luks.h"
#include "iso.h"
#include "partauto.h"
#include "log_part.h"

extern const arch_fnct_t arch_none;

static int g_failures = 0;

static partition_t *make_whole_disk_part(disk_t *disk) {
  partition_t *part = partition_new(&arch_none);
  if (!part)
    return NULL;
  part->part_offset = 0;
  part->part_size = disk->disk_size;
  if (check_EXT2(disk, part, 0) == 0)
    return part;
  if (check_NTFS(disk, part, 0, 0) == 0)
    return part;
  if (check_FAT(disk, part, 0) == 0)
    return part;
  if (check_ISO(disk, part) == 0)
    return part;
  if (check_exFAT(disk, part) == 0)
    return part;
  check_LUKS(disk, part);
  return part;
}

static void walk_tree(file_node_t *node, int depth) {
  struct td_list_head *pos;
  if (!node)
    return;
  printf("%c%c ", node->orphan ? 'O' : (node->deleted ? 'X' : ' '), node->backup_restored ? 'B' : ' ');
  for (int i = 0; i < depth; i++)
    printf("  ");
  printf("%c  %-40s %10llu\n", node->type == NODE_DIR ? 'D' : 'F', node->name ? node->name : "(root)",
         (unsigned long long)node->size);
  td_list_for_each(pos, &node->children) {
    file_node_t *child = td_list_entry(pos, file_node_t, siblings);
    walk_tree(child, depth + 1);
  }
}

static void dump_file(scan_tree_t *tree, disk_t *disk, partition_t *part, const char *path) {
  size_t out_size = 0;
  unsigned char *data;
  unsigned long sum = 0;
  file_node_t *node = tree_find_path(tree, path);
  printf("--- read_file_bytes(%s) ---\n", path);
  if (!node) {
    printf("  not found in tree\n");
    return;
  }
  printf("  size=%llu deleted=%d orphan=%d\n", (unsigned long long)node->size, (int)node->deleted, (int)node->orphan);
  data = read_file_bytes(tree, disk, part, node, &out_size);
  if (!data) {
    printf("  FAIL: read_file_bytes returned NULL\n");
    g_failures++;
    return;
  }
  for (size_t k = 0; k < out_size; k++)
    sum = (sum * 31 + data[k]) & 0xffffffffUL;
  printf("  ok size=%zu checksum=%08lx\n", out_size, sum);
  printf("  head: ");
  for (size_t k = 0; k < out_size && k < 24; k++)
    printf("%02x", data[k]);
  printf("\n");
  free(data);
}

int main(int argc, char **argv) {
  const char *img_path;
  const char *luks_pass = NULL;
  const char *restore_dir = NULL;
  disk_t *disk;
  disk_t *use_disk;
  partition_t *part;
  partition_t *use_part;
  scan_tree_t *tree;
  int i;

  if (argc < 2) {
    fprintf(stderr, "usage: %s <image_path> [--luks-pass PASS] [--restore-to DIR]\n", argv[0]);
    return 1;
  }
  img_path = argv[1];
  for (i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--luks-pass") == 0 && i + 1 < argc)
      luks_pass = argv[++i];
    else if (strcmp(argv[i], "--restore-to") == 0 && i + 1 < argc)
      restore_dir = argv[++i];
  }

  log_open("core-test.log", TD_LOG_CREATE, &i);

  printf("--- open %s ---\n", img_path);
  disk = file_test_availability((char *)img_path, 2, TESTDISK_O_RDONLY);
  if (!disk) {
    printf("FAIL: file_test_availability returned NULL\n");
    return 1;
  }
  printf("  size=%llu sector_size=%u desc=%s\n", (unsigned long long)disk->disk_size, disk->sector_size,
         disk->description_short(disk));

  part = make_whole_disk_part(disk);
  if (!part) {
    printf("FAIL: partition_new returned NULL\n");
    return 1;
  }
  printf("  fsname='%s' upart_type=%d\n", part->fsname, (int)part->upart_type);

  use_disk = disk;
  use_part = part;

  if (luks_pass != NULL) {
    printf("--- luksdec_open ---\n");
    disk_t *dec = luksdec_open(disk, part->part_offset, luks_pass);
    if (!dec) {
      printf("FAIL: luksdec_open returned NULL\n");
      g_failures++;
    } else {
      printf("  decrypted disk size=%llu\n", (unsigned long long)dec->disk_size);
      use_disk = dec;

      /* Replicate the exact GUI flow (PartitionList::detect) */
      printf("--- gui-sim: autodetect_arch ---\n");
      autodetect_arch(dec, NULL);
      printf("  arch=%s\n", dec->arch ? dec->arch->part_name : "(null)");
      printf("--- gui-sim: read_part ---\n");
      if (dec->arch) {
        list_part_t *lp = dec->arch->read_part(dec, 0, 0);
        printf("  read_part=%p\n", (void *)lp);
        for (list_part_t *it = lp; it != NULL; it = it->next) {
          if (it->part == NULL)
            continue;
          check_LUKS(dec, it->part);
          log_partition(dec, it->part);
          printf("  part offset=%llu size=%llu fsname='%s' upart=%d\n", (unsigned long long)it->part->part_offset,
                 (unsigned long long)it->part->part_size, it->part->fsname, (int)it->part->upart_type);
        }
        part_free_list(lp);
      }
      printf("--- gui-sim: descriptions ---\n");
      printf("  desc=%s\n", dec->description(dec));
      printf("  desc_short=%s\n", dec->description_short(dec));

      use_part = make_whole_disk_part(dec);
      printf("  decrypted fsname='%s' upart_type=%d\n", use_part->fsname, (int)use_part->upart_type);
    }
  }

  tree = tree_new();
  printf("--- scanner_run ---\n");
  {
    int rv = scanner_run(tree, use_disk, use_part, false);
    printf("  scanner_run=%d\n", rv);
    if (rv != 0)
      g_failures++;
  }
  walk_tree(tree->root, 0);

  dump_file(tree, use_disk, use_part, "/test.txt");
  dump_file(tree, use_disk, use_part, "/TEST.TXT");
  dump_file(tree, use_disk, use_part, "/payload.bin");
  dump_file(tree, use_disk, use_part, "/PAYLOAD.BIN");

  if (restore_dir != NULL) {
    printf("--- restore_files to %s ---\n", restore_dir);
    const char *names[] = {"/payload.bin", "/PAYLOAD.BIN", "/test.txt", "/TEST.TXT"};
    for (size_t n = 0; n < sizeof(names) / sizeof(names[0]); n++) {
      file_node_t *node = tree_find_path(tree, names[n]);
      if (node)
        node->marked = 1;
    }
    int rv = restore_files(tree, use_disk, use_part, restore_dir);
    printf("  restore_files=%d\n", rv);
    if (rv != 0)
      g_failures++;
  }

  printf("--- carver_run ---\n");
  {
    scan_tree_t *ctree = tree_new();
    int rv = carver_run(ctree, use_disk, use_part, NULL, false);
    printf("  carver_run=%d\n", rv);
    walk_tree(ctree->root, 0);
    tree_free(ctree);
  }

  tree_free(tree);
  printf("=== RESULT: %d failures ===\n", g_failures);
  return g_failures;
}
