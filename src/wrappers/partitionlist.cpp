/*
    
    File: partitionlist.cpp

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
#include "partitionlist.hpp"
#include "disk.hpp"
#include <cstring>

extern "C" {
extern const arch_fnct_t arch_none;
#include "luks.h"
#include "fat.h"
#include "ntfs.h"
#include "ext2.h"
#include "exfat.h"
#include "iso.h"
}

static void detect_fs_type(disk_t *disk, partition_t *part)
{
    if (check_EXT2(disk, part, 0) == 0) return;
    if (check_NTFS(disk, part, 0, 0) == 0) return;
    if (check_FAT(disk, part, 0) == 0) return;
    if (check_ISO(disk, part) == 0) return;
    check_exFAT(disk, part);
}

static void check_partition_luks(disk_t *disk, partition_t *part)
{
    if (!part || !disk) return;
    if (check_LUKS(disk, part) == 0) {
        strncpy(part->fsname, "LUKS encrypted", sizeof(part->fsname) - 1);
    }
}

static QString upartTypeName(upart_type_t type)
{
    switch (type) {
    case UP_UNK:       return QStringLiteral("Unknown");
    case UP_APFS:      return QStringLiteral("APFS");
    case UP_BEOS:      return QStringLiteral("BeOS");
    case UP_BTRFS:     return QStringLiteral("Btrfs");
    case UP_CRAMFS:    return QStringLiteral("CRAMFS");
    case UP_EXFAT:     return QStringLiteral("exFAT");
    case UP_EXT2:      return QStringLiteral("EXT2");
    case UP_EXT3:      return QStringLiteral("EXT3");
    case UP_EXT4:      return QStringLiteral("EXT4");
    case UP_EXTENDED:  return QStringLiteral("Extended");
    case UP_FAT12:     return QStringLiteral("FAT12");
    case UP_FAT16:     return QStringLiteral("FAT16");
    case UP_FAT32:     return QStringLiteral("FAT32");
    case UP_FATX:      return QStringLiteral("FATX");
    case UP_FREEBSD:   return QStringLiteral("FreeBSD");
    case UP_F2FS:      return QStringLiteral("F2FS");
    case UP_GFS2:      return QStringLiteral("GFS2");
    case UP_HFS:       return QStringLiteral("HFS");
    case UP_HFSP:      return QStringLiteral("HFS+");
    case UP_HFSX:      return QStringLiteral("HFSX");
    case UP_HPFS:      return QStringLiteral("HPFS");
    case UP_ISO:       return QStringLiteral("ISO 9660");
    case UP_JFS:       return QStringLiteral("JFS");
    case UP_LINSWAP:   return QStringLiteral("Linux Swap");
    case UP_LINSWAP2:  return QStringLiteral("Linux Swap v2");
    case UP_LUKS:      return QStringLiteral("LUKS");
    case UP_LVM:       return QStringLiteral("LVM");
    case UP_LVM2:      return QStringLiteral("LVM2");
    case UP_MD:        return QStringLiteral("MD RAID");
    case UP_MD1:       return QStringLiteral("MD RAID 1");
    case UP_NETWARE:   return QStringLiteral("NetWare");
    case UP_NTFS:      return QStringLiteral("NTFS");
    case UP_OPENBSD:   return QStringLiteral("OpenBSD");
    case UP_OS2MB:     return QStringLiteral("OS/2 Boot");
    case UP_ReFS:      return QStringLiteral("ReFS");
    case UP_RFS:       return QStringLiteral("ReiserFS");
    case UP_RFS2:      return QStringLiteral("ReiserFS v2");
    case UP_RFS3:      return QStringLiteral("ReiserFS v3");
    case UP_RFS4:      return QStringLiteral("Reiser4");
    case UP_SUN:       return QStringLiteral("Sun");
    case UP_SYSV4:     return QStringLiteral("SysV");
    case UP_UFS:       return QStringLiteral("UFS");
    case UP_UFS2:      return QStringLiteral("UFS2");
    case UP_UFS_LE:    return QStringLiteral("UFS LE");
    case UP_UFS2_LE:   return QStringLiteral("UFS2 LE");
    case UP_VMFS:      return QStringLiteral("VMFS");
    case UP_WBFS:      return QStringLiteral("WBFS");
    case UP_XFS:       return QStringLiteral("XFS");
    case UP_XFS2:      return QStringLiteral("XFS2");
    case UP_XFS3:      return QStringLiteral("XFS3");
    case UP_XFS4:      return QStringLiteral("XFS4");
    case UP_XFS5:      return QStringLiteral("XFS5");
    case UP_ZFS:       return QStringLiteral("ZFS");
    default:           return QStringLiteral("Unknown");
    }
}

PartitionList::PartitionList() : m_partList(nullptr), m_diskList(nullptr), m_count(0) {}

PartitionList::~PartitionList()
{
    if (m_diskList) {
        part_free_list(m_partList);
        free(m_diskList);
    }
}

bool PartitionList::detect(const Disk& disk)
{
    if (!disk.isValid())
        return false;

    if (m_diskList) {
        part_free_list(m_partList);
        free(m_diskList);
        m_partList = nullptr;
        m_diskList = nullptr;
    }

    m_diskList = (list_disk_t*)MALLOC(sizeof(list_disk_t));
    m_diskList->disk = disk.raw();
    m_diskList->prev = nullptr;
    m_diskList->next = nullptr;

    autodetect_arch(disk.raw(), nullptr);

    if (!disk.raw()->arch)
        return false;

    m_partList = disk.raw()->arch->read_part(disk.raw(), 0, 0);
    if (!m_partList)
        return false;

    int count = 0;
    list_part_t* item = m_partList;
    while (item) {
        if (item->part)
            check_partition_luks(disk.raw(), item->part);
        count++;
        item = item->next;
    }
    m_count = count;
    return true;
}

bool PartitionList::detectWholeDisk(const Disk& disk)
{
    if (!disk.isValid())
        return false;

    if (m_diskList) {
        part_free_list(m_partList);
        free(m_diskList);
        m_partList = nullptr;
        m_diskList = nullptr;
    }

    m_diskList = (list_disk_t*)MALLOC(sizeof(list_disk_t));
    m_diskList->disk = disk.raw();
    m_diskList->prev = nullptr;
    m_diskList->next = nullptr;

    partition_t* p = partition_new(&arch_none);
    if (!p)
        return false;
    p->part_offset = 0;
    p->part_size = disk.raw()->disk_size;
    strncpy(p->fsname, "Whole disk", sizeof(p->fsname) - 1);
    detect_fs_type(disk.raw(), p);
    check_partition_luks(disk.raw(), p);

    m_partList = (list_part_t*)MALLOC(sizeof(list_part_t));
    m_partList->part = p;
    m_partList->prev = nullptr;
    m_partList->next = nullptr;
    m_count = 1;
    return true;
}

QVector<PartitionInfo> PartitionList::partitions() const
{
    QVector<PartitionInfo> result;
    if (!m_partList)
        return result;

    list_part_t* item = m_partList;
    while (item) {
        partition_t* part = item->part;
        if (part) {
            PartitionInfo info;
            info.fsname = QString::fromLocal8Bit(part->fsname);
            info.partname = QString::fromLocal8Bit(part->partname);
            info.info = QString::fromLocal8Bit(part->info);
            info.partOffset = part->part_offset;
            info.partSize = part->part_size;
            info.partTypeI386 = part->part_type_i386;
            info.upartType = part->upart_type;
            info.status = part->status;
            info.order = part->order;
            info.encrypted = (strncmp(part->fsname, "LUKS", 4) == 0);

            {
                const char *tname = part->arch ? part->arch->get_partition_typename(part) : NULL;
                info.typenameStr = tname ? QString::fromLocal8Bit(tname) : upartTypeName(part->upart_type);
            }
            result.append(info);
        }
        item = item->next;
    }
    return result;
}

partition_t* PartitionList::rawAt(int index) const
{
    if (!m_partList)
        return nullptr;

    list_part_t* item = m_partList;
    int i = 0;
    while (item) {
        if (i == index)
            return item->part;
        item = item->next;
        i++;
    }
    return nullptr;
}

partition_t* PartitionList::wholeDiskPartition(const Disk& disk) const
{
    partition_t* p = partition_new(&arch_none);
    if (!p)
        return nullptr;
    p->part_offset = 0;
    p->part_size = disk.raw()->disk_size;
    strncpy(p->fsname, "Whole disk", sizeof(p->fsname) - 1);
    detect_fs_type(disk.raw(), p);
    return p;
}

int PartitionList::count() const { return m_count; }
bool PartitionList::isValid() const { return m_partList != nullptr; }
