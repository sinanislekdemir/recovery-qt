/*
    
    File: sessionmanager.cpp

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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "sessionmanager.hpp"
#include <QCoreApplication>
#include <cstring>
#include <ctime>

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
    , m_flags(0)
    , m_active(false)
{
    memset(&m_saveCtx, 0, sizeof(m_saveCtx));
}

bool SessionManager::hasActiveSession() const
{
    return m_active;
}

void SessionManager::beginSession(const QString &path, scan_tree_t *tree,
    disk_t *disk, const partition_t *partition,
    int opType, const QString &extFilter)
{
    m_sessionPath = path;
    m_pathBytes = path.toLocal8Bit();
    m_filterBytes = extFilter.toLocal8Bit();
    m_flags = 0;

    /* Set up partition flags */
    if (partition && partition->upart_type == UP_LUKS)
        m_flags |= SESSION_FLAG_ENCRYPTED;

    memset(&m_saveCtx, 0, sizeof(m_saveCtx));
    m_saveCtx.filepath = m_pathBytes.constData();
    m_saveCtx.tree = tree;
    m_saveCtx.disk = disk;
    m_saveCtx.partition = partition;
    m_saveCtx.op_type = opType;
    m_saveCtx.ext_filter = m_filterBytes.constData();
    m_saveCtx.resume_phase = 0;
    m_saveCtx.resume_offset = 0;
    m_saveCtx.last_save_time = time(NULL);

    g_active_save_ctx = &m_saveCtx;
    g_session_save_cb = session_save_checkpoint;
    m_active = true;
}

void SessionManager::updateSessionPhase(int resumePhase, uint64_t resumeOffset)
{
    if (!m_active)
        return;
    m_saveCtx.resume_phase = resumePhase;
    m_saveCtx.resume_offset = resumeOffset;
}

void SessionManager::endSession(int result)
{
    if (!m_active)
        return;

    /* Final save with completed flag */
    session_save(
        m_pathBytes.constData(),
        m_saveCtx.tree,
        m_saveCtx.disk,
        m_saveCtx.partition,
        m_saveCtx.op_type,
        m_filterBytes.constData(),
        m_saveCtx.resume_offset,
        0,
        m_saveCtx.resume_phase,
        m_saveCtx.resume_offset,
        SESSION_FLAG_COMPLETED);

    g_active_save_ctx = NULL;
    g_session_save_cb = NULL;
    m_active = false;
    (void)result;
}

void SessionManager::cancelSession()
{
    if (!m_active)
        return;

    g_active_save_ctx = NULL;
    g_session_save_cb = NULL;
    m_active = false;
}

void SessionManager::setupResume(scan_tree_t *tree, disk_t *disk,
    int opType, const SessionInfo *info)
{
    if (!info || !tree)
        return;

    if (opType == SESSION_OP_CARVE)
    {
        extern uint64_t g_carver_resume_offset;
        g_carver_resume_offset = info->progress1;
    }
    else if (opType == SESSION_OP_DEEP_SCAN)
    {
        extern int g_scanner_resume_phase;
        extern uint64_t g_scanner_resume_offset;
        g_scanner_resume_phase = info->resumePhase;
        g_scanner_resume_offset = info->resumeOffset;
    }
    (void)disk;
}

SessionInfo *SessionManager::loadSession(const QString &path)
{
    session_info_t *ci;
    SessionInfo *info;

    ci = session_load(path.toLocal8Bit().constData());
    if (!ci)
        return nullptr;

    info = new SessionInfo();
    info->opType = (int)ci->op_type;
    info->resumePhase = ci->resume_phase;
    info->completed = (ci->flags & SESSION_FLAG_COMPLETED) != 0;
    info->cancelled = (ci->flags & SESSION_FLAG_CANCELLED) != 0;
    info->encrypted = (ci->flags & SESSION_FLAG_ENCRYPTED) != 0;
    info->luksDecrypted = (ci->flags & SESSION_FLAG_LUKS_DECRYPTED) != 0;
    info->progress1 = ci->progress1;
    info->progress2 = ci->progress2;
    info->resumeOffset = ci->resume_offset;
    info->timestamp = ci->timestamp;
    info->sectorSize = ci->sector_size;
    info->diskSize = ci->disk_size;
    info->partOffset = ci->part_offset;
    info->partSize = ci->part_size;
    info->upartType = ci->upart_type;
    info->partTypeI386 = ci->part_type_i386;
    info->luksOffset = ci->luks_offset;
    info->devicePath = ci->device_path ? QString::fromUtf8(ci->device_path) : QString();
    info->model = ci->model ? QString::fromUtf8(ci->model) : QString();
    info->extFilter = ci->ext_filter ? QString::fromUtf8(ci->ext_filter) : QString();
    info->tree = ci->tree;
    ci->tree = nullptr;

    session_free(ci);
    return info;
}

void SessionManager::freeSessionInfo(SessionInfo *info)
{
    if (!info)
        return;
    if (info->tree)
        tree_free(info->tree);
    delete info;
}
