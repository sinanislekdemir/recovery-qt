#ifndef SIMPLE_WORKER_HPP
#define SIMPLE_WORKER_HPP

#include "workerbase.hpp"
#include <functional>

class SimpleWorker : public WorkerBase {
    Q_OBJECT
public:
    explicit SimpleWorker(QObject *parent = nullptr);

    void start(std::function<int()> workFn);

signals:
    void finished(int result);
    void errorOccurred(const QString &message);
};

#endif
