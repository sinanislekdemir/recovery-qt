/*

    File: main.cpp

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
#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif
#include <errno.h>
#include "ui/mainwindow.hpp"

extern "C" {
#include "log.h"
#include "hdaccess.h"
#include "luks.h"
#include "luksdec.h"
#include "fat.h"
#include "ntfs.h"
#include "ext2.h"
#include "exfat.h"
#include "iso.h"
#include "partauto.h"
#include "fnctdsk.h"
#include "recovery.h"
extern const arch_fnct_t arch_none;
}

static QString lockFilePath() {
  return QDir::tempPath() + QStringLiteral("/recovery-qt.pid");
}
static QString logFilePath() {
  return QDir::tempPath() + QStringLiteral("/recovery-qt.log");
}

static bool processRunning(qint64 pid) {
#ifdef _WIN32
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
  if (h != NULL) {
    CloseHandle(h);
    return true;
  }
  return false;
#else
  return kill((pid_t)pid, 0) == 0;
#endif
}

static qint64 checkAndCleanupPrevious() {
  QFile lock(lockFilePath());
  if (!lock.exists())
    return 0;
  if (!lock.open(QIODevice::ReadOnly))
    return -1;
  QTextStream in(&lock);
  bool ok = false;
  qint64 stored_pid = in.readLine().trimmed().toLongLong(&ok);
  lock.close();
  if (ok && stored_pid > 1) {
    if (processRunning(stored_pid))
      return stored_pid;
    QFile::remove(lockFilePath());
    return 0;
  }
  QFile::remove(lockFilePath());
  return 0;
}

static bool writeLock() {
  QFile lock(lockFilePath());
  if (!lock.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  QTextStream out(&lock);
  out << QCoreApplication::applicationPid() << "\n";
  lock.close();
  return true;
}

#ifndef _WIN32
static QStringList collectDisplayEnv() {
  QStringList env;
  auto add = [&](const char *name) {
    QByteArray val = qgetenv(name);
    if (!val.isEmpty())
      env << QString::fromLatin1(name) + "=" + QString::fromLocal8Bit(val);
  };
  add("DISPLAY");
  add("XAUTHORITY");
  add("WAYLAND_DISPLAY");
  add("XDG_RUNTIME_DIR");
  add("DBUS_SESSION_BUS_ADDRESS");
  add("QT_QPA_PLATFORM");
  return env;
}
#endif

/* ----------------------------------------------------------------------- */
/* --headless-test: bypass GUI entirely, run scanner on decrypted volume   */
/* ----------------------------------------------------------------------- */
static int runHeadlessTest(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--headless-test") == 0 && i + 1 < argc) {
      const char *img = argv[++i];
      const char *pass = nullptr;
      for (int j = i; j < argc; j++) {
        if (strcmp(argv[j], "--luks-pass") == 0 && j + 1 < argc)
          pass = argv[++j];
      }
      disk_t *disk = file_test_availability((char *)img, 2, TESTDISK_O_RDONLY);
      if (!disk) {
        fprintf(stderr, "headless: failed to open %s\n", img);
        return 1;
      }
      fprintf(stderr, "headless: opened %s size=%llu\n", img, (unsigned long long)disk->disk_size);
      partition_t *part = partition_new(&arch_none);
      part->part_offset = 0;
      part->part_size = disk->disk_size;
      disk_t *use_disk = disk;
      if (pass) {
        fprintf(stderr, "headless: decrypting LUKS...\n");
        use_disk = luksdec_open(disk, 0, pass);
        if (!use_disk) {
          fprintf(stderr, "headless: decrypt failed\n");
          free(part);
          disk->clean(disk);
          return 1;
        }
        fprintf(stderr, "headless: decrypted size=%llu\n", (unsigned long long)use_disk->disk_size);
        autodetect_arch(use_disk, nullptr);
        fprintf(stderr, "headless: arch=%s\n", use_disk->arch ? use_disk->arch->part_name : "(null)");
        if (use_disk->arch) {
          list_part_t *lp = use_disk->arch->read_part(use_disk, 0, 0);
          for (list_part_t *it = lp; it != nullptr; it = it->next) {
            if (it->part) {
              check_LUKS(use_disk, it->part);
              fprintf(stderr, "headless: part fsname=%s upart=%d\n", it->part->fsname, (int)it->part->upart_type);
            }
          }
          free(part);
          part = partition_new(&arch_none);
          part->part_offset = 0;
          part->part_size = use_disk->disk_size;
          check_EXT2(use_disk, part, 0);
          check_NTFS(use_disk, part, 0, 0);
          check_FAT(use_disk, part, 0);
          check_exFAT(use_disk, part);
          check_ISO(use_disk, part);
          part_free_list(lp);
        }
      }
      fprintf(stderr, "headless: fsname=%s upart=%d\n", part->fsname, (int)part->upart_type);
      scan_tree_t *tree = tree_new();
      fprintf(stderr, "headless: running scanner...\n");
      int rv = scanner_run(tree, use_disk, part, false);
      fprintf(stderr, "headless: scanner_run=%d\n", rv);
      tree_free(tree);
      if (use_disk != disk && use_disk->clean)
        use_disk->clean(use_disk);
      disk->clean(disk);
      free(part);
      return rv;
    }
  }
  return -1; /* no --headless-test flag found */
}

/* ----------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
  int ht = runHeadlessTest(argc, argv);
  if (ht >= 0)
    return ht;

#ifndef _WIN32
  if (geteuid() != 0) {
    QApplication app(argc, argv);
    app.setApplicationName("recovery-qt");
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle("Root Privileges Required");
    msgBox.setText("recovery-qt needs root privileges to access disk devices directly.");
    msgBox.setInformativeText("The application will restart with administrative privileges.\n"
                              "You may be prompted for your password.");
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);
    if (msgBox.exec() == QMessageBox::Cancel)
      return 0;
    QString binPath = QDir::cleanPath(QCoreApplication::applicationFilePath());
    QStringList pkexecArgs;
    pkexecArgs << "env";
    pkexecArgs.append(collectDisplayEnv());
    pkexecArgs << ("RECOVERY_QT_CWD=" + QDir::currentPath());
    pkexecArgs << binPath;
    for (int i = 1; i < argc; i++)
      pkexecArgs << QString::fromLocal8Bit(argv[i]);
    if (!QProcess::startDetached("pkexec", pkexecArgs)) {
      QMessageBox::critical(nullptr, "Error", "Failed to launch pkexec. Please install policykit-1 or run with sudo.");
      return 1;
    }
    return 0;
  }
#endif

  QApplication app(argc, argv);
  app.setApplicationName("recovery-qt");
  app.setApplicationVersion("7.3");
  app.setOrganizationName("recovery-qt");

  {
    QFile test(logFilePath());
    if (test.open(QIODevice::WriteOnly)) {
      test.write("recovery-qt log\n", 17);
      test.close();
    }
    int log_errno;
    int ok = log_open(logFilePath().toLocal8Bit().constData(), TD_LOG_APPEND, &log_errno);
    if (!ok) {
      QString msg = QStringLiteral("Cannot create %1 (errno=%2)").arg(logFilePath()).arg(log_errno);
      qDebug() << msg;
      QMessageBox::warning(nullptr, "recovery-qt", msg);
    }
  }

  {
    qint64 prev_pid = checkAndCleanupPrevious();
    if (prev_pid > 0) {
      QMessageBox::warning(nullptr, "recovery-qt", "Only one instance of recovery-qt can run at a time.");
      return 0;
    }
    if (!writeLock()) {
      QMessageBox::critical(nullptr, "recovery-qt", "Failed to create lock file " + lockFilePath());
      return 1;
    }
  }

  MainWindow window;
  window.show();

  int ret = app.exec();
  QFile::remove(lockFilePath());
  return ret;
}
