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

private:
    const file_enable_t* m_rawArray;
    static int extensionPriority(const QString& ext);
};

#endif // PHOTOREC_SIGNATUREREGISTRY_HPP
