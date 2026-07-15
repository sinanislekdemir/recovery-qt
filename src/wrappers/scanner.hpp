/*
    
    File: scanner.hpp

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
#ifndef PHOTOREC_SCANNER_HPP
#define PHOTOREC_SCANNER_HPP

#include <QObject>
#include <QString>
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

class Scanner : public WorkerBase {
    Q_OBJECT
public:
    explicit Scanner(QObject *parent = nullptr);

    void start(scan_tree_t *tree, disk_t *disk, const partition_t *partition, bool deep = true);

signals:
    void progressUpdated(uint64_t deletedCount, uint64_t totalCount, const QString &path);
    void indxProgressUpdated(const QString &msg, uint64_t current, uint64_t total, uint64_t found);
    void finished(int deletedFiles);
    void errorOccurred(const QString &message);
};

#endif
