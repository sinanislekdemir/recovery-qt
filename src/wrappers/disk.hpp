/*
    
    File: disk.hpp

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
#ifndef PHOTOREC_DISK_HPP
#define PHOTOREC_DISK_HPP

#include <QString>
#include <QVector>
#include <memory>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
#include "types.h"
#include "common.h"
#include "hdaccess.h"
#include "hdcache.h"
#ifdef __cplusplus
}
#endif

struct DiskData {
    disk_t *disk;
    list_disk_t *listItem;
    bool cached;
};

class Disk {
public:
    Disk();
    ~Disk() = default;

    Disk(const Disk&) = default;
    Disk& operator=(const Disk&) = default;
    Disk(Disk&&) noexcept = default;
    Disk& operator=(Disk&&) noexcept = default;

    static QVector<Disk> enumerateSystem();
    static Disk openDevice(const QString& path, int mode);
    static Disk openDecrypted(const QString& mapperPath);

    bool isValid() const;
    QString device() const;
    QString model() const;
    QString serialNumber() const;
    uint64_t totalSize() const;
    unsigned int sectorSize() const;
    QString description() const;
    int accessMode() const;

    int read(void *buf, unsigned int count, uint64_t offset) const;
    int write(const void *buf, unsigned int count, uint64_t offset) const;
    int sync();

    disk_t* raw() const;

private:
    std::shared_ptr<DiskData> d;
};

#endif // PHOTOREC_DISK_HPP
