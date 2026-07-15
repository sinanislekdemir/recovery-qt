/*
    
    File: progress_cb.c

    Copyright (C) 2025 Sinan Islekdemir <sinan@islekdemir.com>

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

#include "progress_cb.h"

checkpoint_fn g_checkpoint_progress = 0;
session_save_cb_fn g_session_save_cb = 0;

scanner_progress_fn g_scanner_progress = 0;
scanner_indx_progress_fn g_scanner_indx_progress = 0;
progress_cancel_fn g_scanner_cancel = 0;

carver_progress_fn g_carver_progress = 0;
carver_format_fn g_carver_format = 0;
progress_cancel_fn g_carver_cancel = 0;

restorer_progress_fn g_restorer_progress = 0;
restorer_file_fn g_restorer_file = 0;
progress_cancel_fn g_restorer_cancel = 0;
