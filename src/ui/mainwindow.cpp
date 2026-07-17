/*
    
    File: mainwindow.cpp

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
#include "mainwindow.hpp"
#include "diskselectionwidget.hpp"
#include "partitionselectionwidget.hpp"
#include "browserwidget.hpp"
#include "progressdialog.hpp"
#include "formatselectordialog.hpp"
#include "lukspassworddialog.hpp"
#include "aboutdialog.hpp"
#include "imagepreviewdialog.hpp"
#include "wrappers/scanner.hpp"
#include "wrappers/carver.hpp"
#include "wrappers/restorer.hpp"
#include "wrappers/simpleworker.hpp"
#include "wrappers/luksmanager.hpp"
#include "wrappers/signatureregistry.hpp"
#include "wrappers/sessionmanager.hpp"
#include "wrappers/ghostfinder.hpp"
#include "common/format_utils.hpp"
#include "common/theme.hpp"
#include "recovery.h"
#include <QDebug>
#include <QStandardItemModel>
#include <QMenuBar>
#include <QEventLoop>

extern "C" {
#include "backup.h"
}
extern "C" {
extern const arch_fnct_t arch_none;
}
#include "ghostscan.h"
extern "C" {
#include "fnctdsk.h"
}
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QDir>
#include <QInputDialog>
#include <QStatusBar>
#include <QTimer>
#include <QImage>
#include <cstring>

static partition_t *partitionFromGhostInfo(const PartitionInfo &info) {
  partition_t *part = partition_new(&arch_none);
  if (!part)
    return nullptr;
  part->part_offset = info.partOffset;
  part->part_size = info.partSize;
  part->upart_type = info.upartType;
  part->status = info.status;
  part->part_type_i386 = info.partTypeI386;
  strncpy(part->fsname, info.fsname.toLocal8Bit().constData(), sizeof(part->fsname) - 1);
  strncpy(part->partname, info.partname.toLocal8Bit().constData(), sizeof(part->partname) - 1);
  strncpy(part->info, info.info.toLocal8Bit().constData(), sizeof(part->info) - 1);
  return part;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_stack(nullptr), m_diskPage(nullptr), m_partPage(nullptr), m_browserPage(nullptr),
      m_fileModel(nullptr), m_progressCb(nullptr), m_scanner(nullptr), m_carver(nullptr), m_restorer(nullptr),
      m_luks(new LUKSManager(this)), m_sessionManager(new SessionManager(this)), m_ghostFinder(new GhostFinder(this)),
      m_selectedPartIdx(-1), m_scanTree(nullptr)
/*
 * ARCHITECTURE NOTE: Operation Lifecycle Pattern
 *
 * All scan/carve/restore/deep-scan operations follow this identical flow:
 *   1. Validate selection, get partition pointer
 *   2. Handle LUKS decryption if encrypted
 *   3. Free + re-allocate m_scanTree via tree_new()
 *   4. Create ProgressDialog, wire progress signals via ProgressCallback bridge
 *   5. Call worker->start() which launches a QThread running C core code
 *   6. dlg.exec() blocks the UI until finished or cancelled
 *   7. uninstallAllCallbacks() on return
 *   8. showBrowser() to display results
 *
 * WARNING: Major code duplication. onScanRequested, onDeepScanRequested,
 * onCarveRequested, and onRestoreFromBrowser repeat steps 1-8 almost verbatim.
 * A unified runOperation() helper accepting a lambda for the specific worker
 * call would consolidate ~200 lines of duplicate progress-dialog wiring.
 * The backup/restore operations (onBackupRequested, onRestoreRequested)
 * use a slightly different pattern (QThread::create directly instead of
 * WorkerBase), adding further inconsistency.
 */
{
  m_progressCb = new ProgressCallback(this);
  m_scanner = new Scanner(this);
  m_carver = new Carver(this);
  m_restorer = new Restorer(this);
  m_simpleWorker = new SimpleWorker(this);

  setWindowTitle(tr("recovery-qt - Deleted File Recovery"));
  setMinimumSize(800, 600);
  resize(900, 650);

  setupUi();
  applyTheme();
}

MainWindow::~MainWindow() {
  if (m_scanTree) {
    tree_free(m_scanTree);
    m_scanTree = nullptr;
  }
}

void MainWindow::setupUi() {
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  QAction *quitAction = fileMenu->addAction(tr("&Quit"), this, &QWidget::close);
  quitAction->setShortcut(QKeySequence::Quit);

  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(tr("&About"), this, &MainWindow::onAbout);

  m_stack = new QStackedWidget(this);
  setCentralWidget(m_stack);

  m_diskPage = new DiskSelectionWidget(this);
  m_partPage = new PartitionSelectionWidget(this);
  m_browserPage = new BrowserWidget(this);

  m_stack->addWidget(m_diskPage);
  m_stack->addWidget(m_partPage);
  m_stack->addWidget(m_browserPage);
  m_stack->setCurrentIndex(0);

  m_fileModel = new FileTreeModel(this);

  connect(m_diskPage, &DiskSelectionWidget::diskSelected, this, &MainWindow::onDiskSelected);
  connect(m_diskPage, &DiskSelectionWidget::quitRequested, this, &QWidget::close);
  connect(m_diskPage, &DiskSelectionWidget::continueSessionRequested, this, &MainWindow::onContinueSession);
  connect(m_partPage, &PartitionSelectionWidget::scanRequested, this, &MainWindow::onScanRequested);
  connect(m_partPage, &PartitionSelectionWidget::carveRequested, this, &MainWindow::onCarveRequested);
  connect(m_partPage, &PartitionSelectionWidget::deepScanRequested, this, &MainWindow::onDeepScanRequested);
  connect(m_partPage, &PartitionSelectionWidget::backupRequested, this, &MainWindow::onBackupRequested);
  connect(m_partPage, &PartitionSelectionWidget::restoreRequested, this, &MainWindow::onRestoreRequested);
  connect(m_partPage, &PartitionSelectionWidget::backRequested, this, &MainWindow::onBackToDisks);
  connect(m_partPage, &PartitionSelectionWidget::findPartitionsRequested, this, &MainWindow::onFindPartitionsRequested);
  connect(m_browserPage, &BrowserWidget::restoreRequested, this, &MainWindow::onRestoreFromBrowser);
  connect(m_browserPage, &BrowserWidget::quitRequested, this, &MainWindow::onBrowserQuit);
  connect(m_browserPage, &BrowserWidget::previewRequested, this, &MainWindow::onPreviewRequested);

  connect(m_scanner, &Scanner::errorOccurred, this,
          [this](const QString &msg) { QMessageBox::warning(this, tr("Scan Warning"), msg); });
  connect(m_luks, &LUKSManager::errorOccurred, this,
          [this](const QString &msg) { QMessageBox::critical(this, tr("LUKS Error"), msg); });
}

void MainWindow::applyTheme() {
  setStyleSheet(QStringLiteral("QMainWindow { background-color: #2E3440; }"
                               "QStackedWidget { background-color: #2E3440; }"
                               "QMenuBar {"
                               "  background-color: #3B4252;"
                               "  color: #ECEFF4;"
                               "  border-bottom: 1px solid #4C566A;"
                               "}"
                               "QMenuBar::item:selected {"
                               "  background-color: #88C0D0;"
                               "  color: #2E3440;"
                               "}"
                               "QMenu {"
                               "  background-color: #3B4252;"
                               "  color: #ECEFF4;"
                               "  border: 1px solid #4C566A;"
                               "}"
                               "QMenu::item:selected {"
                               "  background-color: #88C0D0;"
                               "  color: #2E3440;"
                               "}"
                               "QStatusBar {"
                               "  background-color: #3B4252;"
                               "  color: #ECEFF4;"
                               "  border-top: 1px solid #4C566A;"
                               "}"
                               "QMessageBox { background-color: #2E3440; }"
                               "QMessageBox QLabel { color: #ECEFF4; }"
                               "QInputDialog { background-color: #2E3440; }"
                               "QInputDialog QLabel { color: #ECEFF4; }"
                               "QInputDialog QLineEdit {"
                               "  background-color: #3B4252;"
                               "  color: #ECEFF4;"
                               "  border: 1px solid #4C566A;"
                               "}") +
                Theme::globalStyleSheet());
}

void MainWindow::onDiskSelected(const Disk &disk) {
  m_currentDisk = disk;
  detectPartitions();
  m_stack->setCurrentIndex(1);
}

void MainWindow::detectPartitions() {
  m_partList = PartitionList();
  bool detected = m_partList.detect(m_currentDisk);
  qDebug() << "detectPartitions: detect returned" << detected << "count=" << m_partList.count();
  if (!detected) {
    qDebug() << "detectPartitions: falling back to detectWholeDisk";
    m_partList.detectWholeDisk(m_currentDisk);
  }
  m_partitions = m_partList.partitions();
  for (int i = 0; i < m_partitions.size(); i++)
    qDebug() << "  part" << i << "fsname=" << m_partitions[i].fsname << "upartType=" << m_partitions[i].upartType
             << "typename=" << m_partitions[i].typenameStr;
  m_partPage->setPartitions(m_partitions);
}

partition_t *MainWindow::decryptLUKSAndRedetect() {
  LUKSPasswordDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted || dlg.password().isEmpty())
    return nullptr;

  ProgressDialog decryptDlg(this);
  decryptDlg.setWindowTitle(tr("LUKS Decryption"));
  decryptDlg.setStatusText(tr("Decrypting volume, please wait..."));
  decryptDlg.showCancelButton(false);
  decryptDlg.setIndeterminate(true);
  decryptDlg.show();

  bool decryptOk = false;
  QEventLoop loop;
  QMetaObject::Connection conn = connect(m_luks, &LUKSManager::decryptFinished, &loop, [&](bool ok) {
    decryptOk = ok;
    loop.quit();
  });
  m_luks->decryptAsync(m_currentDisk.raw(), m_partitions[m_selectedPartIdx].partOffset, dlg.password());
  loop.exec();
  disconnect(conn);
  decryptDlg.close();

  if (!decryptOk)
    return nullptr;

  Disk decrypted = m_luks->decryptedDisk();
  if (!decrypted.isValid()) {
    QMessageBox::warning(this, tr("LUKS Error"), tr("Failed to open decrypted volume."));
    return nullptr;
  }
  m_currentDisk = std::move(decrypted);
  detectPartitions();
  m_partitions = m_partList.partitions();

  partition_t *part = m_partList.rawAt(0);
  if (!part) {
    QMessageBox::warning(this, tr("Scan Error"), tr("No partitions found on decrypted volume."));
    return nullptr;
  }
  return part;
}

/*
 * Unified scanner operation: handles both normal scan (deep=false) and
 * deep FS scan (deep=true). The deep variant scans free clusters byte-by-byte
 * for deleted directory entries in addition to normal filesystem traversal.
 */
void MainWindow::runScannerOperation(bool deep) {
  m_selectedPartIdx = m_partPage->selectedPartitionIndex();

  partition_t *part = nullptr;
  partition_t *wholeDiskPart = nullptr;

  if (m_selectedPartIdx == -2) {
    wholeDiskPart = m_partList.wholeDiskPartition(m_currentDisk);
    if (!wholeDiskPart)
      return;
    part = wholeDiskPart;
  } else {
    if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
      return;

    const PartitionInfo &pinfo = m_partitions[m_selectedPartIdx];

    if (pinfo.isGhost) {
      part = partitionFromGhostInfo(pinfo);
      if (!part)
        return;
      wholeDiskPart = part;
    } else {
      part = m_partList.rawAt(m_selectedPartIdx);
      if (!part)
        return;
    }

    if (m_partitions[m_selectedPartIdx].encrypted) {
      part = decryptLUKSAndRedetect();
      if (!part)
        return;
    }
  }

  qDebug() << "runScannerOperation: deep=" << deep << "part=" << part->fsname << "offset=" << part->part_offset;

  bool sessionEnabled = false;

  if (m_scanTree) {
    tree_free(m_scanTree);
    m_scanTree = nullptr;
  }
  m_scanTree = tree_new();

  if (deep) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Session Save"), tr("Enable session saving? This lets you resume if interrupted."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
      QString defaultPath = QDir::homePath() + QString("/recovery_session_%1.ses").arg((qlonglong)time(NULL));
      QString sesPath =
          QFileDialog::getSaveFileName(this, tr("Save Session File"), defaultPath, tr("Session files (*.ses)"));
      if (!sesPath.isEmpty()) {
        m_sessionManager->beginSession(sesPath, m_scanTree, m_currentDisk.raw(), part, SESSION_OP_DEEP_SCAN, QString());
        m_progressCb->installCheckpointCallback();
        sessionEnabled = true;
      }
    }
  }

  uint64_t partSize = part->part_size;

  ProgressDialog dlg(this);
  dlg.setWindowTitle(deep ? tr("Deep Filesystem Scan") : tr("Scanning Filesystem"));
  dlg.setStatusText(deep ? tr("Scanning filesystem with deep search...") : tr("Searching for deleted files..."));
  dlg.showFileName(true);
  dlg.showCancelButton(true);
  dlg.setIndeterminate(true);

  connect(&dlg, &ProgressDialog::cancelled, ProgressCallback::instance(), &ProgressCallback::cancel);

  QMetaObject::Connection sConn1 = connect(
      m_scanner, &Scanner::progressUpdated, this, [&dlg](uint64_t deleted, uint64_t total, const QString &path) {
        dlg.setIndeterminate(true);
        dlg.setStatusText(QString("%1 deleted / %2 total").arg(deleted).arg(total));
        if (!path.isEmpty())
          dlg.setFileName(path);
      });

  QMetaObject::Connection sConn2;
  if (deep) {
    sConn2 = connect(m_scanner, &Scanner::indxProgressUpdated, this,
                     [&dlg, partSize](const QString &, uint64_t current, uint64_t total, uint64_t found) {
                       unsigned int pct = (total > 0) ? (unsigned int)(current * 100 / total) : 0;
                       if (pct > 100)
                         pct = 100;
                       dlg.setIndeterminate(false);
                       uint64_t scannedBytes = (total > 0) ? (current * partSize / total) : 0;
                       double scannedGB = scannedBytes / (1024.0 * 1024.0 * 1024.0);
                       double totalGB = partSize / (1024.0 * 1024.0 * 1024.0);
                       dlg.updateProgress((int)pct, QString::asprintf("%.2f / %.2f GB  ·  %llu found", scannedGB,
                                                                      totalGB, (unsigned long long)found));
                     });
  } else {
    sConn2 =
        connect(m_scanner, &Scanner::indxProgressUpdated, this,
                [&dlg](const QString &msg, uint64_t current, uint64_t total, uint64_t found) {
                  unsigned int pct = (total > 0) ? (unsigned int)(current * 100 / total) : 0;
                  if (pct > 100)
                    pct = 100;
                  dlg.setIndeterminate(false);
                  dlg.updateProgress(
                      (int)pct, QString("%1  %2/%3 clusters  (%4 found)").arg(msg).arg(current).arg(total).arg(found));
                });
  }

  bool sessionActive = sessionEnabled;

  connect(m_scanner, &Scanner::finished, this, [&dlg, sConn1, sConn2, deep, this, &sessionActive](int result) {
    disconnect(sConn1);
    disconnect(sConn2);
    if (sessionActive) {
      if (!m_progressCb->isCancelled())
        m_sessionManager->endSession(result);
      sessionActive = false;
    }
    dlg.setFinished(result >= 0, result >= 0 ? (deep ? tr("Deep scan complete.") : tr("Scan complete."))
                                             : tr("No filesystem detected."));
  });

  m_scanner->start(m_scanTree, m_currentDisk.raw(), part, deep ? 1 : 0);
  dlg.showFileName(true);
  dlg.exec();

  m_progressCb->uninstallAllCallbacks();
  if (sessionActive)
    m_sessionManager->cancelSession();
  if (wholeDiskPart)
    free(wholeDiskPart);
  if (m_scanTree && m_scanTree->root)
    showBrowser();
}

void MainWindow::onScanRequested() {
  runScannerOperation(false);
}

void MainWindow::onDeepScanRequested() {
  runScannerOperation(true);
}

void MainWindow::onCarveRequested() {
  m_selectedPartIdx = m_partPage->selectedPartitionIndex();

  partition_t *part = nullptr;
  partition_t *wholeDiskPart = nullptr;

  if (m_selectedPartIdx == -2) {
    wholeDiskPart = m_partList.wholeDiskPartition(m_currentDisk);
    if (!wholeDiskPart)
      return;
    part = wholeDiskPart;
  } else {
    if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
      return;

    const PartitionInfo &cpinfo = m_partitions[m_selectedPartIdx];

    if (cpinfo.isGhost) {
      part = partitionFromGhostInfo(cpinfo);
      if (!part)
        return;
      wholeDiskPart = part;
    } else {
      part = m_partList.rawAt(m_selectedPartIdx);
      if (!part)
        return;
    }

    if (m_partitions[m_selectedPartIdx].encrypted) {
      QMessageBox::StandardButton reply =
          QMessageBox::question(this, tr("LUKS Encryption"), tr("Decrypt LUKS volume before carving?"),
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
      if (reply == QMessageBox::Yes) {
        part = decryptLUKSAndRedetect();
        if (!part)
          return;
      }
    }
  }

  SignatureRegistry sigs;
  FormatSelectorDialog fmtDlg(this);
  QVector<SignatureInfo> infos = sigs.allSignatures();
  fmtDlg.setExtensions(infos);
  if (fmtDlg.exec() != QDialog::Accepted)
    return;

  QStringList selectedExts = fmtDlg.selectedExtensions();
  if (selectedExts.isEmpty())
    return;

  if (m_scanTree) {
    tree_free(m_scanTree);
    m_scanTree = nullptr;
  }
  m_scanTree = tree_new();

  bool sessionEnabled = false;
  {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Session Save"), tr("Enable session saving? This lets you resume if interrupted."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
      QString defaultPath = QDir::homePath() + QString("/recovery_session_%1.ses").arg((qlonglong)time(NULL));
      QString sesPath =
          QFileDialog::getSaveFileName(this, tr("Save Session File"), defaultPath, tr("Session files (*.ses)"));
      if (!sesPath.isEmpty()) {
        QString extStr = selectedExts.join(',');
        m_sessionManager->beginSession(sesPath, m_scanTree, m_currentDisk.raw(), part, SESSION_OP_CARVE, extStr);
        m_progressCb->installCheckpointCallback();
        sessionEnabled = true;
      }
    }
  }

  ProgressDialog dlg(this);
  dlg.setWindowTitle(tr("Carving Files"));
  dlg.setStatusText(tr("Scanning partition for file signatures..."));
  dlg.showCancelButton(true);
  dlg.showFileName(false);

  connect(&dlg, &ProgressDialog::cancelled, ProgressCallback::instance(), &ProgressCallback::cancel);
  QMetaObject::Connection pcConn =
      connect(ProgressCallback::instance(), &ProgressCallback::carverProgress, this,
              [&dlg](uint64_t scanned, uint64_t total, unsigned int files, uint64_t recovered) {
                int pct = (total > 0) ? (int)(scanned * 100 / total) : 0;
                if (pct > 100)
                  pct = 100;
                Q_UNUSED(recovered);
                QString status;
                double scannedF = scanned / (1024.0 * 1024.0 * 1024.0);
                double totalF = total / (1024.0 * 1024.0 * 1024.0);
                status = QString::asprintf("Scanned %.2f / %.2f GB  ·  %u files recovered", scannedF, totalF, files);
                dlg.updateProgress(pct, status);
              });
  bool sessionActive = sessionEnabled;

  connect(m_carver, &Carver::finished, this, [&dlg, pcConn, this, &sessionActive](int result) {
    disconnect(pcConn);
    if (sessionActive) {
      if (!m_progressCb->isCancelled())
        m_sessionManager->endSession(result);
      sessionActive = false;
    }
    dlg.setFinished(result > 0,
                    result > 0 ? QString("Carving complete. %1 files found.").arg(result) : tr("No files recovered."));
  });

  sigs.setEnabledExtensions(selectedExts);
  m_carver->start(m_scanTree, m_currentDisk.raw(), part, selectedExts.join(','), false);

  dlg.exec();
  m_progressCb->uninstallAllCallbacks();
  if (sessionActive)
    m_sessionManager->cancelSession();
  if (wholeDiskPart)
    free(wholeDiskPart);
  if (m_scanTree && m_scanTree->root)
    showBrowser();
}
void MainWindow::onFindPartitionsRequested() {
  QStringList modes = {tr("Quick (1 MB stride)"), tr("Thorough (4 KB stride)"), tr("Forensic (512 B stride)")};
  bool ok = false;
  QString choice =
      QInputDialog::getItem(this, tr("Scan Mode"), tr("Select scanning thoroughness:"), modes, 0, false, &ok);
  if (!ok)
    return;

  uint64_t strideSectors = GHOSTSCAN_STRIDE_QUICK;
  if (choice == modes[1])
    strideSectors = GHOSTSCAN_STRIDE_THOROUGH;
  else if (choice == modes[2])
    strideSectors = GHOSTSCAN_STRIDE_FORENSIC;

  disk_t *rawDisk = m_currentDisk.raw();
  if (!rawDisk)
    return;

  ProgressDialog dlg(this);
  dlg.setWindowTitle(tr("Finding Deleted Partitions"));
  dlg.setStatusText(tr("Scanning disk for filesystem signatures..."));
  dlg.showCancelButton(true);
  dlg.showFileName(false);
  dlg.setIndeterminate(true);

  connect(&dlg, &ProgressDialog::cancelled, m_ghostFinder, &GhostFinder::cancel);

  {
    bool clearedAny = false;
    for (int i = m_partitions.size() - 1; i >= 0; i--) {
      if (m_partitions[i].isGhost) {
        m_partitions.removeAt(i);
        clearedAny = true;
      }
    }
    if (clearedAny)
      m_partPage->setPartitions(m_partitions);
  }

  disconnect(m_ghostFinder, &GhostFinder::finished, nullptr, nullptr);

  QMetaObject::Connection conn1 =
      connect(m_ghostFinder, &GhostFinder::progressUpdated, this,
              [&dlg, rawDisk](uint64_t sectorsScanned, uint64_t totalSectors) {
                uint64_t totalGB = (totalSectors * rawDisk->sector_size) / (1024ULL * 1024ULL * 1024ULL);
                uint64_t scannedGB = (sectorsScanned * rawDisk->sector_size) / (1024ULL * 1024ULL * 1024ULL);
                unsigned int pct = (totalSectors > 0) ? (unsigned int)(sectorsScanned * 100 / totalSectors) : 0;
                if (pct > 100)
                  pct = 100;
                dlg.setIndeterminate(false);
                dlg.updateProgress((int)pct, QString::asprintf("%llu / %llu GB scanned", (unsigned long long)scannedGB,
                                                               (unsigned long long)totalGB));
              });

  list_part_t *ghostList = nullptr;
  QMetaObject::Connection conn2 =
      connect(m_ghostFinder, &GhostFinder::finished, this, [&dlg, conn1, &conn2](int count) {
        disconnect(conn1);
        disconnect(conn2);
        if (count >= 0)
          dlg.setFinished(true, tr("Found %1 possible deleted partition(s).").arg(count));
        else
          dlg.setFinished(false, tr("No deleted partitions found."));
      });

  m_ghostFinder->start(rawDisk, m_partList.rawList(), strideSectors);
  dlg.exec();

  ghostList = m_ghostFinder->resultList();
  if (ghostList) {
    for (list_part_t *p = ghostList; p != NULL; p = p->next) {
      if (!p->part)
        continue;
      PartitionInfo info;
      info.fsname = QString::fromLocal8Bit(p->part->fsname);
      info.partname = QString::fromLocal8Bit(p->part->partname);
      info.info = QString::fromLocal8Bit(p->part->info);
      info.partOffset = p->part->part_offset;
      info.partSize = p->part->part_size;
      info.partTypeI386 = p->part->part_type_i386;
      info.upartType = p->part->upart_type;
      info.status = p->part->status;
      info.order = p->part->order;
      info.encrypted = (strncmp(p->part->fsname, "LUKS", 4) == 0);
      info.isGhost = true;
      m_partitions.append(info);
    }
    part_free_list(ghostList);
    m_partPage->setPartitions(m_partitions);
  }
}

/*
 * BACKUP operation: Live filesystem index backup to .dsk file.
 * Now uses SimpleWorker (WorkerBase subclass) instead of raw QThread::create().
 */
void MainWindow::onBackupRequested() {
  m_selectedPartIdx = m_partPage->selectedPartitionIndex();
  if (m_selectedPartIdx < -1)
    m_selectedPartIdx = 0;
  if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
    return;

  partition_t *part = m_partList.rawAt(m_selectedPartIdx);
  partition_t *ghostPart = nullptr;
  if (!part && m_partitions[m_selectedPartIdx].isGhost) {
    ghostPart = partitionFromGhostInfo(m_partitions[m_selectedPartIdx]);
    part = ghostPart;
  }
  if (!part)
    return;

  QString destDir = QFileDialog::getExistingDirectory(this, tr("Select Backup Destination"));
  if (destDir.isEmpty())
    return;

  ProgressDialog dlg(this);
  dlg.setWindowTitle(tr("Creating Backup"));
  dlg.setStatusText(tr("Writing filesystem backup..."));
  dlg.showCancelButton(false);
  dlg.setIndeterminate(true);
  dlg.show();

  QByteArray dirBytes = destDir.toLocal8Bit();
  disk_t *diskPtr = m_currentDisk.raw();

  connect(m_simpleWorker, &SimpleWorker::finished, this, [&dlg](int result) {
    dlg.setFinished(result == 0, result == 0 ? tr("Backup created successfully.") : tr("Backup failed."));
  });

  m_simpleWorker->start([diskPtr, part, dirBytes]() { return backup_create(diskPtr, part, dirBytes.constData()); });

  dlg.exec();

  if (ghostPart)
    free(ghostPart);
}

/*
 * RESTORE-FROM-BACKUP operation: Parse .dsk file and compare against live FS.
 * Now uses SimpleWorker for consistent WorkerBase lifecycle.
 */
void MainWindow::onRestoreRequested() {
  m_selectedPartIdx = m_partPage->selectedPartitionIndex();
  if (m_selectedPartIdx < -1)
    m_selectedPartIdx = 0;
  if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
    return;

  partition_t *part = m_partList.rawAt(m_selectedPartIdx);
  partition_t *restoreGhostPart = nullptr;
  if (!part && m_partitions[m_selectedPartIdx].isGhost) {
    restoreGhostPart = partitionFromGhostInfo(m_partitions[m_selectedPartIdx]);
    part = restoreGhostPart;
  }
  if (!part)
    return;

  QString backupPath = QFileDialog::getOpenFileName(this, tr("Select Backup File"), QString(),
                                                    tr("Backup Files (*.dsk);;All Files (*)"));
  if (backupPath.isEmpty())
    return;

  if (m_scanTree) {
    tree_free(m_scanTree);
    m_scanTree = nullptr;
  }
  m_scanTree = tree_new();

  ProgressDialog dlg(this);
  dlg.setWindowTitle(tr("Restoring from Backup"));
  dlg.setStatusText(tr("Reading backup entries..."));
  dlg.showCancelButton(false);
  dlg.setIndeterminate(true);
  dlg.show();

  QByteArray pathBytes = backupPath.toLocal8Bit();
  disk_t *diskPtr = m_currentDisk.raw();
  scan_tree_t *tree = m_scanTree;

  connect(m_simpleWorker, &SimpleWorker::finished, this, [&dlg](int result) {
    dlg.setFinished(result == 0, result == 0 ? tr("Restore complete.") : tr("Restore failed."));
  });

  m_simpleWorker->start(
      [tree, diskPtr, part, pathBytes]() { return backup_restore(tree, diskPtr, part, pathBytes.constData()); });

  dlg.exec();
  m_progressCb->uninstallAllCallbacks();

  if (restoreGhostPart)
    free(restoreGhostPart);

  if (m_scanTree)
    showBrowser();
}

void MainWindow::onBackToDisks() {
  m_stack->setCurrentIndex(0);
  m_diskPage->refreshDisks();
}

/*
 * RESTORE operation: Copy marked files from scan/carve tree to local disk.
 * From BrowserWidget, user marks files and presses F5.
 * For NORMAL files: uses FS-specific copy_file() drivers (fat/ntfs/ext).
 * For ORPHAN/BACKUP files: reads raw sectors from disk via first_sector/cluster_list.
 *
 * Progress bridged through ProgressCallback::restoreProgress signal chain.
 * Uses Restorer worker (restore_files() in C core, src/prestore.c).
 *
 * DUPLICATION: Same ProgressDialog→worker→exec→uninstallCallbacks pattern
 * as onScanRequested/onCarveRequested/onDeepScanRequested.
 */
void MainWindow::onRestoreFromBrowser() {
  if (!m_fileModel || !m_scanTree)
    return;

  int markedCount = m_fileModel->markedCount();
  if (markedCount == 0) {
    m_browserPage->markSelected();
    markedCount = m_fileModel->markedCount();
  }
  if (markedCount == 0) {
    QMessageBox::information(this, tr("No Files Marked"), tr("Please mark files for recovery first (Space key)."));
    return;
  }

  QString destDir = QFileDialog::getExistingDirectory(this, tr("Select Restore Destination"));
  if (destDir.isEmpty())
    return;

  partition_t *part = nullptr;
  partition_t *wholeDiskPart = nullptr;
  if (m_selectedPartIdx >= 0 && m_selectedPartIdx < m_partitions.size())
    part = m_partList.rawAt(m_selectedPartIdx);
  else if (m_selectedPartIdx == -2)
    wholeDiskPart = m_partList.wholeDiskPartition(m_currentDisk);
  if (!part && !wholeDiskPart) {
    QMessageBox::warning(this, tr("Error"), tr("No valid partition for restore."));
    return;
  }
  if (wholeDiskPart)
    part = wholeDiskPart;

  ProgressDialog dlg(this);
  dlg.setWindowTitle(tr("Restoring Files"));
  dlg.setStatusText(tr("Preparing to restore..."));
  dlg.showFileName(true);
  dlg.showCancelButton(true);

  m_progressCb->reset();

  connect(&dlg, &ProgressDialog::cancelled, ProgressCallback::instance(), &ProgressCallback::cancel);
  QMetaObject::Connection rConn1 =
      connect(ProgressCallback::instance(), &ProgressCallback::restoreProgress, this,
              [&dlg](int pct, const QString &file, int total, int done) {
                dlg.updateProgress(pct, QStringLiteral("File %1 / %2").arg(done).arg(total));
                if (!file.isEmpty())
                  dlg.setFileName(file);
              });
  QMetaObject::Connection rConn2 = connect(ProgressCallback::instance(), &ProgressCallback::fileRestored, this,
                                           [&dlg](const QString &path, bool ok) {
                                             Q_UNUSED(path);
                                             Q_UNUSED(ok);
                                           });
  connect(m_restorer, &Restorer::finished, this, [&dlg, rConn1, rConn2](uint64_t ok, uint64_t fail) {
    disconnect(rConn1);
    disconnect(rConn2);
    if (fail == 0)
      dlg.setFinished(true, QStringLiteral("All %1 files restored successfully.").arg(ok));
    else
      dlg.setFinished(false, QStringLiteral("%1 ok, %2 failed.").arg(ok).arg(fail));
  });

  m_restorer->start(m_scanTree, m_currentDisk.raw(), part, destDir, nullptr);

  dlg.exec();
  m_progressCb->uninstallAllCallbacks();
  if (wholeDiskPart)
    free(wholeDiskPart);
}

void MainWindow::onBrowserQuit() {
  m_stack->setCurrentIndex(0);
  m_diskPage->refreshDisks();
}

/*
 * IMAGE PREVIEW: Read file bytes from disk and display as image.
 * User presses Enter on a file whose extension matches the previewable list
 * (jpg, png, gif, bmp, etc. - see SignatureRegistry::isPreviewableImage()).
 *
 * Data reading path (src/prestore.c:read_file_bytes):
 *   1. cluster_list present? → read from EXT/backup clusters (64MB cap)
 *   2. orphan with first_sector? → read from raw sector offset (64MB cap)
 *   3. normal FS file? → uses memory_capture: runs a fake restore_file_node()
 *      and captures the bytes written by the FS-specific copy_file() driver
 *
 * WARNING: Path 3 (normal FS files) is expensive - it does a full filesystem
 * driver restore just to capture bytes in memory. Consider caching or
 * implementing direct FS-read for preview instead.
 */
void MainWindow::onPreviewRequested(const QModelIndex &idx) {
  QVariant v = idx.data(FileNodeRole);
  if (!v.isValid())
    return;
  file_node_t *node = reinterpret_cast<file_node_t *>(v.value<quintptr>());
  if (!node || node->type != NODE_FILE)
    return;

  const char *ext = strrchr(node->name, '.');
  if (!ext)
    return;
  ext++;
  if (!SignatureRegistry::isPreviewableImage(QString::fromLatin1(ext))) {
    m_browserPage->setStatusMessage(tr("Not an image file: %1").arg(node->name));
    return;
  }

  disk_t *diskPtr = m_currentDisk.raw();
  partition_t *part = nullptr;
  partition_t *wholeDiskPart = nullptr;
  if (m_selectedPartIdx >= 0 && m_selectedPartIdx < m_partitions.size())
    part = m_partList.rawAt(m_selectedPartIdx);
  else if (m_selectedPartIdx == -2)
    wholeDiskPart = m_partList.wholeDiskPartition(m_currentDisk);
  if (!diskPtr || (!part && !wholeDiskPart)) {
    m_browserPage->setStatusMessage(tr("Cannot read disk"));
    return;
  }
  if (wholeDiskPart)
    part = wholeDiskPart;

  size_t dataSize = 0;
  unsigned char *rawData = read_file_bytes(m_scanTree, diskPtr, part, node, &dataSize);
  if (wholeDiskPart)
    free(wholeDiskPart);
  if (!rawData || dataSize == 0) {
    m_browserPage->setStatusMessage(tr("Failed to read file data"));
    return;
  }

  QByteArray buffer((const char *)rawData, (int)dataSize);
  free(rawData);

  ImagePreviewDialog dlg(this);
  if (dlg.loadFromData(buffer)) {
    dlg.setWindowTitle(tr("Image Preview - %1").arg(node->name));
    dlg.exec();
  } else {
    m_browserPage->setStatusMessage(tr("Corrupted image, can't be previewed"));
  }
}

void MainWindow::onAbout() {
  AboutDialog dlg(this);
  dlg.exec();
}

void MainWindow::onContinueSession() {
  QString sesPath = QFileDialog::getOpenFileName(this, tr("Open Session File"), QDir::homePath(),
                                                 tr("Session files (*.ses);;All Files (*)"));
  if (sesPath.isEmpty())
    return;

  SessionInfo *info = SessionManager::loadSession(sesPath);
  if (!info) {
    QMessageBox::warning(this, tr("Load Failed"), tr("Could not read the session file:\n%1").arg(sesPath));
    return;
  }

  Disk sesDisk = Disk::openDevice(info->devicePath, TESTDISK_O_RDONLY);
  if (!sesDisk.isValid()) {
    QMessageBox::warning(this, tr("Disk Not Found"),
                         tr("The original disk '%1' could not be opened.\n"
                            "Please ensure the disk is connected and try again.")
                             .arg(info->devicePath));
    SessionManager::freeSessionInfo(info);
    return;
  }

  if (info->luksDecrypted) {
    LUKSPasswordDialog luksDlg(this);
    if (luksDlg.exec() != QDialog::Accepted || luksDlg.password().isEmpty()) {
      SessionManager::freeSessionInfo(info);
      return;
    }

    ProgressDialog decryptDlg(this);
    decryptDlg.setWindowTitle(tr("LUKS Decryption"));
    decryptDlg.setStatusText(tr("Decrypting volume..."));
    decryptDlg.showCancelButton(false);
    decryptDlg.setIndeterminate(true);
    decryptDlg.show();

    bool ok = false;
    QEventLoop loop;
    QMetaObject::Connection conn = connect(m_luks, &LUKSManager::decryptFinished, &loop, [&](bool success) {
      ok = success;
      loop.quit();
    });
    m_luks->decryptAsync(sesDisk.raw(), info->luksOffset, luksDlg.password());
    loop.exec();
    disconnect(conn);
    decryptDlg.close();

    if (!ok) {
      QMessageBox::warning(this, tr("LUKS Error"), tr("Decryption failed for session volume."));
      SessionManager::freeSessionInfo(info);
      return;
    }

    Disk decrypted = m_luks->decryptedDisk();
    if (!decrypted.isValid()) {
      SessionManager::freeSessionInfo(info);
      return;
    }
    sesDisk = std::move(decrypted);
  }

  m_currentDisk = std::move(sesDisk);
  detectPartitions();

  partition_t *matchingPart = nullptr;
  QString opName;
  matchingPart = nullptr;
  for (int i = 0; i < m_partitions.size(); i++) {
    if (m_partitions[i].partOffset == info->partOffset && m_partitions[i].partSize == info->partSize) {
      matchingPart = m_partList.rawAt(i);
      break;
    }
  }

  if (!matchingPart) {
    QMessageBox::warning(this, tr("Partition Not Found"),
                         tr("The original partition could not be found on this disk.\n"
                            "The disk layout may have changed since the session was saved."));
    SessionManager::freeSessionInfo(info);
    return;
  }

  if (m_scanTree) {
    tree_free(m_scanTree);
    m_scanTree = nullptr;
  }

  if (info->completed && info->tree) {
    m_scanTree = info->tree;
    info->tree = nullptr;
    SessionManager::freeSessionInfo(info);
    showBrowser();
    return;
  }

  m_scanTree = info->tree ? info->tree : tree_new();
  if (info->tree)
    info->tree = nullptr;

  m_sessionManager->setupResume(m_scanTree, m_currentDisk.raw(), info->opType, info);

  if (info->opType == SESSION_OP_CARVE) {
    opName = tr("Resuming Carve");
    ProgressDialog dlg(this);
    dlg.setWindowTitle(opName);
    dlg.setStatusText(tr("Resuming carving from checkpoint..."));
    dlg.showCancelButton(true);
    dlg.showFileName(false);

    connect(&dlg, &ProgressDialog::cancelled, ProgressCallback::instance(), &ProgressCallback::cancel);

    bool sActive = true;
    QMetaObject::Connection pcConn =
        connect(ProgressCallback::instance(), &ProgressCallback::carverProgress, this,
                [&dlg](uint64_t scanned, uint64_t total, unsigned int files, uint64_t recovered) {
                  int pct = (total > 0) ? (int)(scanned * 100 / total) : 0;
                  if (pct > 100)
                    pct = 100;
                  Q_UNUSED(recovered);
                  QString status = QString::asprintf("Scanned %.2f / %.2f GB  ·  %u files recovered",
                                                     scanned / (1024.0 * 1024.0 * 1024.0),
                                                     total / (1024.0 * 1024.0 * 1024.0), files);
                  dlg.updateProgress(pct, status);
                });

    m_sessionManager->beginSession(sesPath, m_scanTree, m_currentDisk.raw(), matchingPart, SESSION_OP_CARVE,
                                   info->extFilter);
    m_progressCb->installCheckpointCallback();

    connect(m_carver, &Carver::finished, this, [&dlg, pcConn, this, &sActive](int result) {
      disconnect(pcConn);
      if (sActive) {
        if (!m_progressCb->isCancelled())
          m_sessionManager->endSession(result);
        sActive = false;
      }
      dlg.setFinished(result > 0, result > 0 ? QString("Carving complete. %1 files found.").arg(result)
                                             : tr("No files recovered."));
    });

    SignatureRegistry sigs;
    if (!info->extFilter.isEmpty())
      sigs.setEnabledExtensions(info->extFilter.split(','));

    m_carver->start(m_scanTree, m_currentDisk.raw(), matchingPart, info->extFilter, false);
    dlg.exec();

    m_progressCb->uninstallAllCallbacks();
    if (sActive)
      m_sessionManager->cancelSession();
  } else if (info->opType == SESSION_OP_DEEP_SCAN) {
    ProgressDialog dlg(this);
    dlg.setWindowTitle(tr("Resuming Deep Scan"));
    dlg.setStatusText(tr("Resuming deep scan from checkpoint..."));
    dlg.showFileName(true);
    dlg.showCancelButton(true);
    dlg.setIndeterminate(true);

    connect(&dlg, &ProgressDialog::cancelled, ProgressCallback::instance(), &ProgressCallback::cancel);

    bool sActive = true;
    auto sConn1 = connect(m_scanner, &Scanner::progressUpdated, this,
                          [&dlg](uint64_t deleted, uint64_t total, const QString &path) {
                            dlg.setIndeterminate(true);
                            dlg.setStatusText(QString("%1 deleted / %2 total").arg(deleted).arg(total));
                            if (!path.isEmpty())
                              dlg.setFileName(path);
                          });

    uint64_t partSize = matchingPart->part_size;
    auto sConn2 = connect(m_scanner, &Scanner::indxProgressUpdated, this,
                          [&dlg, partSize](const QString &, uint64_t current, uint64_t total, uint64_t found) {
                            unsigned int pct = (total > 0) ? (unsigned int)(current * 100 / total) : 0;
                            if (pct > 100)
                              pct = 100;
                            dlg.setIndeterminate(false);
                            uint64_t scannedBytes = (total > 0) ? (current * partSize / total) : 0;
                            dlg.updateProgress((int)pct, QString::asprintf("%.2f / %.2f GB  ·  %llu found",
                                                                           scannedBytes / (1024.0 * 1024.0 * 1024.0),
                                                                           partSize / (1024.0 * 1024.0 * 1024.0),
                                                                           (unsigned long long)found));
                          });

    m_sessionManager->beginSession(sesPath, m_scanTree, m_currentDisk.raw(), matchingPart, SESSION_OP_DEEP_SCAN,
                                   QString());
    m_progressCb->installCheckpointCallback();

    connect(m_scanner, &Scanner::finished, this, [&dlg, sConn1, sConn2, this, &sActive](int result) {
      disconnect(sConn1);
      disconnect(sConn2);
      if (sActive) {
        if (!m_progressCb->isCancelled())
          m_sessionManager->endSession(result);
        sActive = false;
      }
      dlg.setFinished(result >= 0, result >= 0 ? tr("Deep scan complete.") : tr("No filesystem detected."));
    });

    m_scanner->start(m_scanTree, m_currentDisk.raw(), matchingPart, 1);
    dlg.showFileName(true);
    dlg.exec();

    m_progressCb->uninstallAllCallbacks();
    if (sActive)
      m_sessionManager->cancelSession();
  }

  SessionManager::freeSessionInfo(info);

  if (m_scanTree && m_scanTree->root)
    showBrowser();
}

/*
 * Transfers scan_tree_t to FileTreeModel and switches view to BrowserWidget.
 * Called after every scan/carve/deep-scan/backup-restore completes.
 * The FileTreeModel wraps the C tree; BrowserWidget displays via QTreeView.
 */
void MainWindow::showBrowser() {
  if (!m_scanTree)
    return;
  int n = 0;
  struct td_list_head *pos;
  if (m_scanTree->root)
    td_list_for_each(pos, &m_scanTree->root->children) {
      n++;
    }
  qDebug() << "showBrowser: total_files=" << m_scanTree->total_files << "total_dirs=" << m_scanTree->total_dirs
           << "root_children=" << n;

  m_fileModel->setTree(m_scanTree);
  m_browserPage->setFileModel(m_fileModel);
  m_stack->setCurrentIndex(2);
}
