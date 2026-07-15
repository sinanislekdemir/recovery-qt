/*
    
    File: scanner.cpp

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
#include "scanner.hpp"
#include "progresscallback.hpp"

Scanner::Scanner(QObject *parent)
    : WorkerBase(parent)
{
}

void Scanner::start(scan_tree_t *tree, disk_t *disk, const partition_t *partition, bool deep)
{
    ProgressCallback *pc = beginOperation();
    if (!pc) return;

    storeConnection(connect(pc, &ProgressCallback::scannerProgress,
            this, &Scanner::progressUpdated, Qt::DirectConnection));
    storeConnection(connect(pc, &ProgressCallback::scannerIndxProgress,
            this, &Scanner::indxProgressUpdated, Qt::DirectConnection));

    startThread([this, pc, tree, disk, partition, deep]() {
        pc->installScannerCallbacks();
        int result = scanner_run(tree, disk, partition, deep);
        if (result < 0)
            emit errorOccurred(tr("No filesystem detected on this partition"));
        emit finished(result);
        m_running.store(false);
    });
}
