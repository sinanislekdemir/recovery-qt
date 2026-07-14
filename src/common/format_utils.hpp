/*
    
    File: format_utils.hpp

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
