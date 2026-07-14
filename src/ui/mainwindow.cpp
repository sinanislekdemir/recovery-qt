#include "mainwindow.hpp"
#include "diskselectionwidget.hpp"
#include "partitionselectionwidget.hpp"
#include "browserwidget.hpp"
#include "progressdialog.hpp"
#include "formatselectordialog.hpp"
#include "lukspassworddialog.hpp"
#include "aboutdialog.hpp"
#include "wrappers/scanner.hpp"
#include "wrappers/carver.hpp"
#include "wrappers/restorer.hpp"
#include "wrappers/luksmanager.hpp"
#include "wrappers/signatureregistry.hpp"
#include "common/format_utils.hpp"
#include "common/theme.hpp"
#include <QDebug>
#include <QStandardItemModel>
#include <QMenuBar>
#include <QEventLoop>

extern "C" {
#include "pbackup.h"
}
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QStatusBar>
#include <QTimer>
#include <cstring>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_stack(nullptr),
      m_diskPage(nullptr),
      m_partPage(nullptr),
      m_browserPage(nullptr),
      m_fileModel(nullptr),
      m_progressCb(nullptr),
      m_scanner(nullptr),
      m_carver(nullptr),
      m_restorer(nullptr),
      m_luks(new LUKSManager(this)),
      m_selectedPartIdx(-1),
      m_scanTree(nullptr)
{
    m_progressCb = new ProgressCallback(this);
    m_scanner = new Scanner(this);
    m_carver = new Carver(this);
    m_restorer = new Restorer(this);

    setWindowTitle(tr("recovery-qt - Deleted File Recovery"));
    setMinimumSize(800, 600);
    resize(900, 650);

    setupUi();
    applyTheme();
}

MainWindow::~MainWindow()
{
    if (m_scanTree) {
        tree_free(m_scanTree);
        m_scanTree = nullptr;
    }
}

void MainWindow::setupUi()
{
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

    connect(m_diskPage, &DiskSelectionWidget::diskSelected,
            this, &MainWindow::onDiskSelected);
    connect(m_diskPage, &DiskSelectionWidget::quitRequested,
            this, &QWidget::close);
    connect(m_partPage, &PartitionSelectionWidget::scanRequested,
            this, &MainWindow::onScanRequested);
    connect(m_partPage, &PartitionSelectionWidget::carveRequested,
            this, &MainWindow::onCarveRequested);
    connect(m_partPage, &PartitionSelectionWidget::deepScanRequested,
            this, &MainWindow::onDeepScanRequested);
    connect(m_partPage, &PartitionSelectionWidget::backupRequested,
            this, &MainWindow::onBackupRequested);
    connect(m_partPage, &PartitionSelectionWidget::restoreRequested,
            this, &MainWindow::onRestoreRequested);
    connect(m_partPage, &PartitionSelectionWidget::backRequested,
            this, &MainWindow::onBackToDisks);
    connect(m_browserPage, &BrowserWidget::restoreRequested,
            this, &MainWindow::onRestoreFromBrowser);
    connect(m_browserPage, &BrowserWidget::quitRequested,
            this, &MainWindow::onBrowserQuit);

    connect(m_scanner, &Scanner::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::warning(this, tr("Scan Warning"), msg);
    });
    connect(m_luks, &LUKSManager::errorOccurred, this, [this](const QString &msg) {
        QMessageBox::critical(this, tr("LUKS Error"), msg);
    });
}

void MainWindow::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow { background-color: #2E3440; }"
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
        "}"
    ) + Theme::globalStyleSheet());
}

void MainWindow::onDiskSelected(const Disk &disk)
{
    m_currentDisk = disk;
    detectPartitions();
    m_stack->setCurrentIndex(1);
}

void MainWindow::detectPartitions()
{
    m_partList = PartitionList();
    bool detected = m_partList.detect(m_currentDisk);
    qDebug() << "detectPartitions: detect returned" << detected
             << "count=" << m_partList.count();
    if (!detected) {
        qDebug() << "detectPartitions: falling back to detectWholeDisk";
        m_partList.detectWholeDisk(m_currentDisk);
    }
    m_partitions = m_partList.partitions();
    for (int i = 0; i < m_partitions.size(); i++)
        qDebug() << "  part" << i << "fsname=" << m_partitions[i].fsname
                 << "upartType=" << m_partitions[i].upartType
                 << "typename=" << m_partitions[i].typenameStr;
    m_partPage->setPartitions(m_partitions);
}

partition_t *MainWindow::decryptLUKSAndRedetect()
{
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
    QMetaObject::Connection conn = connect(m_luks, &LUKSManager::decryptFinished,
        &loop, [&](bool ok) {
            decryptOk = ok;
            loop.quit();
        });
    m_luks->decryptAsync(m_currentDisk.device(),
                         m_partitions[m_selectedPartIdx].partOffset,
                         dlg.password());
    loop.exec();
    disconnect(conn);
    decryptDlg.close();

    if (!decryptOk)
        return nullptr;

    Disk decrypted = Disk::openDecrypted(m_luks->mapperPath());
    if (!decrypted.isValid()) {
        QMessageBox::warning(this, tr("LUKS Error"),
            tr("Failed to open decrypted device:\n%1").arg(m_luks->mapperPath()));
        return nullptr;
    }
    m_currentDisk = std::move(decrypted);
    detectPartitions();
    m_partitions = m_partList.partitions();

    partition_t *part = m_partList.rawAt(0);
    if (!part) {
        QMessageBox::warning(this, tr("Scan Error"),
            tr("No partitions found on decrypted volume."));
        return nullptr;
    }
    return part;
}

void MainWindow::onScanRequested()
{
    m_selectedPartIdx = m_partPage->selectedPartitionIndex();
    if (m_selectedPartIdx < -1)
        m_selectedPartIdx = 0;

    if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
        return;

    partition_t *part = m_partList.rawAt(m_selectedPartIdx);
    if (!part)
        return;

    if (m_partitions[m_selectedPartIdx].encrypted) {
        part = decryptLUKSAndRedetect();
        if (!part)
            return;
    }

    qDebug() << "onScanRequested: starting scanner, part="
             << part->fsname << "offset=" << part->part_offset;

    if (m_scanTree) {
        tree_free(m_scanTree);
        m_scanTree = nullptr;
    }
    m_scanTree = tree_new();

    ProgressDialog dlg(this);
    dlg.setWindowTitle(tr("Scanning Filesystem"));
    dlg.setStatusText(tr("Searching for deleted files..."));
    dlg.showFileName(true);
    dlg.showCancelButton(true);
    dlg.setIndeterminate(true);

    connect(&dlg, &ProgressDialog::cancelled, ProgressCallback::instance(), &ProgressCallback::cancel);
    QMetaObject::Connection sConn1 = connect(m_scanner, &Scanner::progressUpdated, this, [&dlg](uint64_t deleted, uint64_t total, const QString &path) {
        dlg.setIndeterminate(true);
        dlg.setStatusText(QString("%1 deleted / %2 total").arg(deleted).arg(total));
        if (!path.isEmpty())
            dlg.setFileName(path);
    });
    QMetaObject::Connection sConn2 = connect(m_scanner, &Scanner::indxProgressUpdated, this, [&dlg](const QString &msg, uint64_t current, uint64_t total, uint64_t found) {
        unsigned int pct = (total > 0) ? (unsigned int)(current * 100 / total) : 0;
        if (pct > 100) pct = 100;
        dlg.setIndeterminate(false);
        dlg.updateProgress((int)pct,
            QString("%1  %2/%3 clusters  (%4 found)")
                .arg(msg).arg(current).arg(total).arg(found));
    });
    connect(m_scanner, &Scanner::finished, this, [&dlg, sConn1, sConn2](int result) {
        disconnect(sConn1);
        disconnect(sConn2);
        dlg.setFinished(result >= 0,
            result >= 0 ? tr("Scan complete.") : tr("No filesystem detected."));
    });

    m_scanner->start(m_scanTree, m_currentDisk.raw(), part, 0);
    dlg.showFileName(true);
    dlg.exec();

    m_progressCb->uninstallAllCallbacks();
    if (m_scanTree && m_scanTree->root)
        showBrowser();
}

void MainWindow::onCarveRequested()
{
    m_selectedPartIdx = m_partPage->selectedPartitionIndex();
    if (m_selectedPartIdx < -1)
        m_selectedPartIdx = 0;
    if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
        return;

    partition_t *part = m_partList.rawAt(m_selectedPartIdx);
    if (!part)
        return;

    if (m_partitions[m_selectedPartIdx].encrypted) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("LUKS Encryption"),
            tr("Decrypt LUKS volume before carving?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (reply == QMessageBox::Yes) {
            part = decryptLUKSAndRedetect();
            if (!part)
                return;
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

    ProgressDialog dlg(this);
    dlg.setWindowTitle(tr("Carving Files"));
    dlg.setStatusText(tr("Scanning partition for file signatures..."));
    dlg.showCancelButton(true);
    dlg.showFileName(false);

    connect(&dlg, &ProgressDialog::cancelled, ProgressCallback::instance(), &ProgressCallback::cancel);
    QMetaObject::Connection pcConn = connect(ProgressCallback::instance(), &ProgressCallback::carverProgress,
            this, [&dlg](uint64_t scanned, uint64_t total, unsigned int files, uint64_t recovered) {
        int pct = (total > 0) ? (int)(scanned * 100 / total) : 0;
        if (pct > 100) pct = 100;

        QString status;
        double scannedF = scanned / (1024.0 * 1024.0 * 1024.0);
        double totalF = total / (1024.0 * 1024.0 * 1024.0);
        Q_UNUSED(recovered);
        status = QString::asprintf("Scanned %.2f / %.2f GB  ·  %u files recovered", scannedF, totalF, files);
        dlg.updateProgress(pct, status);
    });
    connect(m_carver, &Carver::finished, this, [&dlg, pcConn](int result) {
        disconnect(pcConn);
        dlg.setFinished(result > 0,
            result > 0 ? QString("Carving complete. %1 files found.").arg(result) : tr("No files recovered."));
    });

    sigs.setEnabledExtensions(selectedExts);
    m_carver->start(m_scanTree, m_currentDisk.raw(), part,
                    selectedExts.join(','), false);

    dlg.exec();
    m_progressCb->uninstallAllCallbacks();
    if (m_scanTree && m_scanTree->root)
        showBrowser();
}

void MainWindow::onDeepScanRequested()
{
    m_selectedPartIdx = m_partPage->selectedPartitionIndex();
    if (m_selectedPartIdx < -1)
        m_selectedPartIdx = 0;
    if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
        return;

    partition_t *part = m_partList.rawAt(m_selectedPartIdx);
    if (!part)
        return;

    if (m_partitions[m_selectedPartIdx].encrypted) {
        part = decryptLUKSAndRedetect();
        if (!part)
            return;
    }

    if (m_scanTree) {
        tree_free(m_scanTree);
        m_scanTree = nullptr;
    }
    m_scanTree = tree_new();

    uint64_t partSize = part->part_size;

    ProgressDialog dlg(this);
    dlg.setWindowTitle(tr("Deep Filesystem Scan"));
    dlg.setStatusText(tr("Scanning filesystem with deep search..."));
    dlg.showFileName(true);
    dlg.showCancelButton(true);
    dlg.setIndeterminate(true);

    connect(&dlg, &ProgressDialog::cancelled, ProgressCallback::instance(), &ProgressCallback::cancel);
    QMetaObject::Connection dsConn1 = connect(m_scanner, &Scanner::progressUpdated, this, [&dlg](uint64_t deleted, uint64_t total, const QString &path) {
        dlg.setIndeterminate(true);
        dlg.setStatusText(QString("%1 deleted / %2 total").arg(deleted).arg(total));
        if (!path.isEmpty())
            dlg.setFileName(path);
    });
    QMetaObject::Connection dsConn2 = connect(m_scanner, &Scanner::indxProgressUpdated, this, [&dlg, partSize](const QString &, uint64_t current, uint64_t total, uint64_t found) {
        unsigned int pct = (total > 0) ? (unsigned int)(current * 100 / total) : 0;
        if (pct > 100) pct = 100;
        dlg.setIndeterminate(false);

        uint64_t scannedBytes = (total > 0) ? (current * partSize / total) : 0;
        double scannedGB = scannedBytes / (1024.0 * 1024.0 * 1024.0);
        double totalGB = partSize / (1024.0 * 1024.0 * 1024.0);
        dlg.updateProgress((int)pct,
            QString::asprintf("%.2f / %.2f GB  ·  %llu found", scannedGB, totalGB, (unsigned long long)found));
    });
    connect(m_scanner, &Scanner::finished, this, [&dlg, dsConn1, dsConn2](int result) {
        disconnect(dsConn1);
        disconnect(dsConn2);
        dlg.setFinished(result >= 0,
            result >= 0 ? tr("Deep scan complete.") : tr("No filesystem detected."));
    });

    m_scanner->start(m_scanTree, m_currentDisk.raw(), part, 1);
    dlg.showFileName(true);
    dlg.exec();

    m_progressCb->uninstallAllCallbacks();
    if (m_scanTree && m_scanTree->total_files > 0)
        showBrowser();
}

void MainWindow::onBackupRequested()
{
    m_selectedPartIdx = m_partPage->selectedPartitionIndex();
    if (m_selectedPartIdx < -1)
        m_selectedPartIdx = 0;
    if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
        return;

    partition_t *part = m_partList.rawAt(m_selectedPartIdx);
    if (!part)
        return;

    QString destDir = QFileDialog::getExistingDirectory(this,
        tr("Select Backup Destination"));
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

    QThread *thread = QThread::create([this, diskPtr, part, dirBytes, &dlg]() {
        int result = backup_create(diskPtr, part, dirBytes.constData());
        QMetaObject::invokeMethod(this, [&dlg, result]() {
            if (result != 0)
                dlg.setFinished(false, tr("Backup failed."));
            else
                dlg.setFinished(true, tr("Backup created successfully."));
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();

    dlg.exec();
}

void MainWindow::onRestoreRequested()
{
    m_selectedPartIdx = m_partPage->selectedPartitionIndex();
    if (m_selectedPartIdx < -1)
        m_selectedPartIdx = 0;
    if (m_selectedPartIdx < 0 || m_selectedPartIdx >= m_partitions.size())
        return;

    partition_t *part = m_partList.rawAt(m_selectedPartIdx);
    if (!part)
        return;

    QString backupPath = QFileDialog::getOpenFileName(this,
        tr("Select Backup File"), QString(),
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

    QThread *thread = QThread::create([this, tree, diskPtr, part, pathBytes, &dlg]() {
        int result = backup_restore(tree, diskPtr, part, pathBytes.constData());
        QMetaObject::invokeMethod(this, [&dlg, result]() {
            if (result != 0)
                dlg.setFinished(false, tr("Restore failed."));
            else
                dlg.setFinished(true, tr("Restore complete."));
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();

    dlg.exec();
    m_progressCb->uninstallAllCallbacks();
    if (m_scanTree)
        showBrowser();
}

void MainWindow::onBackToDisks()
{
    m_stack->setCurrentIndex(0);
    m_diskPage->refreshDisks();
}

void MainWindow::onRestoreFromBrowser()
{
    if (!m_fileModel || !m_scanTree)
        return;

    int markedCount = m_fileModel->markedCount();
    if (markedCount == 0) {
        m_browserPage->markSelected();
        markedCount = m_fileModel->markedCount();
    }
    if (markedCount == 0) {
        QMessageBox::information(this, tr("No Files Marked"),
            tr("Please mark files for recovery first (Space key)."));
        return;
    }

    QString destDir = QFileDialog::getExistingDirectory(this,
        tr("Select Restore Destination"));
    if (destDir.isEmpty())
        return;

    partition_t *part = nullptr;
    if (m_selectedPartIdx >= 0 && m_selectedPartIdx < m_partitions.size())
        part = m_partList.rawAt(m_selectedPartIdx);
    if (!part) {
        QMessageBox::warning(this, tr("Error"),
            tr("No valid partition for restore."));
        return;
    }

    ProgressDialog dlg(this);
    dlg.setWindowTitle(tr("Restoring Files"));
    dlg.setStatusText(tr("Preparing to restore..."));
    dlg.showFileName(true);
    dlg.showCancelButton(true);

    m_progressCb->reset();

    connect(&dlg, &ProgressDialog::cancelled,
            ProgressCallback::instance(), &ProgressCallback::cancel);
    QMetaObject::Connection rConn1 = connect(ProgressCallback::instance(), &ProgressCallback::restoreProgress,
            this, [&dlg](int pct, const QString &file, int total, int done) {
        dlg.updateProgress(pct,
            QStringLiteral("File %1 / %2").arg(done).arg(total));
        if (!file.isEmpty())
            dlg.setFileName(file);
    });
    QMetaObject::Connection rConn2 = connect(ProgressCallback::instance(), &ProgressCallback::fileRestored,
            this, [&dlg](const QString &path, bool ok) {
        Q_UNUSED(path); Q_UNUSED(ok);
    });
    connect(m_restorer, &Restorer::finished, this, [&dlg, rConn1, rConn2](uint64_t ok, uint64_t fail) {
        disconnect(rConn1);
        disconnect(rConn2);
        if (fail == 0)
            dlg.setFinished(true,
                QStringLiteral("All %1 files restored successfully.").arg(ok));
        else
            dlg.setFinished(false,
                QStringLiteral("%1 ok, %2 failed.").arg(ok).arg(fail));
    });

    m_restorer->start(m_scanTree, m_currentDisk.raw(), part, destDir, nullptr);

    dlg.exec();
    m_progressCb->uninstallAllCallbacks();
}

void MainWindow::onBrowserQuit()
{
    m_stack->setCurrentIndex(0);
    m_diskPage->refreshDisks();
}

void MainWindow::onAbout()
{
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::showBrowser()
{
    if (!m_scanTree)
        return;
    int n = 0;
    struct td_list_head *pos;
    if (m_scanTree->root)
        td_list_for_each(pos, &m_scanTree->root->children) { n++; }
    qDebug() << "showBrowser: total_files=" << m_scanTree->total_files 
             << "total_dirs=" << m_scanTree->total_dirs << "root_children=" << n;

    m_fileModel->setTree(m_scanTree);
    m_browserPage->setFileModel(m_fileModel);
    m_stack->setCurrentIndex(2);
}
