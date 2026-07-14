/*
    
    File: luksmanager.hpp

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
