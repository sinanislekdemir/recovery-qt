/*

    File: file_webp.c

    Copyright (C) 2025 Christophe GRENIER <grenier@cgsecurity.org>
    Modified 2026 by Sinan Islekdemir <sinan@islekdemir.com>
  
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

#if !defined(SINGLE_FORMAT) || defined(SINGLE_FORMAT_webp)
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

static void register_header_check_webp(file_stat_t *file_stat);

const file_hint_t file_hint_webp = {.extension = "webp",
                                    .description = "Google WebP image",
                                    .max_filesize = PHOTOREC_MAX_FILE_SIZE,
                                    .recover = 1,
                                    .enable_by_default = 1,
                                    .register_header_check = &register_header_check_webp};

struct riff_header {
  uint32_t magic;
  uint32_t file_size;
  uint32_t form_type;
} __attribute__((gcc_struct, __packed__));

static int header_check_webp(const unsigned char *buffer, const unsigned int buffer_size,
                             const unsigned int safe_header_only, const file_recovery_t *file_recovery,
                             file_recovery_t *file_recovery_new) {
  const struct riff_header *hdr = (const struct riff_header *)buffer;
  uint32_t file_size = le32(hdr->file_size);
  /* RIFF header at offset 0: "RIFF" + size (LE32) + "WEBP" */
  if (memcmp(buffer, "RIFF", 4) != 0)
    return 0;
  if (memcmp(buffer + 8, "WEBP", 4) != 0)
    return 0;
  reset_file_recovery(file_recovery_new);
  file_recovery_new->extension = file_hint_webp.extension;
  file_recovery_new->min_filesize = 12;
  if (file_size > 12) {
    file_recovery_new->calculated_file_size = file_size + 8;
    file_recovery_new->data_check = &data_check_size;
    file_recovery_new->file_check = &file_check_size;
  }
  return 1;
}

static void register_header_check_webp(file_stat_t *file_stat) {
  static const unsigned char webp_header[12] = {'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00, /* size - wildcard */
                                                'W', 'E', 'B', 'P'};
  register_header_check(0, webp_header, 4, &header_check_webp, file_stat);
}
#endif
