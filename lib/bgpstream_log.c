/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h> // getpid()
#include "bgpstream_log.h"

void bgpstream_log_func(int level, const char *file, int line, const char *fmt, ...)
{
  va_list va_ap;

  // TODO: consider make this configurable via the public API
  FILE *bgpstream_log_file = stderr;

  if (level <= BGPSTREAM_LOG_LEVEL) {
    char msgbuf[4096];
    va_start(va_ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf)-1, fmt, va_ap);
    msgbuf[sizeof(msgbuf)-1] = '\0';
    va_end(va_ap);

    char datebuf[32];
    time_t t = time(NULL);
    strftime(datebuf, sizeof(datebuf), "%Y-%m-%d %H:%M:%S", localtime(&t));

    const char *prefix =
      (level <= BGPSTREAM_LOG_ERR)    ? "ERROR: "    :
      (level <= BGPSTREAM_LOG_WARN)   ? "WARNING: "  :
      (level <= BGPSTREAM_LOG_INFO)   ? "INFO: "     :
      (level <= BGPSTREAM_LOG_CONFIG) ? "CONFIG: "   :
      (level <= BGPSTREAM_LOG_FINE)   ? "FINE: "     :
      (level <= BGPSTREAM_LOG_VFINE)  ? "VERYFINE: " :
      (level <= BGPSTREAM_LOG_FINEST) ? "FINEST: "   :
      "";

    fprintf(bgpstream_log_file, "%s %u: %s:%d: %s%s\n",
            datebuf, getpid(), file, line, prefix, msgbuf);

    fflush(bgpstream_log_file);
  }
}
