#include "workerbase.hpp"
#include "progresscallback.hpp"

WorkerBase::WorkerBase(QObject *parent)
    : QObject(parent)
    , m_running(false)
    , m_thread(nullptr)
{
}

WorkerBase::~WorkerBase()
{
    if (m_thread) {
        if (m_thread->isRunning()) {
            cancel();
            m_thread->quit();
            m_thread->wait(3000);
        }
        delete m_thread;
        m_thread = nullptr;
    }
}

void WorkerBase::cancel()
{
    ProgressCallback::instance()->cancel();
}

bool WorkerBase::isRunning() const
{
    return m_running.load();
}

void WorkerBase::startThread(std::function<void()> workFn)
{
    if (m_running.load())
        return;

    m_running.store(true);

    m_thread = QThread::create([this, workFn = std::move(workFn)]() {
        workFn();
    });

    connect(m_thread, &QThread::finished, this, [this]() {
        disconnectAllConnections();
    });

    m_thread->start();
}

void WorkerBase::storeConnection(QMetaObject::Connection conn)
{
    m_connections.push_back(conn);
}

void WorkerBase::disconnectAllConnections()
{
    for (auto &conn : m_connections)
        disconnect(conn);
    m_connections.clear();
}
