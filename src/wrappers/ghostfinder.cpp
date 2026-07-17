/*
    
    File: ghostfinder.cpp

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
#include "ghostfinder.hpp"

extern "C" {
#include "ghostscan.h"
#include "fnctdsk.h"
}

static int ghostscanProgressCb(uint64_t current, uint64_t total, void *user_data) {
  GhostFinder *finder = static_cast<GhostFinder *>(user_data);
  if (!finder)
    return 1;
  finder->onProgress(current, total);
  return finder->isRunning() ? 0 : 1;
}

GhostFinder::GhostFinder(QObject *parent)
    : WorkerBase(parent), m_resultList(nullptr), m_pollTimer(new QTimer(this)), m_scanned(0), m_total(0) {
  m_pollTimer->setInterval(100);
  connect(m_pollTimer, &QTimer::timeout, this, [this]() {
    uint64_t total = m_total.load();
    if (total == 0)
      return;
    uint64_t scanned = m_scanned.load();
    emit progressUpdated(scanned, total);
  });
  connect(this, &GhostFinder::finished, m_pollTimer, &QTimer::stop);
}

void GhostFinder::onProgress(uint64_t scanned, uint64_t total) {
  m_scanned.store(scanned);
  m_total.store(total);
}

void GhostFinder::start(disk_t *disk, const list_part_t *knownParts, uint64_t strideSectors) {
  if (!disk || m_running.load())
    return;

  if (m_resultList) {
    part_free_list(m_resultList);
    m_resultList = nullptr;
  }
  m_scanned.store(0);
  m_total.store(0);

  m_pollTimer->start();

  startThread([this, disk, knownParts, strideSectors]() {
    ghostscan_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.stride_sectors = strideSectors;
    cfg.offset_start = 0;
    cfg.offset_end = disk->disk_size;
    cfg.skip_list = knownParts;
    cfg.progress_cb = ghostscanProgressCb;
    cfg.user_data = this;

    m_resultList = scan_for_ghost_partitions(disk, &cfg);

    int count = 0;
    for (list_part_t *p = m_resultList; p != NULL; p = p->next)
      count++;

    emit finished(m_resultList ? count : -1);
    m_running.store(false);
  });
}
