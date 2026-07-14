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
    int count() const;

    bool isValid() const;

private:
    list_part_t* m_partList;
    list_disk_t* m_diskList;
    int m_count;
};

#endif // PHOTOREC_PARTITIONLIST_HPP
