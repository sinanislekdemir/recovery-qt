/*
    
    File: restorer.hpp

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
#ifndef PHOTOREC_RESTORER_HPP
#define PHOTOREC_RESTORER_HPP

#include <QObject>
#include <QString>
#include <atomic>
#include <cstdint>
#include "wrappers/workerbase.hpp"

class ProgressCallback;

#ifdef __cplusplus
extern "C" {
#endif
#include "recovery.h"
#include "types.h"
#ifdef __cplusplus
}
#endif

class Restorer : public WorkerBase {
    Q_OBJECT
public:
    explicit Restorer(QObject *parent = nullptr);

    void start(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
               const QString &destDir, file_node_t *onlyNode = nullptr);

signals:
    void progressUpdated(int pct, const QString &currentFile, int total, int done);
    void fileRestored(const QString &path, bool ok);
    void finished(uint64_t okCount, uint64_t failCount);
    void errorOccurred(const QString &message);

private:
    std::atomic<uint64_t> m_okCount;
    std::atomic<uint64_t> m_failCount;
};

#endif
