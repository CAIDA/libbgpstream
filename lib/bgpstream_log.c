/*
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
