#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "simpleworker.hpp"

SimpleWorker::SimpleWorker(QObject *parent)
    : WorkerBase(parent)
{
}

void SimpleWorker::start(std::function<int()> workFn)
{
    ProgressCallback *pc = beginOperation();
    if (!pc) return;

    startThread([this, workFn = std::move(workFn)]() {
        int result = workFn();
        emit finished(result);
        m_running.store(false);
    });
}
