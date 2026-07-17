/*
    
    File: ghostfinder.hpp

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
#ifndef PHOTOREC_GHOSTFINDER_HPP
#define PHOTOREC_GHOSTFINDER_HPP

#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>
#include <cstdint>
#include "wrappers/workerbase.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include "types.h"
#include "common.h"
#ifdef __cplusplus
}
#endif

class GhostFinder : public WorkerBase {
  Q_OBJECT
public:
  explicit GhostFinder(QObject *parent = nullptr);

  void start(disk_t *disk, const list_part_t *knownParts, uint64_t strideSectors);

  list_part_t *resultList() const {
    return m_resultList;
  }

  void onProgress(uint64_t scanned, uint64_t total);

signals:
  void progressUpdated(uint64_t sectorsScanned, uint64_t totalSectors);
  void ghostFound(uint64_t offset, const QString &fsType);
  void finished(int ghostCount);

private:
  list_part_t *m_resultList;
  QTimer *m_pollTimer;
  std::atomic<uint64_t> m_scanned;
  std::atomic<uint64_t> m_total;
};

#endif
