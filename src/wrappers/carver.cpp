/*
    
    File: carver.cpp

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
