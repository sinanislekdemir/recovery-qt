/*
    
    File: partitionlist.hpp

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
#ifndef PHOTOREC_PARTITIONLIST_HPP
#define PHOTOREC_PARTITIONLIST_HPP

#include <QString>
#include <QVector>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
#include "types.h"
#include "common.h"
#include "partauto.h"
#include "fnctdsk.h"
#ifdef __cplusplus
}
#endif

struct PartitionInfo {
    QString fsname;
    QString partname;
    QString info;
    QString typenameStr;
    uint64_t partOffset;
    uint64_t partSize;
    unsigned int partTypeI386;
    upart_type_t upartType;
    status_type_t status;
    int order;
    bool encrypted;
};

class PartitionList {
public:
    PartitionList();
    ~PartitionList();

    bool detect(const class Disk& disk);
    bool detectWholeDisk(const class Disk& disk);

    QVector<PartitionInfo> partitions() const;
    partition_t* rawAt(int index) const;
    partition_t* wholeDiskPartition(const class Disk& disk) const;
    int count() const;

    bool isValid() const;

private:
    list_part_t* m_partList;
    list_disk_t* m_diskList;
    int m_count;
};

#endif // PHOTOREC_PARTITIONLIST_HPP
