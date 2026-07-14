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
    if (!s_scannerInstance)
        return;
    QMetaObject::invokeMethod(s_scannerInstance, [=]() {
        emit s_scannerInstance->scannerProgress(deleted, total,
            path ? QString::fromUtf8(path) : QString());
    }, Qt::QueuedConnection);
}

void ProgressCallback::cScannerIndxProgress(const char *msg, uint64_t cur, uint64_t tot, uint64_t found)
{
    if (!s_scannerInstance)
        return;
    QMetaObject::invokeMethod(s_scannerInstance, [=]() {
        emit s_scannerInstance->scannerIndxProgress(
            msg ? QString::fromUtf8(msg) : QString(), cur, tot, found);
    }, Qt::QueuedConnection);
}

int ProgressCallback::cIsCancelled()
{
    return s_scannerInstance ? s_scannerInstance->isCancelled() : 0;
}

void ProgressCallback::cCarverProgress(uint64_t scanned, uint64_t total,
    unsigned int files, uint64_t recovered)
{
    if (!s_carverInstance)
        return;
    QMetaObject::invokeMethod(s_carverInstance, [=]() {
        emit s_carverInstance->carverProgress(scanned, total, files, recovered);
    }, Qt::QueuedConnection);
}

int ProgressCallback::cCarverCancelled()
{
    return s_carverInstance ? s_carverInstance->isCancelled() : 0;
}

void ProgressCallback::cRestoreProgress(int pct, const char *file, int total, int done)
{
    if (!s_restoreInstance)
        return;
    QMetaObject::invokeMethod(s_restoreInstance, [=]() {
        emit s_restoreInstance->restoreProgress(pct,
            file ? QString::fromUtf8(file) : QString(), total, done);
    }, Qt::QueuedConnection);
}

void ProgressCallback::cRestoreFile(const char *path, int ok)
{
    if (!s_restoreInstance)
        return;
    QMetaObject::invokeMethod(s_restoreInstance, [=]() {
        emit s_restoreInstance->fileRestored(
            path ? QString::fromUtf8(path) : QString(), ok != 0);
    }, Qt::QueuedConnection);
}

int ProgressCallback::cRestoreCancelled()
{
    return s_restoreInstance ? s_restoreInstance->isCancelled() : 0;
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
    s_carverInstance = nullptr;
    s_scannerInstance = nullptr;
    s_restoreInstance = nullptr;
}
