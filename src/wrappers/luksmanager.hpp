#ifndef PHOTOREC_LUKSMANAGER_HPP
#define PHOTOREC_LUKSMANAGER_HPP

#include <QObject>
#include <QString>
#include <cstddef>
#include <cstdint>

class Disk;

#ifdef __cplusplus
extern "C" {
#endif
#include "photorec_nc.h"
#include "luksnc.h"
#include "luks.h"
#ifdef __cplusplus
}
#endif

class LUKSManager : public QObject {
    Q_OBJECT
public:
    explicit LUKSManager(QObject *parent = nullptr);

    static bool isEncrypted(disk_t *disk, partition_t *partition);
    bool decrypt(const QString &device, uint64_t offset, const QString &passphrase);
    void decryptAsync(const QString &device, uint64_t offset, const QString &passphrase);
    bool decryptDisk(Disk &disk, partition_t *partition, const QString &passphrase);
    void close();
    void cleanupOrphans();

    QString mapperPath() const;
    bool isDecrypted() const;

signals:
    void errorOccurred(const QString &message);
    void decryptFinished(bool ok);

private:
    char m_mapperName[256];
    bool m_decrypted;
};

#endif // PHOTOREC_LUKSMANAGER_HPP
