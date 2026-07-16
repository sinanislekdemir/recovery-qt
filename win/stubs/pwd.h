/*

    File: pwd.h (MinGW-w64 stub for building libntfs-3g)

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
#ifndef STUB_PWD_H
#define STUB_PWD_H

struct passwd {
  char *pw_name;
  char *pw_passwd;
  int pw_uid;
  int pw_gid;
  char *pw_gecos;
  char *pw_dir;
  char *pw_shell;
};

static __inline__ struct passwd *getpwnam(const char *n) {
  (void)n;
  return (struct passwd *)0;
}

static __inline__ struct passwd *getpwuid(int u) {
  (void)u;
  return (struct passwd *)0;
}

#endif
