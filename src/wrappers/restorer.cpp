#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "restorer.hpp"
#include "progresscallback.hpp"

Restorer::Restorer(QObject *parent)
    : WorkerBase(parent)
    , m_okCount(0)
    , m_failCount(0)
{
}

void Restorer::start(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
                     const QString &destDir, file_node_t *onlyNode)
{
    if (m_running.load())
        return;

    m_okCount.store(0);
    m_failCount.store(0);

    QByteArray dirBytes = destDir.toLocal8Bit();

    ProgressCallback *pc = ProgressCallback::instance();
    pc->reset();

    storeConnection(connect(pc, &ProgressCallback::restoreProgress,
            this, &Restorer::progressUpdated, Qt::DirectConnection));
    storeConnection(connect(pc, &ProgressCallback::fileRestored, this,
            [this](const QString &path, bool ok) {
                if (ok)
                    m_okCount.fetch_add(1);
                else
                    m_failCount.fetch_add(1);
                emit fileRestored(path, ok);
            }, Qt::DirectConnection));

    startThread([this, pc, tree, disk, partition, dirBytes, onlyNode]() {
        pc->installRestoreCallbacks();
        int result = restore_files(tree, disk, partition,
            dirBytes.constData(), onlyNode);

        if (result < 0) {
            emit errorOccurred(tr("File restore failed"));
        }
        emit finished(m_okCount.load(), m_failCount.load());
        m_running.store(false);
    });
}
