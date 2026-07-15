/*
    
    File: luksmanager.cpp

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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "luksmanager.hpp"
#include <cstring>
#include <QDebug>
#include <QThread>

LUKSManager::LUKSManager(QObject *parent)
    : QObject(parent)
    , m_decryptedDisk(nullptr)
    , m_decrypted(false)
{
}

bool LUKSManager::isEncrypted(disk_t *disk, partition_t *partition)
{
    return check_LUKS(disk, partition) != 0;
}

void LUKSManager::decryptAsync(disk_t *base, uint64_t offset,
                               const QString &passphrase)
{
    QByteArray passBytes = passphrase.toLocal8Bit();
    QThread *thread = QThread::create([this, base, offset, passBytes]() {
        m_decrypted = false;
        if (m_decryptedDisk) {
            if (m_decryptedDisk->clean)
                m_decryptedDisk->clean(m_decryptedDisk);
            m_decryptedDisk = nullptr;
        }

        disk_t *dec = luksdec_open(base, offset, passBytes.constData());
        if (dec == nullptr) {
            emit errorOccurred(tr("Failed to decrypt LUKS volume "
                "(wrong passphrase or unsupported format)."));
            emit decryptFinished(false);
            return;
        }
        m_decryptedDisk = dec;
        m_decrypted = true;
        emit decryptFinished(true);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

Disk LUKSManager::decryptedDisk()
{
    if (!m_decrypted || m_decryptedDisk == nullptr)
        return Disk();

    disk_t *raw = m_decryptedDisk;
    m_decryptedDisk = nullptr;
    m_decrypted = false;
    return Disk::adopt(raw);
}

bool LUKSManager::isDecrypted() const
{
    return m_decrypted;
}
