/*

    File: grp.h (MinGW-w64 stub for building libntfs-3g)

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
#ifndef STUB_GRP_H
#define STUB_GRP_H

struct group {
  char *gr_name;
  char *gr_passwd;
  int gr_gid;
  char **gr_mem;
};

static __inline__ struct group *getgrnam(const char *n) {
  (void)n;
  return (struct group *)0;
}

static __inline__ struct group *getgrgid(int g) {
  (void)g;
  return (struct group *)0;
}

static __inline__ int setgroups(int n, const int *g) {
  (void)n;
  (void)g;
  return -1;
}

#endif
