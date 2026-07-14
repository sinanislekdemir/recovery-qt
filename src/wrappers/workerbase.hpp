#ifndef WORKER_BASE_HPP
#define WORKER_BASE_HPP

#include <QObject>
#include <QThread>
#include <QMetaObject>
#include <atomic>
#include <functional>
#include <vector>

class ProgressCallback;

class WorkerBase : public QObject {
    Q_OBJECT
public:
    explicit WorkerBase(QObject *parent = nullptr);
    ~WorkerBase() override;

    void cancel();
    bool isRunning() const;

protected:
    void startThread(std::function<void()> workFn);
    void storeConnection(QMetaObject::Connection conn);
    void disconnectAllConnections();

    std::atomic<bool> m_running;
    QThread *m_thread;

private:
    std::vector<QMetaObject::Connection> m_connections;
};

#endif
