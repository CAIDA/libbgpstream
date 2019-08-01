/*
 * Copyright (C) 2016 The Regents of the University of California.
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
 *
 * Author: Shane Alcock <salcock@waikato.ac.nz>
 */

#include "config.h"

#include "bgpstream_utils_time.h"
#include <inttypes.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

int bgpstream_time_calc_recent_interval(uint32_t *start, uint32_t *end,
                                        const char *optval)
{
  char *p;
  uint32_t unitcount = 0;
  struct timeval tv;

  unitcount = strtoul(optval, &p, 10);

  if (p == optval || *p == '\0') {
    return 0;
  }
  while (isspace(*p)) ++p;

  switch (*p) {
  case 's':
    break;
  case 'm':
    unitcount = unitcount * 60;
    break;
  case 'h':
    unitcount = unitcount * 60 * 60;
    break;
  case 'd':
    unitcount = unitcount * 60 * 60 * 24;
    break;
  default:
    return 0;
  }
  if (*++p != '\0') {
    return 0;
  }

  if (gettimeofday(&tv, NULL)) {
    return 0;
  }

  *start = tv.tv_sec - unitcount;
  *end = tv.tv_sec;
  return 1;
}

// Convert a string to a unix timestamp in *t.  The string can be in
// "Y-m-d [H:M[:S]]" format (in the UTC timezone) or a unix timestamp.
// Returns a pointer to the first unused character of the string, or NULL if
// the string is invalid.
char *bgpstream_parse_time(const char *s, uint32_t *t)
{
  char *end;
  const char *formats[] = {
    // "%Y-%m-%d %H:%M:%S %z ", // %z is not posix
    "%Y-%m-%d %H:%M:%S ",
    // "%Y-%m-%d %H:%M %z ",    // %z is not posix
    "%Y-%m-%d %H:%M ",
    "%Y-%m-%d ",
    NULL,
  };

  while (isspace(*s)) s++;

  // Try each format
  struct tm tm;
  for (int i = 0; formats[i]; i++) {
    memset(&tm, 0, sizeof(tm));
    end = strptime(s, formats[i], &tm);
    if (end) { // success
      // Annoyingly, mktime() uses localtime, so we have to setenv TZ.
      char *oldtz = getenv("TZ");
      if (setenv("TZ", "UTC", 1) != 0) {
        return NULL;
      }
      time_t tt = mktime(&tm);
      // restore TZ
      if (oldtz) {
        setenv("TZ", oldtz, 1);
      } else {
        unsetenv("TZ");
      }
      if (tt > UINT32_MAX)
        return NULL;
      *t = (uint32_t)tt;
      return end;
    }
  }

  // Try unix timestamp
  if (!isdigit(*s))
    return NULL;
  errno = 0;
  unsigned long ul = strtoul(s, &end, 10);
  if (errno || ul > UINT32_MAX)
    return NULL;
  *t = (uint32_t)ul;
  while (isspace(*end)) end++;
  return end;
}
