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
