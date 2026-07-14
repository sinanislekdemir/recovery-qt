/*
    
    File: workerbase.hpp

    Copyright (C) 2025 Sinan Islekdemir <sinan@islekdemir.com>

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
