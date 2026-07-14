#include "signatureregistry.hpp"
#include <algorithm>

extern "C" {
extern file_enable_t array_file_enable[];
}

static const char* wellKnownFormats[] = {
    "jpg", "mov", "png", "mp4", "gif", "pdf", "zip", "doc",
    "docx", "xls", "xlsx", "ppt", "pptx", "mp3", "wav", "avi",
    "bmp", "tiff", "psd", "mxf", "wmv", "ogg", "flac", "mkv",
    "html", "svg", "dwg", "rar", "7z", "tar", "gz", "sqlite",
    "exe", "dll", nullptr
};

SignatureRegistry::SignatureRegistry() : m_rawArray(array_file_enable) {}

void SignatureRegistry::resetDefaults()
{
    file_enable_t* fe;
    for (fe = (file_enable_t*)m_rawArray; fe->file_hint != nullptr; fe++)
        fe->enable = fe->file_hint->enable_by_default;
}

void SignatureRegistry::setEnabled(const QString& extension, bool enable)
{
    file_enable_t* fe;
    QByteArray ext = extension.toLower().toLatin1();
    for (fe = (file_enable_t*)m_rawArray; fe->file_hint != nullptr; fe++) {
        if (fe->file_hint->extension &&
            strcmp(fe->file_hint->extension, ext.constData()) == 0) {
            fe->enable = enable ? 1 : 0;
            return;
        }
    }
}

void SignatureRegistry::setEnabledExtensions(const QStringList& extensions)
{
    disableAll();
    for (const QString& ext : extensions)
        setEnabled(ext, true);
}

void SignatureRegistry::disableAll()
{
    file_enable_t* fe;
    for (fe = (file_enable_t*)m_rawArray; fe->file_hint != nullptr; fe++)
        fe->enable = 0;
}

QVector<SignatureInfo> SignatureRegistry::allSignatures() const
{
    QVector<SignatureInfo> result;
    const file_enable_t* fe;
    for (fe = m_rawArray; fe->file_hint != nullptr; fe++) {
        SignatureInfo info;
        const char* ext = fe->file_hint->extension;
        info.extension = ext ? QString::fromLatin1(ext) : QString("bin");
        info.description = QString::fromLatin1(fe->file_hint->description);
        info.maxFilesize = fe->file_hint->max_filesize;
        info.enabledByDefault = fe->file_hint->enable_by_default != 0;
        info.enabled = fe->enable != 0;
        info.priority = extensionPriority(info.extension);
        result.append(info);
    }
    return result;
}

QStringList SignatureRegistry::enabledExtensions() const
{
    QStringList result;
    const file_enable_t* fe;
    for (fe = m_rawArray; fe->file_hint != nullptr; fe++) {
        if (fe->enable && fe->file_hint->extension)
            result.append(QString::fromLatin1(fe->file_hint->extension));
    }
    return result;
}

int SignatureRegistry::count() const
{
    int n = 0;
    const file_enable_t* fe;
    for (fe = m_rawArray; fe->file_hint != nullptr; fe++)
        n++;
    return n;
}

int SignatureRegistry::extensionPriority(const QString& ext)
{
    QByteArray e = ext.toLower().toLatin1();
    for (int i = 0; i < 100; i++) {
        if (wellKnownFormats[i] == nullptr)
            return 100;
        if (e == wellKnownFormats[i])
            return i;
    }
    return 100;
}
