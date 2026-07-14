/*
    
    File: workerbase.cpp

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
