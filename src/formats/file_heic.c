/*

    File: file_heic.c

    Copyright (C) 2025 Christophe GRENIER <grenier@cgsecurity.org>
  
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

#if !defined(SINGLE_FORMAT) || defined(SINGLE_FORMAT_heic)
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#include "types.h"
#include "filegen.h"
#include "common.h"

static void register_header_check_heic(file_stat_t *file_stat);

const file_hint_t file_hint_heic = {.extension = "heic",
                                    .description = "High Efficiency Image File (HEIC/HEIF)",
                                    .max_filesize = PHOTOREC_MAX_FILE_SIZE,
                                    .recover = 1,
                                    .enable_by_default = 1,
                                    .register_header_check = &register_header_check_heic};

/* ISO base media file format: ftyp box with HEIF brand */
/* ftyp box: 4 bytes size (BE32), 4 bytes "ftyp", 4 bytes major brand */

static int heic_is_valid_brand(const unsigned char *brand) {
  static const unsigned char brands[][4] = {
      {'h', 'e', 'i', 'c'}, {'h', 'e', 'i', 'x'}, {'h', 'e', 'v', 'c'}, {'h', 'e', 'i', 'm'}, {'h', 'e', 'i', 's'},
      {'h', 'e', 'v', 'm'}, {'h', 'e', 'v', 's'}, {'m', 'i', 'f', '1'}, {'m', 's', 'f', '1'}, {'a', 'v', 'i', 'f'}};
  unsigned int i;
  for (i = 0; i < sizeof(brands) / sizeof(brands[0]); i++) {
    if (memcmp(brand, brands[i], 4) == 0)
      return 1;
  }
  return 0;
}

static int header_check_heic(const unsigned char *buffer, const unsigned int buffer_size,
                             const unsigned int safe_header_only, const file_recovery_t *file_recovery,
                             file_recovery_t *file_recovery_new) {
  uint32_t box_size;
  const unsigned char *brand;
  /* Need at least: 4 (size) + 4 ("ftyp") + 4 (major brand) = 12 bytes */
  if (buffer_size < 12)
    return 0;
  box_size =
      ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) | ((uint32_t)buffer[2] << 8) | (uint32_t)buffer[3];
  if (box_size < 12 || box_size > 1024)
    return 0;
  if (memcmp(buffer + 4, "ftyp", 4) != 0)
    return 0;
  brand = buffer + 8;
  if (!heic_is_valid_brand(brand))
    return 0;
  reset_file_recovery(file_recovery_new);
  file_recovery_new->extension = file_hint_heic.extension;
  file_recovery_new->min_filesize = 16;
  if (box_size >= 16) {
    file_recovery_new->calculated_file_size = box_size + 8;
    file_recovery_new->data_check = &data_check_size;
    file_recovery_new->file_check = &file_check_size;
  }
  return 1;
}

static void register_header_check_heic(file_stat_t *file_stat) {
  /* Register offset 4 to match the "ftyp" box type at that position */
  static const unsigned char heic_header[4] = {'f', 't', 'y', 'p'};
  register_header_check(4, heic_header, sizeof(heic_header), &header_check_heic, file_stat);
}
#endif
