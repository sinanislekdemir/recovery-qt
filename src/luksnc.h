/*

    File: luksnc.h

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
#ifndef _LUKSNC_H
#define _LUKSNC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_NCURSES
char *ask_luks_passphrase(void);
#endif

int luks_open(const char *device, uint64_t offset, const char *passphrase, char *mapper_name, size_t mapper_name_size);
int luks_close(const char *mapper_name);
void luks_cleanup_orphans(void);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif
