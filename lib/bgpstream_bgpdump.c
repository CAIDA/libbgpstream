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

#include "bgpstream_parsebgp_common.h"
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

  /* Record type */
  switch (elem->type) {
  case BGPSTREAM_ELEM_TYPE_RIB:
    c = snprintf(buf_p, B_REMAIN, "TABLE_DUMP2|%u", record->time_sec);
    break;

  case BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT:
  case BGPSTREAM_ELEM_TYPE_WITHDRAWAL:
  case BGPSTREAM_ELEM_TYPE_PEERSTATE:
    c = snprintf(buf_p, B_REMAIN, "BGP4MP|%u", record->time_sec);
    break;
  default:
    break;
  }
  written += c;
  buf_p += c;
  ADD_PIPE;

  switch(elem->type){
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

  switch(elem->type){
  case BGPSTREAM_ELEM_TYPE_RIB:
  case BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT:
    /* PREFIX */
    if (bgpstream_pfx_snprintf(buf_p, B_REMAIN,
                               (bgpstream_pfx_t *)&(elem->prefix)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed prefix (R/A)");
      return NULL;
    }
    SEEK_STR_END;
    ADD_PIPE;

    /* AS PATH */
    c = bgpstream_as_path_snprintf(buf_p, B_REMAIN, elem->as_path);
    written += c;
    buf_p += c;
    if (B_FULL)
      return NULL;
    ADD_PIPE;

    /* SOURCE (IGP) */
    switch(elem->origin){
    case PARSEBGP_BGP_UPDATE_ORIGIN_IGP:
      c = snprintf(buf_p, B_REMAIN, "IGP");
      break;
    case PARSEBGP_BGP_UPDATE_ORIGIN_EGP:
      c = snprintf(buf_p, B_REMAIN, "EGP");
      break;
    case PARSEBGP_BGP_UPDATE_ORIGIN_INCOMPLETE:
      c = snprintf(buf_p, B_REMAIN, "INCOMPLETE");
      break;
    default:
      break;
    }
    written += c;
    buf_p += c;
    ADD_PIPE;

    /* NEXT HOP */
    if (bgpstream_addr_ntop(buf_p, B_REMAIN, &elem->nexthop) != NULL) {
      SEEK_STR_END;
    }
    ADD_PIPE;

    /* LOCAL_PREF */
    c = snprintf(buf_p, B_REMAIN, "0");
    written += c;
    buf_p += c;
    ADD_PIPE;
    // FIXME

    /* MED */
    c = snprintf(buf_p, B_REMAIN, "0");
    written += c;
    buf_p += c;
    ADD_PIPE;
    // FIXME

    /* COMMUNITIES */
    c = bgpstream_community_set_snprintf(buf_p, B_REMAIN, elem->communities);
    written += c;
    buf_p += c;
    if (B_FULL)
      return NULL;
    ADD_PIPE;

    /* AGGREGATE AG/NAG */
    // FIXME
    c = snprintf(buf_p, B_REMAIN, "NAG");
    written += c;
    buf_p += c;
    ADD_PIPE;

    /* AGGREGATOR AS AND IP */
    // FIXME
    ADD_PIPE;

    break;
  case BGPSTREAM_ELEM_TYPE_WITHDRAWAL:
    /* PREFIX */
    if (bgpstream_pfx_snprintf(buf_p, B_REMAIN,
                               (bgpstream_pfx_t *)&(elem->prefix)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Malformed prefix (R/A)");
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

  if (B_FULL)
    return NULL;

  return buf;
}

/** Write the string representation of the record type into the provided buffer
 *
 * @param buf           pointer to a char array
 * @param len           length of the char array
 * @param type          record type to convert to string
 * @return the number of characters that would have been written if len was
 * unlimited
 */
int bgpstream_record_type_bgpdump_snprintf(char *buf, size_t len,
                                   bgpstream_record_type_t type){

  /* ensure we have enough bytes to write our single character */
  if (len == 0) {
    return -1;
  } else if (len == 1) {
    buf[0] = '\0';
    return -1;
  }
  switch (type) {
  case BGPSTREAM_RIB:
    buf[0] = 'R';
    break;
  case BGPSTREAM_UPDATE:
    buf[0] = 'U';
    break;
  default:
    buf[0] = '\0';
    break;
  }
  buf[1] = '\0';
  return 1;

}
