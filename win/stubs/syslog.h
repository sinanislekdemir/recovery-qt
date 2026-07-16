/*

    File: syslog.h (MinGW-w64 stub for building libntfs-3g)

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
#ifndef STUB_SYSLOG_H
#define STUB_SYSLOG_H

#define LOG_EMERG 0
#define LOG_ALERT 1
#define LOG_CRIT 2
#define LOG_ERR 3
#define LOG_WARNING 4
#define LOG_NOTICE 5
#define LOG_INFO 6
#define LOG_DEBUG 7
#define LOG_DAEMON 0
#define LOG_USER 0
#define LOG_PID 0

static __inline__ void openlog(const char *i, int o, int f) {
  (void)i;
  (void)o;
  (void)f;
}

static __inline__ void closelog(void) {}

static __inline__ void syslog(int p, const char *fmt, ...) {
  (void)p;
  (void)fmt;
}

static __inline__ void vsyslog(int p, const char *fmt, __builtin_va_list ap) {
  (void)p;
  (void)fmt;
  (void)ap;
}

#endif
