/*

    File: file_pdb.c

    Copyright (C) 2018 Christophe GRENIER <grenier@cgsecurity.org>

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

#if !defined(SINGLE_FORMAT) || defined(SINGLE_FORMAT_pdb)
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#include "types.h"
#include "filegen.h"


static void register_header_check_pdb(file_stat_t *file_stat);

const file_hint_t file_hint_pdb= {
  .extension="pdb",
  .description="Protein Data Bank data",
  .max_filesize=PHOTOREC_MAX_FILE_SIZE,
  .recover=1,
  .enable_by_default=1,
  .register_header_check=&register_header_check_pdb
};


static data_check_t data_check_pdb81(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery)
{
  unsigned int i;
  
  
  
  while(file_recovery->calculated_file_size + buffer_size/2  >= file_recovery->file_size &&
      file_recovery->calculated_file_size + 81 <= file_recovery->file_size + buffer_size/2)
  {
    const unsigned int i=file_recovery->calculated_file_size + buffer_size/2 - file_recovery->file_size;
    
    if(buffer[i+80]!='\n')
    {
      return DC_STOP;
    }
    file_recovery->calculated_file_size+=81;
  }
  return DC_CONTINUE;
}


static data_check_t data_check_pdb82(const unsigned char *buffer, const unsigned int buffer_size, file_recovery_t *file_recovery)
{
  unsigned int i;
  
  
  
  while(file_recovery->calculated_file_size + buffer_size/2  >= file_recovery->file_size &&
      file_recovery->calculated_file_size + 82 <= file_recovery->file_size + buffer_size/2)
  {
    const unsigned int i=file_recovery->calculated_file_size + buffer_size/2 - file_recovery->file_size;
    
    if(buffer[i+80]!='\r' || buffer[i+81]!='\n')
    {
      return DC_STOP;
    }
    file_recovery->calculated_file_size+=82;
  }
  return DC_CONTINUE;
}


static int header_check_pdb(const unsigned char *buffer, const unsigned int buffer_size, const unsigned int safe_header_only, const file_recovery_t *file_recovery, file_recovery_t *file_recovery_new)
{
  /* Check date */
  if(buffer[0x32] < '0' || buffer[0x32] > '9' || buffer[0x33] < '0' || buffer[0x33] > '9')
    return 0;
  if(buffer[0x34]!='-')
    return 0;
  if(buffer[0x35] < 'A' || buffer[0x35] > 'Z' || buffer[0x36] < 'A' || buffer[0x36] > 'Z' || buffer[0x37] < 'A' || buffer[0x37] > 'Z')
    return 0;
  if(buffer[0x38]!='-')
    return 0;
  if(buffer[0x39] < '0' || buffer[0x39] > '9' || buffer[0x3a] < '0' || buffer[0x3a] > '9')
    return 0;
  /* Check space */
  if(buffer[59]!=' ' || buffer[60]!=' ' || buffer[61]!=' ' || buffer[66]!=' ' || buffer[67]!=' ' || buffer[68]!=' ' || buffer[69]!=' ')
    return 0;
  if(buffer[80]=='\r' && buffer[81]=='\n')
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->extension=file_hint_pdb.extension;
    file_recovery_new->data_check=&data_check_pdb82;
    file_recovery_new->min_filesize=82;
    return 1;
  }
  if(buffer[80]=='\n')
  {
    reset_file_recovery(file_recovery_new);
    file_recovery_new->extension=file_hint_pdb.extension;
    file_recovery_new->data_check=&data_check_pdb81;
    file_recovery_new->min_filesize=81;
    return 1;
  }
  return 0;
}

static void register_header_check_pdb(file_stat_t *file_stat)
{
  register_header_check(0, "HEADER    ", 10, &header_check_pdb, file_stat);
}
#endif
