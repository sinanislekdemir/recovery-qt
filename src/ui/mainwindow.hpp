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
    void onBrowserQuit();
    void onAbout();

private:
    void setupUi();
    void applyTheme();
    void showBrowser();
    void detectPartitions();
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
    LUKSManager *m_luks;

    Disk m_currentDisk;
    QVector<PartitionInfo> m_partitions;
    PartitionList m_partList;
    int m_selectedPartIdx;
    scan_tree_t *m_scanTree;
};

#endif // MAINWINDOW_HPP
