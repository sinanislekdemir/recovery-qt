#include "disk.hpp"
#include <QDebug>
#include <cstring>

Disk::Disk() : d(std::make_shared<DiskData>()) {}

QVector<Disk> Disk::enumerateSystem()
{
    QVector<Disk> disks;
    list_disk_t* list = hd_parse(nullptr, 0, 0);
    if (!list)
        return disks;

    hd_update_all_geometry(list, 0);

    list_disk_t* current = list;
    while (current) {
        if (current->disk) {
            Disk disk;
            disk.d->disk = current->disk;
            disk.d->listItem = current->prev;
            disk.d->cached = false;
            disks.append(disk);
        }
        current = current->next;
    }
    return disks;
}

Disk Disk::openDevice(const QString& path, int mode)
{
    QByteArray pathBytes = path.toLocal8Bit();
    Disk disk;
    disk.d->disk = file_test_availability(
        const_cast<char*>(pathBytes.constData()), mode, 0);
    if (!disk.d->disk)
        return Disk();
    return disk;
}

Disk Disk::openDecrypted(const QString& mapperPath)
{
    return openDevice(mapperPath, TESTDISK_O_RDONLY);
}

bool Disk::isValid() const { return d->disk != nullptr; }

QString Disk::device() const
{
    if (!d->disk || !d->disk->device)
        return QString();
    return QString::fromLocal8Bit(d->disk->device);
}

QString Disk::model() const
{
    if (!d->disk || !d->disk->model)
        return QString();
    return QString::fromLocal8Bit(d->disk->model);
}

QString Disk::serialNumber() const
{
    if (!d->disk || !d->disk->serial_no)
        return QString();
    return QString::fromLocal8Bit(d->disk->serial_no);
}

uint64_t Disk::totalSize() const
{
    return d->disk ? d->disk->disk_size : 0;
}

unsigned int Disk::sectorSize() const
{
    return d->disk ? d->disk->sector_size : 512;
}

QString Disk::description() const
{
    if (!d->disk)
        return QString();
    return QString::fromLocal8Bit(d->disk->description_short(d->disk));
}

int Disk::accessMode() const
{
    return d->disk ? d->disk->access_mode : TESTDISK_O_RDONLY;
}

int Disk::read(void *buf, unsigned int count, uint64_t offset) const
{
    if (!d->disk)
        return -1;
    return d->disk->pread(d->disk, buf, count, offset);
}

int Disk::write(const void *buf, unsigned int count, uint64_t offset) const
{
    if (!d->disk)
        return -1;
    return d->disk->pwrite(d->disk, buf, count, offset);
}

int Disk::sync()
{
    if (!d->disk)
        return -1;
    return d->disk->sync(d->disk);
}

disk_t* Disk::raw() const { return d->disk; }
