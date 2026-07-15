/*
    
    File: progresscallback.cpp

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
#include "progresscallback.hpp"
#include <QMetaObject>
#include <QCoreApplication>

#ifdef __cplusplus
extern "C" {
#endif
#include "progress_cb.h"
#ifdef __cplusplus
}
#endif

ProgressCallback* ProgressCallback::s_instance = nullptr;
ProgressCallback* ProgressCallback::s_carverInstance = nullptr;
ProgressCallback* ProgressCallback::s_scannerInstance = nullptr;
ProgressCallback* ProgressCallback::s_restoreInstance = nullptr;
ProgressCallback* ProgressCallback::s_checkpointInstance = nullptr;

ProgressCallback* ProgressCallback::instance()
{
    if (!s_instance)
        s_instance = new ProgressCallback();
    return s_instance;
}

ProgressCallback::ProgressCallback(QObject *parent)
    : QObject(parent), m_cancelled(false) {}

void ProgressCallback::reset() { m_cancelled.store(false); }
void ProgressCallback::cancel() { m_cancelled.store(true); }
bool ProgressCallback::isCancelled() const { return m_cancelled.load(); }

void ProgressCallback::cScannerProgress(uint64_t deleted, uint64_t total, const char *path)
{
    emitToInstance(s_scannerInstance, [=]() {
        emit s_scannerInstance->scannerProgress(deleted, total,
            path ? QString::fromUtf8(path) : QString());
    });
}

void ProgressCallback::cScannerIndxProgress(const char *msg, uint64_t cur, uint64_t tot, uint64_t found)
{
    emitToInstance(s_scannerInstance, [=]() {
        emit s_scannerInstance->scannerIndxProgress(
            msg ? QString::fromUtf8(msg) : QString(), cur, tot, found);
    });
}

int ProgressCallback::cIsCancelled()
{
    return s_scannerInstance ? s_scannerInstance->isCancelled() : 0;
}

void ProgressCallback::cCarverProgress(uint64_t scanned, uint64_t total,
    unsigned int files, uint64_t recovered)
{
    emitToInstance(s_carverInstance, [=]() {
        emit s_carverInstance->carverProgress(scanned, total, files, recovered);
    });
}

int ProgressCallback::cCarverCancelled()
{
    return s_carverInstance ? s_carverInstance->isCancelled() : 0;
}

void ProgressCallback::cRestoreProgress(int pct, const char *file, int total, int done)
{
    emitToInstance(s_restoreInstance, [=]() {
        emit s_restoreInstance->restoreProgress(pct,
            file ? QString::fromUtf8(file) : QString(), total, done);
    });
}

void ProgressCallback::cRestoreFile(const char *path, int ok)
{
    emitToInstance(s_restoreInstance, [=]() {
        emit s_restoreInstance->fileRestored(
            path ? QString::fromUtf8(path) : QString(), ok != 0);
    });
}

int ProgressCallback::cRestoreCancelled()
{
    return s_restoreInstance ? s_restoreInstance->isCancelled() : 0;
}

void ProgressCallback::cCheckpointProgress(uint64_t progress1, uint64_t progress2)
{
    emitToInstance(s_checkpointInstance, [=]() {
        emit s_checkpointInstance->checkpointProgress(progress1, progress2);
    });
}

void ProgressCallback::installCarverCallbacks()
{
    s_carverInstance = this;
    g_carver_progress = cCarverProgress;
    g_carver_cancel = cCarverCancelled;
}

void ProgressCallback::installScannerCallbacks()
{
    s_scannerInstance = this;
    g_scanner_progress = cScannerProgress;
    g_scanner_indx_progress = cScannerIndxProgress;
    g_scanner_cancel = cIsCancelled;
}

void ProgressCallback::installRestoreCallbacks()
{
    s_restoreInstance = this;
    g_restorer_progress = cRestoreProgress;
    g_restorer_file = cRestoreFile;
    g_restorer_cancel = cRestoreCancelled;
}

void ProgressCallback::installCheckpointCallback()
{
    s_checkpointInstance = this;
    g_checkpoint_progress = cCheckpointProgress;
}

void ProgressCallback::uninstallAllCallbacks()
{
    g_carver_progress = 0;
    g_carver_cancel = 0;
    g_scanner_progress = 0;
    g_scanner_indx_progress = 0;
    g_scanner_cancel = 0;
    g_restorer_progress = 0;
    g_restorer_file = 0;
    g_restorer_cancel = 0;
    g_checkpoint_progress = 0;
    g_session_save_cb = 0;
    s_carverInstance = nullptr;
    s_scannerInstance = nullptr;
    s_restoreInstance = nullptr;
    s_checkpointInstance = nullptr;
}
