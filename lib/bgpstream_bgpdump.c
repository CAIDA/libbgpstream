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
#include "bgpstream_elem_int.h"
#include "bgpstream_int.h"
#include "bgpstream_log.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#define B_REMAIN (len > written ? len - written : 0) /* unsigned */
#define B_FULL (written >= len)
#define ADD_PIPE                                                               \
  do {                                                                         \
    if (len > written + 1) {                                                   \
      buf_p[0] = '|';                                                          \
      buf_p[1] = '\0';                                                         \
    }                                                                          \
    buf_p++;                                                                   \
    written++;                                                                 \
  } while (0)

#define SEEK_STR_END                                                           \
  do {                                                                         \
    while (*buf_p != '\0') {                                                   \
      written++;                                                               \
      buf_p++;                                                                 \
    }                                                                          \
  } while (0)

char *bgpstream_record_elem_bgpdump_snprintf(char *buf, size_t len,
                                             const bgpstream_record_t *record,
                                             const bgpstream_elem_t *elem)
{
  assert(record);
  assert(elem);

  size_t written = 0; /* < how many bytes we wanted to write */
  ssize_t c = 0;      /* < how many chars were written */
  char *buf_p = buf;

  /* Record type */
  switch (elem->type) {
  case BGPSTREAM_ELEM_TYPE_RIB:
    c = snprintf(buf_p, B_REMAIN, "TABLE_DUMP2|%" PRIu32, record->time_sec);
    break;
  case BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT:
  case BGPSTREAM_ELEM_TYPE_WITHDRAWAL:
  case BGPSTREAM_ELEM_TYPE_PEERSTATE:
    c = snprintf(buf_p, B_REMAIN, "BGP4MP|%" PRIu32, record->time_sec);
    break;
  default:
    c = 0;
    break;
  }
  written += c;
  buf_p += c;
  ADD_PIPE;

  switch (elem->type) {
  case BGPSTREAM_ELEM_TYPE_RIB:
    c = snprintf(buf_p, B_REMAIN, "B");
    break;
  case BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT:
    c = snprintf(buf_p, B_REMAIN, "A");
    break;
  case BGPSTREAM_ELEM_TYPE_WITHDRAWAL:
    c = snprintf(buf_p, B_REMAIN, "W");
    break;
  case BGPSTREAM_ELEM_TYPE_PEERSTATE:
    c = snprintf(buf_p, B_REMAIN, "STATE");
    break;
  default:
    c = 0;
    break;
  }

  written += c;
  buf_p += c;
  ADD_PIPE;

  /* PEER IP */
  if (bgpstream_addr_ntop(buf_p, B_REMAIN, &elem->peer_ip) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed peer address");
    return NULL;
  }
  SEEK_STR_END;
  ADD_PIPE;

  /* PEER ASN */
  c = snprintf(buf_p, B_REMAIN, "%" PRIu32, elem->peer_asn);
  written += c;
  buf_p += c;
  ADD_PIPE;

  switch (elem->type) {
  case BGPSTREAM_ELEM_TYPE_RIB:
  case BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT:
    /* PREFIX */
    if (bgpstream_pfx_snprintf(buf_p, B_REMAIN, &(elem->prefix)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed prefix");
      return NULL;
    }
    SEEK_STR_END;
    ADD_PIPE;

    /* AS PATH */
    c = bgpstream_as_path_snprintf(buf_p, B_REMAIN, elem->as_path);
    written += c;
    buf_p += c;

    ADD_PIPE;

    /* SOURCE (IGP) */
    if (elem->has_origin) {
      switch (elem->origin) {
      case BGPSTREAM_ELEM_BGP_UPDATE_ORIGIN_IGP:
        c = snprintf(buf_p, B_REMAIN, "IGP");
        break;
      case BGPSTREAM_ELEM_BGP_UPDATE_ORIGIN_EGP:
        c = snprintf(buf_p, B_REMAIN, "EGP");
        break;
      case BGPSTREAM_ELEM_BGP_UPDATE_ORIGIN_INCOMPLETE:
        c = snprintf(buf_p, B_REMAIN, "INCOMPLETE");
        break;
      default:
        c = 0;
        break;
      }
      written += c;
      buf_p += c;
    }
    ADD_PIPE;

    /* NEXT HOP */
    if (bgpstream_addr_ntop(buf_p, B_REMAIN, &elem->nexthop) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed next_hop IP address");
      return NULL;
    }
    SEEK_STR_END;
    ADD_PIPE;

    /* LOCAL_PREF */
    if (elem->has_local_pref) {
      c = snprintf(buf_p, B_REMAIN, "%" PRIu32, elem->local_pref);
      written += c;
      buf_p += c;
    } else {
      c = snprintf(buf_p, B_REMAIN, "0");
      written += c;
      buf_p += c;
    }
    ADD_PIPE;

    /* MED */
    if (elem->has_med) {
      c = snprintf(buf_p, B_REMAIN, "%" PRIu32, elem->med);
      written += c;
      buf_p += c;
    } else {
      c = snprintf(buf_p, B_REMAIN, "0");
      written += c;
      buf_p += c;
    }
    ADD_PIPE;

    /* COMMUNITIES */
    c = bgpstream_community_set_snprintf(buf_p, B_REMAIN, elem->communities);
    written += c;
    buf_p += c;

    ADD_PIPE;

    /* AGGREGATE AG/NAG */
    if (elem->atomic_aggregate == 1) {
      c = snprintf(buf_p, B_REMAIN, "AG");
    } else {
      c = snprintf(buf_p, B_REMAIN, "NAG");
    }
    written += c;
    buf_p += c;
    ADD_PIPE;

    /* AGGREGATOR AS AND IP */
    if (elem->aggregator.has_aggregator > 0) {
      c = snprintf(buf_p, B_REMAIN, "%" PRIu32 " ",
                   elem->aggregator.aggregator_asn);
      written += c;
      buf_p += c;
      if (bgpstream_addr_ntop(buf_p, B_REMAIN,
                              &elem->aggregator.aggregator_addr) == NULL) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed aggregator IP address");
        return NULL;
      }
      SEEK_STR_END;
    }

    ADD_PIPE;

    break;
  case BGPSTREAM_ELEM_TYPE_WITHDRAWAL:
    /* PREFIX */
    if (bgpstream_pfx_snprintf(buf_p, B_REMAIN, &(elem->prefix)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed prefix");
      return NULL;
    }
    SEEK_STR_END;
    break;
  case BGPSTREAM_ELEM_TYPE_PEERSTATE:
    c = snprintf(buf_p, B_REMAIN, "%u|%u", elem->old_state, elem->new_state);
    written += c;
    buf_p += c;
    break;
  default:
    break;
  }

  return B_FULL ? NULL : buf;
}
