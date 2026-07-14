#ifndef FORMAT_UTILS_HPP
#define FORMAT_UTILS_HPP

#include <QString>
#include <cstdint>

inline QString formatSize(uint64_t bytes)
{
    if (bytes >= (1ULL << 40))
        return QString::asprintf("%.2f TB", bytes / static_cast<double>(1ULL << 40));
    if (bytes >= (1ULL << 30))
        return QString::asprintf("%.2f GB", bytes / static_cast<double>(1ULL << 30));
    if (bytes >= (1ULL << 20))
        return QString::asprintf("%.2f MB", bytes / static_cast<double>(1ULL << 20));
    if (bytes >= (1ULL << 10))
        return QString::asprintf("%.2f KB", bytes / static_cast<double>(1ULL << 10));
    return QString::asprintf("%llu B", static_cast<unsigned long long>(bytes));
}

#endif
