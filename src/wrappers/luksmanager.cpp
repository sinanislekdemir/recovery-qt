#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "luksmanager.hpp"
#include "disk.hpp"
#include <cstring>
#include <QDebug>
#include <QThread>

LUKSManager::LUKSManager(QObject *parent)
    : QObject(parent)
    , m_decrypted(false)
{
    m_mapperName[0] = '\0';
}

bool LUKSManager::isEncrypted(disk_t *disk, partition_t *partition)
{
    return check_LUKS(disk, partition) != 0;
}

bool LUKSManager::decrypt(const QString &device, uint64_t offset,
                          const QString &passphrase)
{
    QByteArray devBytes = device.toLocal8Bit();
    QByteArray passBytes = passphrase.toLocal8Bit();

    qDebug() << "LUKSManager::decrypt ENTER device=" << device << "offset=" << offset;
    m_mapperName[0] = '\0';

    int result = luks_open(devBytes.constData(), offset,
        passBytes.constData(), m_mapperName, sizeof(m_mapperName));

    qDebug() << "LUKSManager::decrypt luks_open returned" << result
             << "mapper=" << m_mapperName;

    if (result != 0) {
        m_decrypted = false;
        emit errorOccurred(tr("Failed to decrypt LUKS volume: %1")
            .arg(device));
        return false;
    }

    m_decrypted = true;
    return true;
}

void LUKSManager::decryptAsync(const QString &device, uint64_t offset,
                               const QString &passphrase)
{
    QThread *thread = QThread::create([this, device, offset, passphrase]() {
        bool ok = decrypt(device, offset, passphrase);
        emit decryptFinished(ok);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

bool LUKSManager::decryptDisk(Disk &disk, partition_t *partition,
                              const QString &passphrase)
{
    if (!disk.isValid() || !partition)
        return false;

    uint64_t offset = partition->part_offset;
    return decrypt(disk.device(), offset, passphrase);
}

void LUKSManager::close()
{
    if (!m_decrypted || m_mapperName[0] == '\0')
        return;

    luks_close(m_mapperName);
    m_decrypted = false;
    m_mapperName[0] = '\0';
}

void LUKSManager::cleanupOrphans()
{
    luks_cleanup_orphans();
}

QString LUKSManager::mapperPath() const
{
    if (!m_decrypted || m_mapperName[0] == '\0')
        return QString();

    return QString::fromLocal8Bit(m_mapperName);
}

bool LUKSManager::isDecrypted() const
{
    return m_decrypted;
}
