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
 *
 * Authors:
 *   Mingwei Zhang
 *   Alistair King
 */

#include "bgpstream_bgpdump.h"
#include "bgpstream_int.h"
#include "bgpstream_elem_int.h"
#include "bgpstream_log.h"
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

#define B_REMAIN (len - written)
#define B_FULL (written >= len)
#define ADD_PIPE                                                               \
  do {                                                                         \
    if (B_REMAIN > 1) {                                                        \
      *buf_p = '|';                                                            \
      buf_p++;                                                                 \
      *buf_p = '\0';                                                           \
      written++;                                                               \
    } else {                                                                   \
      return NULL;                                                             \
    }                                                                          \
  } while (0)

#define SEEK_STR_END                                                           \
  do {                                                                         \
    while (*buf_p != '\0') {                                                   \
      written++;                                                               \
      buf_p++;                                                                 \
    }                                                                          \
  } while (0)

char *bgpstream_record_elem_bgpdump_snprintf(char *buf, size_t len,
                                     bgpstream_record_t *record,
                                     bgpstream_elem_t *elem)
{
  assert(record);
  assert(elem);

  size_t written = 0; /* < how many bytes we wanted to write */
  ssize_t c = 0;      /* < how many chars were written */
  char *buf_p = buf;

  switch(record->type){
  case BGPSTREAM_UPDATE:
    break;
  case BGPSTREAM_RIB:
    break;
  default:
    break;
  }

  /* Record type */
  if ((c = bgpstream_record_type_snprintf(buf_p, B_REMAIN,
                                          record->type)) < 0) {
    return NULL;
  }
  written += c;
  buf_p += c;
  ADD_PIPE;

  /* Elem type */
  if ((c = bgpstream_elem_type_snprintf(buf_p, B_REMAIN, elem->type)) < 0) {
    return NULL;
  }
  written += c;
  buf_p += c;
  ADD_PIPE;

  /* Record timestamp, project, collector, router names */
  c = snprintf(buf_p, B_REMAIN, "%" PRIu32 ".%06" PRIu32 "|BGPDUMP!|%s|%s|%s|",
               record->time_sec, record->time_usec,
               record->project_name, record->collector_name,
               record->router_name);
  written += c;
  buf_p += c;

  if (B_FULL)
    return NULL;

  /* Router IP */
  if (record->router_ip.version != 0) {
    if (bgpstream_addr_ntop(buf_p, B_REMAIN, &record->router_ip) ==
        NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed Router IP address");
      return NULL;
    }
    SEEK_STR_END;
  }
  ADD_PIPE;

  if (bgpstream_elem_custom_snprintf(buf_p, B_REMAIN, elem, 0) == NULL) {
    return NULL;
  }

  written += c;
  buf_p += c;

  if (B_FULL)
    return NULL;

  return buf;
}
