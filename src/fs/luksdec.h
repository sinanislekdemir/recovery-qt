/*

    File: luksdec.h

    Native in-process LUKS1/LUKS2 read-only decryption. No kernel device
    mapper, no loop devices, no child processes: the disk is opened read-only
    and a decrypting disk_t wrapper presents the plaintext payload.

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
#ifndef _LUKSDEC_H
#define _LUKSDEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "common.h"

/*
 * Unlock the LUKS volume located at part_offset on base disk using the
 * given passphrase and return a read-only decrypting disk_t wrapper.
 *
 * The returned disk presents the decrypted payload starting at offset 0,
 * exactly like a /dev/mapper device would, so the existing partition
 * detection and filesystem drivers work unchanged.
 *
 * The wrapper borrows base (it is NOT freed by the wrapper's clean()).
 * base must therefore outlive the returned disk.
 *
 * Returns a new disk_t* on success, NULL on failure (wrong passphrase,
 * unsupported cipher, malformed header, ...).
 */
disk_t *luksdec_open(disk_t *base, uint64_t part_offset, const char *passphrase);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
