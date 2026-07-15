/*
    
    File: signatureregistry.hpp

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
#ifndef PHOTOREC_SIGNATUREREGISTRY_HPP
#define PHOTOREC_SIGNATUREREGISTRY_HPP

#include <QString>
#include <QVector>
#include <QMap>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif
#include "filegen.h"
#ifdef __cplusplus
}
#endif

struct SignatureInfo {
    QString extension;
    QString description;
    uint64_t maxFilesize;
    bool enabledByDefault;
    bool enabled;
    int priority;
};

class SignatureRegistry {
public:
    SignatureRegistry();

    void resetDefaults();
    void setEnabled(const QString& extension, bool enable);
    void setEnabledExtensions(const QStringList& extensions);
    void disableAll();

    QVector<SignatureInfo> allSignatures() const;
    QStringList enabledExtensions() const;
    int count() const;

    file_enable_t* rawArray() const { return (file_enable_t*)m_rawArray; }

    static bool isPreviewableImage(const QString &ext);

private:
    const file_enable_t* m_rawArray;
    static int extensionPriority(const QString& ext);
};

#endif // PHOTOREC_SIGNATUREREGISTRY_HPP
