// SPDX-License-Identifier: GPL-2.0-or-later
/*
    File: progress.h

    Callback types and global pointers for progress reporting.
    Set by Qt wrapper layer before calling C scanner/carver/restorer.
 */
#ifndef _PROGRESS_H
#define _PROGRESS_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

typedef void (*scanner_progress_fn)(uint64_t deleted_count, uint64_t total_count, const char *current_path);
typedef void (*scanner_indx_progress_fn)(const char *msg, uint64_t current, uint64_t total, uint64_t found);

typedef void (*carver_progress_fn)(uint64_t scanned_bytes, uint64_t total_bytes,
    unsigned int file_count, uint64_t recovered_size);
typedef void (*carver_format_fn)(const char *ext, unsigned int count, uint64_t size);

typedef void (*restorer_progress_fn)(int pct, const char *current_file, int total, int done);
typedef void (*restorer_file_fn)(const char *path, int ok);

typedef int (*progress_cancel_fn)(void);

typedef void (*checkpoint_fn)(uint64_t progress1, uint64_t progress2);
extern checkpoint_fn g_checkpoint_progress;

typedef int (*session_save_cb_fn)(uint64_t progress1, uint64_t progress2);
extern session_save_cb_fn g_session_save_cb;

extern scanner_progress_fn g_scanner_progress;
extern scanner_indx_progress_fn g_scanner_indx_progress;
extern progress_cancel_fn g_scanner_cancel;

extern carver_progress_fn g_carver_progress;
extern carver_format_fn g_carver_format;
extern progress_cancel_fn g_carver_cancel;

extern restorer_progress_fn g_restorer_progress;
extern restorer_file_fn g_restorer_file;
extern progress_cancel_fn g_restorer_cancel;

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif /* _PROGRESS_H */
