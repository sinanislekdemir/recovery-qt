/*

    File: file_jp2.c

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

#if !defined(SINGLE_FORMAT) || defined(SINGLE_FORMAT_jp2)
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

/*@ requires valid_register_header_check(file_stat); */
static void register_header_check_jp2(file_stat_t *file_stat);

const file_hint_t file_hint_jp2= {
  .extension="jp2",
  .description="JPEG 2000 image",
  .max_filesize=PHOTOREC_MAX_FILE_SIZE,
  .recover=1,
  .enable_by_default=1,
  .register_header_check=&register_header_check_jp2
};

/* JPEG 2000 Signature box: 12 bytes at offset 0 */
/* 0x0000000C 0x6A502020 0x0D0A870A = \0\0\0\x0C + jP\s\s + \r\n\x87\n */
/* JPEG 2000 codestream starts with SOC marker: 0xFF4F 0xFF51 */

/*@
  @ requires buffer_size >= 12;
  @ requires separation: \separated(&file_hint_jp2, buffer+(..), file_recovery, file_recovery_new);
  @ requires valid_header_check_param(buffer, buffer_size, safe_header_only, file_recovery, file_recovery_new);
  @ ensures  valid_header_check_result(\result, file_recovery_new);
  @ assigns  *file_recovery_new;
  @*/
static int header_check_jp2(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  static const unsigned char jp2_sig[12]= {
    0x00, 0x00, 0x00, 0x0C, 'j', 'P', 0x20, 0x20,
    0x0D, 0x0A, 0x87, 0x0A
  };
  if(memcmp(buffer, jp2_sig, sizeof(jp2_sig))!=0)
    return 0;
  reset_file_recovery(file_recovery_new);
  file_recovery_new->extension=file_hint_jp2.extension;
  file_recovery_new->min_filesize=16;
  return 1;
}

/* JPEG 2000 raw codestream: SOC marker (0xFF4F) */
/*@
  @ requires buffer_size >= 2;
  @ requires separation: \separated(&file_hint_jp2, buffer+(..), file_recovery, file_recovery_new);
  @ requires valid_header_check_param(buffer, buffer_size, safe_header_only, file_recovery, file_recovery_new);
  @ ensures  valid_header_check_result(\result, file_recovery_new);
  @ assigns  *file_recovery_new;
  @*/
static int header_check_j2c(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  if(buffer[0]!=0xFF || buffer[1]!=0x4F)
    return 0;
  if(buffer_size < 4)
    return 1;
  if(buffer[2]!=0xFF || buffer[3]!=0x51)
    return 0;
  reset_file_recovery(file_recovery_new);
  file_recovery_new->extension="j2c";
  file_recovery_new->min_filesize=4;
  return 1;
}

static void register_header_check_jp2(file_stat_t *file_stat)
{
  static const unsigned char jp2_header[12]= {
    0x00, 0x00, 0x00, 0x0C, 'j', 'P', 0x20, 0x20,
    0x0D, 0x0A, 0x87, 0x0A
  };
  static const unsigned char j2c_header[2]= { 0xFF, 0x4F };
  register_header_check(0, jp2_header, sizeof(jp2_header), &header_check_jp2, file_stat);
  register_header_check(0, j2c_header, sizeof(j2c_header), &header_check_j2c, file_stat);
}
#endif
