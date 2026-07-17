/*
    
    File: mainwindow.hpp

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
#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QStackedWidget>
#include <QVector>
#include "wrappers/disk.hpp"
#include "wrappers/partitionlist.hpp"
#include "wrappers/filetreemodel.hpp"
#include "wrappers/progresscallback.hpp"
#include "wrappers/signatureregistry.hpp"

class DiskSelectionWidget;
class PartitionSelectionWidget;
class BrowserWidget;
class FormatSelectorDialog;
class LUKSPasswordDialog;
class Scanner;
class Carver;
class Restorer;
class LUKSManager;
class SimpleWorker;
class SessionManager;
class GhostFinder;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private slots:
  void onDiskSelected(const Disk &disk);
  void onScanRequested();
  void onCarveRequested();
  void onDeepScanRequested();
  void onBackupRequested();
  void onRestoreRequested();
  void onBackToDisks();
  void onRestoreFromBrowser();
  void onPreviewRequested(const QModelIndex &idx);
  void onBrowserQuit();
  void onAbout();
  void onContinueSession();
  void onFindPartitionsRequested();

private:
  void setupUi();
  void applyTheme();
  void showBrowser();
  void detectPartitions();
  void runScannerOperation(bool deep);
  partition_t *decryptLUKSAndRedetect();

  QStackedWidget *m_stack;
  DiskSelectionWidget *m_diskPage;
  PartitionSelectionWidget *m_partPage;
  BrowserWidget *m_browserPage;
  FileTreeModel *m_fileModel;
  ProgressCallback *m_progressCb;

  Scanner *m_scanner;
  Carver *m_carver;
  Restorer *m_restorer;
  SimpleWorker *m_simpleWorker;
  LUKSManager *m_luks;
  SessionManager *m_sessionManager;
  GhostFinder *m_ghostFinder;

  Disk m_currentDisk;
  QVector<PartitionInfo> m_partitions;
  PartitionList m_partList;
  int m_selectedPartIdx;
  scan_tree_t *m_scanTree;
};

#endif // MAINWINDOW_HPP
