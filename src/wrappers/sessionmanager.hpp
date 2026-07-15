/*
    
    File: sessionmanager.hpp

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
#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include <QObject>
#include <QString>
#include <QByteArray>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif
#include "session.h"
#include "common.h"
#include "progress.h"
#ifdef __cplusplus
}
#endif

struct SessionInfo {
    int opType;
    int resumePhase;
    bool completed;
    bool cancelled;
    bool encrypted;
    bool luksDecrypted;
    uint64_t progress1;
    uint64_t progress2;
    uint64_t resumeOffset;
    uint64_t timestamp;
    unsigned int sectorSize;
    uint64_t diskSize;
    uint64_t partOffset;
    uint64_t partSize;
    uint32_t upartType;
    uint32_t partTypeI386;
    uint64_t luksOffset;
    QString devicePath;
    QString model;
    QString extFilter;
    scan_tree_t *tree;
};

class SessionManager : public QObject {
    Q_OBJECT
public:
    explicit SessionManager(QObject *parent = nullptr);

    bool hasActiveSession() const;

    void beginSession(const QString &path, scan_tree_t *tree,
        disk_t *disk, const partition_t *partition,
        int opType, const QString &extFilter);

    void updateSessionPhase(int resumePhase, uint64_t resumeOffset);

    void endSession(int result);
    void cancelSession();

    void setupResume(scan_tree_t *tree, disk_t *disk,
        int opType, const SessionInfo *info);

    QString sessionPath() const { return m_sessionPath; }

    static SessionInfo *loadSession(const QString &path);
    static void freeSessionInfo(SessionInfo *info);

private:
    QString m_sessionPath;
    session_save_ctx_t m_saveCtx;
    QByteArray m_pathBytes;
    QByteArray m_filterBytes;
    int m_flags;
    bool m_active;
};

#endif
