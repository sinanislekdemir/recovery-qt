/*

    File: mingw_compat.c

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
 * POSIX symbols referenced by the statically linked libntfs-3g objects that
 * MinGW-w64 does not provide. NTFS access in recovery-qt is read-only, so
 * ownership and device-node helpers can safely return neutral values.
 */
#ifdef __MINGW32__

#include <stdlib.h>

int S_ISLNK(int m);
int major(int d);
int minor(int d);
int makedev(int ma, int mi);
int getuid(void);
int getgid(void);
long random(void);
void srandom(unsigned int seed);

int S_ISLNK(int m) { return (m & 0170000) == 0120000; }

int major(int d) { return (d >> 8) & 0xfff; }

int minor(int d) { return d & 0xff; }

int makedev(int ma, int mi) { return ((ma & 0xfff) << 8) | (mi & 0xff); }

int getuid(void) { return 0; }

int getgid(void) { return 0; }

long random(void) { return rand(); }

void srandom(unsigned int seed) { srand(seed); }

#endif
