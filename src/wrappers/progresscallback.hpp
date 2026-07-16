/*
    
    File: progresscallback.hpp

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
#ifndef PHOTOREC_PROGRESSCALLBACK_HPP
#define PHOTOREC_PROGRESSCALLBACK_HPP

#include <QObject>
#include <QString>
#include <functional>
#include <atomic>
#include <cstdint>

class ProgressCallback : public QObject {
  Q_OBJECT
public:
  static ProgressCallback *instance();

  explicit ProgressCallback(QObject *parent = nullptr);

  void reset();
  bool isCancelled() const;

public slots:
  void cancel();

signals:
  void progressChanged(int percent, const QString &status);
  void scannerProgress(uint64_t deletedCount, uint64_t totalCount, const QString &path);
  void carverProgress(uint64_t scannedBytes, uint64_t totalBytes, unsigned int fileCount, uint64_t recoveredSize);
  void restoreProgress(int pct, const QString &currentFile, int total, int done);
  void fileRestored(const QString &path, bool ok);
  void scannerIndxProgress(const QString &msg, uint64_t current, uint64_t total, uint64_t found);
  void checkpointProgress(uint64_t progress1, uint64_t progress2);

private:
  std::atomic<bool> m_cancelled;
  static ProgressCallback *s_instance;

  template <typename Fn> static void emitToInstance(ProgressCallback *instance, Fn &&fn) {
    if (!instance)
      return;
    QMetaObject::invokeMethod(instance, std::forward<Fn>(fn), Qt::QueuedConnection);
  }

  static void cScannerProgress(uint64_t deleted, uint64_t total, const char *path);
  static void cScannerIndxProgress(const char *msg, uint64_t cur, uint64_t tot, uint64_t found);
  static int cIsCancelled();

  static void cCarverProgress(uint64_t scanned, uint64_t total, unsigned int files, uint64_t recovered);
  static int cCarverCancelled();

  static void cRestoreProgress(int pct, const char *file, int total, int done);
  static void cRestoreFile(const char *path, int ok);
  static int cRestoreCancelled();

public:
  void installCarverCallbacks();
  void installScannerCallbacks();
  void installRestoreCallbacks();
  void installCheckpointCallback();
  void uninstallAllCallbacks();

  static ProgressCallback *s_carverInstance;
  static ProgressCallback *s_scannerInstance;
  static ProgressCallback *s_restoreInstance;
  static ProgressCallback *s_checkpointInstance;

  static void cCheckpointProgress(uint64_t progress1, uint64_t progress2);
};

#endif // PHOTOREC_PROGRESSCALLBACK_HPP
