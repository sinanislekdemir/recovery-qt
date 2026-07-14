#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "carver.hpp"
#include "progresscallback.hpp"

Carver::Carver(QObject *parent)
    : WorkerBase(parent)
{
}

void Carver::start(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
                   const QString &extFilter, bool deepScan)
{
    if (m_running.load())
        return;

    QByteArray extBytes = extFilter.toLocal8Bit();

    ProgressCallback *pc = ProgressCallback::instance();
    pc->reset();

    storeConnection(connect(pc, &ProgressCallback::carverProgress,
            this, &Carver::progressUpdated, Qt::DirectConnection));

    startThread([this, pc, tree, disk, partition, extBytes, deepScan]() {
        pc->installCarverCallbacks();
        int result = carver_run(tree, disk, partition,
            extBytes.constData(), deepScan ? 1 : 0);

        if (result < 0) {
            emit errorOccurred(tr("Carving failed"));
        }
        emit finished(result);
        m_running.store(false);
    });
}
