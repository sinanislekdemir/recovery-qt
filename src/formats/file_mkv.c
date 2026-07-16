/*

    File: file_mkv.c

    Copyright (C) 1998-2007,2011 Christophe GRENIER <grenier@cgsecurity.org>
    Copyright (C) 2011 Nick Schrader <nick.schrader@iserv-gis.de>
  
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

#if !defined(SINGLE_FORMAT) || defined(SINGLE_FORMAT_mkv)
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "types.h"
#include "filegen.h"
#include "common.h"
#ifdef DEBUG_MKV
#include "log.h"
#endif

static void register_header_check_mkv(file_stat_t *file_stat);

const file_hint_t file_hint_mkv = {.extension = "mkv",
                                   .description = "Matroska",
                                   .max_filesize = PHOTOREC_MAX_FILE_SIZE,
                                   .recover = 1,
                                   .enable_by_default = 1,
                                   .register_header_check = &register_header_check_mkv};

static int EBML_read_unsigned(const unsigned char *p, const unsigned int p_size, uint64_t *uint64) {
  unsigned char test_bit = 0x80;
  unsigned int i, bytes = 1;

  const unsigned char c = *p;
  uint64_t val;
  if (p_size == 0 || c == 0x00)
    return -1;

  while ((c & test_bit) != test_bit) {
    test_bit >>= 1;
    bytes++;
  }

  if (p_size < bytes)
    return -1;

  //  val = c & !test_bit; //eliminate first bit, val < 0x80

  val = c & ~test_bit;

  for (i = 1; i < bytes; i++) {
    val <<= 8;
    val += p[i];
  }

  *uint64 = val;
  return bytes;
}

static int EBML_find(const unsigned char *buffer, const unsigned int buffer_size, const unsigned char *EBML_Header,
                     const unsigned int EBML_size) {
  unsigned int offset = 0;

  while (offset < buffer_size) {
    uint64_t uint64 = 0;
    int bytes;
    bytes = EBML_read_unsigned(&buffer[offset], buffer_size - offset, &uint64);
#ifdef DEBUG_MKV
    log_info("EBML_find %02x%02x bytes=%d\n", buffer[offset], buffer[offset + 1], bytes);
#endif
    if (bytes <= 0)
      return -1;
    if ((unsigned int)bytes == EBML_size && memcmp(&buffer[offset], EBML_Header, EBML_size) == 0) {
      return offset + bytes;
    }
    offset += bytes;
    if (offset >= buffer_size)
      return -1;
    bytes = EBML_read_unsigned(&buffer[offset], buffer_size - offset, &uint64);
    if (bytes <= 0 || uint64 > buffer_size)
      return -1;
    offset += bytes;
    offset += uint64;
  }
  return -1;
}

static int EBML_read_string(const unsigned char *p, const unsigned int p_size, uint64_t *strlength) {
  int bytes;
  *strlength = 0;
  bytes = EBML_read_unsigned(p, p_size, strlength);
#ifdef DEBUG_MKV
  log_info("EBML_read_string bytes=%d strlength=%llu\n", bytes, (long long unsigned)*strlength);
#endif
  if (bytes <= 0)
    return -1;

  if (bytes + *strlength > p_size)
    return -1;

  return bytes;
}

static const unsigned char EBML_header[4] = {0x1a, 0x45, 0xdf, 0xa3};

static int header_check_mkv(const unsigned char *buffer, const unsigned int buffer_size,
                            const unsigned int safe_header_only, const file_recovery_t *file_recovery,
                            file_recovery_t *file_recovery_new) {
  if (memcmp(buffer, EBML_header, sizeof(EBML_header)) != 0)
    return 0;
  {
    const unsigned char EBML_DocType[2] = {0x42, 0x82};
    const unsigned char EBML_Segment[4] = {0x18, 0x53, 0x80, 0x67};
    uint64_t segment_size = 0;
    uint64_t header_data_size = 0;
    const unsigned char *p;
    unsigned int header_data_offset;
    unsigned int segment_offset;
    unsigned int segment_data_offset;
    uint64_t strlength = 0;
    int bytes;
    int len;
    int offset_doctype;

    if ((len = EBML_read_unsigned(buffer + sizeof(EBML_header), buffer_size - sizeof(EBML_header), &header_data_size)) <
        0)
      return 0;
    header_data_offset = sizeof(EBML_header) + len;
    if (header_data_offset >= buffer_size)
      return 0;
#ifdef DEBUG_MKV
    log_info("header_data_offset %llu\n", (long long unsigned)header_data_offset);
    log_info("header_data_size   %llu\n", (long long unsigned)header_data_size);
#endif
    if (header_data_size >= buffer_size)
      return 0;
    segment_offset = header_data_offset + header_data_size;
#ifdef DEBUG_MKV
    log_info("segment_offset     %llu\n", (long long unsigned)segment_offset);
#endif
    if (segment_offset + sizeof(EBML_Segment) >= buffer_size)
      return 0;
    if (memcmp(&buffer[segment_offset], EBML_Segment, sizeof(EBML_Segment)) != 0)
      return 0;
    p = &buffer[segment_offset + sizeof(EBML_Segment)];
    if ((len = EBML_read_unsigned(p, buffer_size - (p - buffer), &segment_size)) < 0)
      return 0;
    segment_data_offset = segment_offset + sizeof(EBML_Segment) + len;
    /* Check if size is unkown */
    if (segment_size == (1ULL << (7 * len)) - 1)
      segment_size = 0;
#ifdef DEBUG_MKV
    log_info("segment_data_offset %llu\n", (long long unsigned)segment_data_offset);
    log_info("segment size %llu\n", (long long unsigned)segment_size);
#endif
    /* get EBML_DocType, it will be used to set the file extension */
    offset_doctype = EBML_find(&buffer[header_data_offset], header_data_size, EBML_DocType, sizeof(EBML_DocType));
#ifdef DEBUG_MKV
    log_info("offset_doctype = %u\n", offset_doctype);
#endif
    if (offset_doctype < 0 || header_data_size <= (uint64_t)offset_doctype)
      return 0;

    p = &buffer[header_data_offset + offset_doctype];
    bytes =
        EBML_read_string(&buffer[header_data_offset + offset_doctype], header_data_size - offset_doctype, &strlength);
    if (bytes < 0)
      return 0;
    reset_file_recovery(file_recovery_new);
    if ((strlength == 8 && memcmp(p + bytes, "matroska", 8) == 0) ||
        (strlength == 9 && memcmp(p + bytes, "matroska", 9) == 0))
      file_recovery_new->extension = file_hint_mkv.extension;
    else if ((strlength == 4 && memcmp(p + bytes, "webm", 4) == 0) ||
             (strlength == 5 && memcmp(p + bytes, "webm", 5) == 0))
      file_recovery_new->extension = "webm";
    else
      file_recovery_new->extension = "ebml";
    if (segment_size > 0) {
      file_recovery_new->calculated_file_size = segment_data_offset + segment_size;
#ifdef DEBUG_MKV
      log_info("file size    %llu\n", (long long unsigned)file_recovery_new->calculated_file_size);
#endif
      file_recovery_new->data_check = &data_check_size;
      file_recovery_new->file_check = &file_check_size;
    }
  }
  return 1;
}

static void register_header_check_mkv(file_stat_t *file_stat) {
  register_header_check(0, EBML_header, sizeof(EBML_header), &header_check_mkv, file_stat);
}
#endif
