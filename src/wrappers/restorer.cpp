/*
    
    File: restorer.cpp

    Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>

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
    ProgressCallback *pc = beginOperation();
    if (!pc) return;

    m_okCount.store(0);
    m_failCount.store(0);

    QByteArray dirBytes = destDir.toLocal8Bit();

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
        int result;
        pc->installRestoreCallbacks();
        if (onlyNode)
            result = restore_file_node(tree, disk, partition,
                dirBytes.constData(), onlyNode);
        else
            result = restore_files(tree, disk, partition,
                dirBytes.constData());

        if (result < 0) {
            emit errorOccurred(tr("File restore failed"));
        }
        emit finished(m_okCount.load(), m_failCount.load());
        m_running.store(false);
    });
}
