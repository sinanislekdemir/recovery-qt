// SPDX-License-Identifier: GPL-2.0-or-later

#include "progress.h"

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
