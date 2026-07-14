#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "scanner.hpp"
#include "progresscallback.hpp"

Scanner::Scanner(QObject *parent)
    : WorkerBase(parent)
    , m_tree(nullptr)
    , m_disk(nullptr)
    , m_partition(nullptr)
{
}

void Scanner::start(scan_tree_t *tree, disk_t *disk, const partition_t *partition, int deep)
{
    if (m_running.load())
        return;

    m_tree = tree;
    m_disk = disk;
    m_partition = partition;

    ProgressCallback *pc = ProgressCallback::instance();
    pc->reset();

    storeConnection(connect(pc, &ProgressCallback::scannerProgress,
            this, &Scanner::progressUpdated, Qt::DirectConnection));
    storeConnection(connect(pc, &ProgressCallback::scannerIndxProgress,
            this, &Scanner::indxProgressUpdated, Qt::DirectConnection));

    startThread([this, pc, deep]() {
        pc->installScannerCallbacks();
        int result = scanner_run(m_tree, m_disk, m_partition, deep);

        if (result < 0) {
            emit errorOccurred(tr("No filesystem detected on this partition"));
        }
        emit finished(result);
        m_running.store(false);
    });
}
