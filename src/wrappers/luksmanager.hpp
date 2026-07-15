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
#include "disk.hpp"

#ifdef __cplusplus
extern "C" {
#endif
#include "photorec_nc.h"
#include "luksdec.h"
#include "luks.h"
#ifdef __cplusplus
}
#endif

/*
 * LUKSManager performs native (in-process) LUKS1/LUKS2 decryption.
 *
 * There is no cryptsetup, losetup, device-mapper or child process involved:
 * decryptAsync() derives the master key from the passphrase and builds an
 * in-memory read-only decrypting disk_t wrapper over the base disk. The
 * result is retrieved as a Disk via decryptedDisk().
 */
class LUKSManager : public QObject {
    Q_OBJECT
public:
    explicit LUKSManager(QObject *parent = nullptr);

    static bool isEncrypted(disk_t *disk, partition_t *partition);

    /* Decrypts LUKS at part_offset on base, off the UI thread.
     * base must outlive the returned decrypted Disk. */
    void decryptAsync(disk_t *base, uint64_t offset, const QString &passphrase);

    /* Transfers ownership of the last successfully decrypted disk into a Disk.
     * Returns an invalid Disk if decryption failed or already consumed. */
    Disk decryptedDisk();

    bool isDecrypted() const;

signals:
    void errorOccurred(const QString &message);
    void decryptFinished(bool ok);

private:
    disk_t *m_decryptedDisk;
    bool m_decrypted;
};

#endif // PHOTOREC_LUKSMANAGER_HPP
