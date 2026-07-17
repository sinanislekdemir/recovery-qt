/*

    File: suspend_no.c

    Copyright (C) 2008 Christophe GRENIER <grenier@cgsecurity.org>
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef DISABLED_FOR_FRAMAC
#undef HAVE_LIBJPEG
#endif

#if defined(HAVE_LIBJPEG) && defined(HAVE_JPEGLIB_H)
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <jpeglib.h>
#include "suspend.h"

void suspend_memory(j_common_ptr cinfo) {};

int resume_memory(j_common_ptr cinfo) {
  /* Can't resume */
  return -1;
};
#endif
