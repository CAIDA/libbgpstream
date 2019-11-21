/*
 * Copyright (C) 2015 The Regents of the University of California.
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

#include "bs_format_bmp.h"
#include "bgpstream_format_interface.h"
#include "bgpstream_record_int.h"
#include "bgpstream_log.h"
#include "bgpstream_parsebgp_common.h"
#include "utils.h"
#include <assert.h>

#define STATE ((state_t *)(format->state))

#define RDATA ((rec_data_t *)(record->__int->data))

#define TIF filter_mgr->time_interval

typedef struct rec_data {

  // reusable elem instance
  bgpstream_elem_t *elem;

  // have we extracted all the possible elems out of the current message?
  int end_of_elems;

  // have we extracted the peer header info into the elem?
  int peer_hdr_done;

  // state for UPDATE elem extraction
  bgpstream_parsebgp_upd_state_t upd_state;

  // reusable parser message structure
  parsebgp_msg_t *msg;

} rec_data_t;

typedef struct state {

  // parsebgp decode wrapper state
  bgpstream_parsebgp_decode_state_t decoder;

} state_t;

static int handle_update(rec_data_t *rd, parsebgp_bgp_msg_t *bgp)
{
  int rc;

  if ((rc = bgpstream_parsebgp_process_update(&rd->upd_state, rd->elem, bgp)) <
      0) {
    return rc;
  }
  if (rc == 0) {
    rd->end_of_elems = 1;
  }
  return rc;
}

static int handle_peer_up_down(rec_data_t *rd, int peer_up)
{
  rd->elem->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
  rd->elem->old_state = BGPSTREAM_ELEM_PEERSTATE_UNKNOWN;
  if (peer_up) {
    rd->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_ACTIVE;
  } else {
    rd->elem->new_state = BGPSTREAM_ELEM_PEERSTATE_IDLE;
  }

  rd->end_of_elems = 1;
  return 1;
}

static int handle_peer_hdr(bgpstream_elem_t *el, parsebgp_bmp_msg_t *bmp)
{
  parsebgp_bmp_peer_hdr_t *hdr = &bmp->peer_hdr;

  // Timestamps
  el->orig_time_sec = hdr->ts_sec;
  el->orig_time_usec = hdr->ts_usec;

  // Peer Address
  COPY_IP(&el->peer_ip, hdr->afi, hdr->addr, return -1);

  // Peer ASN
  el->peer_asn = hdr->asn;

  return 0;
}

/* -------------------- RECORD FILTERING -------------------- */

static int check_filters(bgpstream_record_t *record,
                         bgpstream_filter_mgr_t *filter_mgr)
{
  // Collector
  if (filter_mgr->collectors != NULL) {
    if (bgpstream_str_set_exists(filter_mgr->collectors,
                                 record->collector_name) == 0) {
      return 0;
    }
  }

  // Router
  if (filter_mgr->routers != NULL) {
    if (bgpstream_str_set_exists(filter_mgr->routers, record->router_name) ==
        0) {
      return 0;
    }
  }

  return 1;
}

static int is_wanted_time(uint32_t record_time,
                          bgpstream_filter_mgr_t *filter_mgr)
{
  if (TIF==NULL ||
      (record_time >= TIF->begin_time &&
      (TIF->end_time == BGPSTREAM_FOREVER || record_time <= TIF->end_time))) {
    // matches a filter interval
    return 1;
  }

  return 0;
}

#define DESERIALIZE_VAL(to)                                                    \
  do {                                                                         \
    if (((len) - (nread)) < sizeof(to)) {                                      \
      return -1;                                                               \
    }                                                                          \
    memcpy(&(to), (buf), sizeof(to));                                          \
    nread += sizeof(to);                                                       \
    buf += sizeof(to);                                                         \
  } while (0)

#define IS_ROUTER_MSG (flags & 0x80)
#define IS_ROUTER_IPV6 (flags & 0x40)

static int populate_prep_cb(bgpstream_format_t *format, uint8_t *buf,
                            size_t *lenp, bgpstream_record_t *record)
{
  size_t len = *lenp, nread = 0;
  int newln = 0;
  uint8_t ver_maj, ver_min, flags, u8;
  uint16_t u16;
  uint32_t u32;
  int name_len = 0;

  // we want at least a few bytes to do header checks
  if (len < 4) {
    *lenp = 0;
    return 0;
  }

  // is this an OpenBMP ASCII header (either "text" or "legacy-text")?
  if (*buf == 'V') {
    // skip until we find double-newlines
    while ((len - nread) > 0) {
      if (newln == 2) {
        // this is the first byte of the payload
        *lenp = nread;
        return 0;
      }
      if (*buf == '\n') {
        newln++;
      } else {
        newln = 0;
      }
      nread++;
      buf++;
    }
    // if we reach here, then we've failed to parse the header. just give up
    *lenp = 0;
    return 0;
  }

  // double-check the magic number
  if (memcmp(buf, "OBMP", 4) != 0) {
    // it's not a known OpenBMP header, assume that it is raw BMP
    *lenp = 0;
    return 0;
  }
  nread += 4;
  buf += 4;

  // Confirm the version number
  DESERIALIZE_VAL(ver_maj);
  DESERIALIZE_VAL(ver_min);
  if (ver_maj != 1 || ver_min != 7) {
    bgpstream_log(BGPSTREAM_LOG_WARN,
                  "Unrecognized OpenBMP header version (%" PRIu8 ".%" PRIu8 ")",
                  ver_maj, ver_min);
    return 0;
  }

  // skip past the header length and the message length (since we'll parse the
  // entire header anyway).
  nread += 2 + 4;
  buf += 2 + 4;

  // read the flags
  DESERIALIZE_VAL(flags);
  // check the flags
  if (!IS_ROUTER_MSG) {
    // we only care about bmp raw messages, which are always router messages
    return 0;
  }

  // check the object type
  DESERIALIZE_VAL(u8);
  if (u8 != 12) {
    // we only want BMP RAW messages, so skip this one
    return 0;
  }

  // load the time stamps into the record
  DESERIALIZE_VAL(u32);
  record->time_sec = ntohl(u32);
  DESERIALIZE_VAL(u32);
  record->time_usec = ntohl(u32);

  // skip past the collector hash
  nread += 16;
  buf += 16;

  // grab the collector admin ID as collector name
  // TODO: if there is no admin ID, use the hash
  DESERIALIZE_VAL(u16);
  u16 = ntohs(u16);
  // maybe truncate the collector name
  if (u16 < BGPSTREAM_UTILS_STR_NAME_LEN) {
    name_len = u16;
  } else {
    name_len = BGPSTREAM_UTILS_STR_NAME_LEN - 1;
  }
  // copy the collector name in
  if ((len - nread) < u16) {
    return -1;
  }
  memcpy(record->collector_name, buf, name_len);
  record->collector_name[name_len] = '\0';
  nread += u16;
  buf += u16;

  if ((len - nread) < 32) {
    // not enough buffer left for router hash and IP
    return -1;
  }

  // skip past the router hash
  nread += 16;
  buf += 16;

  // grab the router IP
  if (IS_ROUTER_IPV6) {
    bgpstream_ipv6_addr_init(&record->router_ip, buf);
  } else {
    bgpstream_ipv4_addr_init(&record->router_ip, buf);
  }
  nread += 16;
  buf += 16;

  // router name
  // TODO: if there is no name, or it is "default", use the IP
  DESERIALIZE_VAL(u16);
  u16 = ntohs(u16);
  // maybe truncate the router name
  if (u16 < BGPSTREAM_UTILS_STR_NAME_LEN) {
    name_len = u16;
  } else {
    name_len = BGPSTREAM_UTILS_STR_NAME_LEN - 1;
  }
  // copy the router name in
  if ((len - nread) < u16) {
    return -1;
  }
  memcpy(record->router_name, buf, name_len);
  record->router_name[name_len] = '\0';
  nread += u16;
  buf += u16;

  // and then ignore the row count
  nread += 4;
  buf += 4;

  *lenp = nread;
  return 0;
}

static bgpstream_parsebgp_check_filter_rc_t
populate_filter_cb(bgpstream_format_t *format, bgpstream_record_t *record,
                   parsebgp_msg_t *msg)
{
  parsebgp_bmp_msg_t *bmp = msg->types.bmp;
  uint32_t ts_sec = record->time_sec;
  assert(msg->type == PARSEBGP_MSG_TYPE_BMP);

  // for now we only care about ROUTE_MON, PEER_DOWN, and PEER_UP messages
  if (bmp->type != PARSEBGP_BMP_TYPE_ROUTE_MON &&
      bmp->type != PARSEBGP_BMP_TYPE_PEER_DOWN &&
      bmp->type != PARSEBGP_BMP_TYPE_PEER_UP) {
    return BGPSTREAM_PARSEBGP_FILTER_OUT;
  }

  // and we are only interested in UPDATE messages
  if (bmp->type == PARSEBGP_BMP_TYPE_ROUTE_MON &&
      bmp->types.route_mon->type != PARSEBGP_BGP_TYPE_UPDATE) {
    return BGPSTREAM_PARSEBGP_FILTER_OUT;
  }

  // is this from a collector and router that we care about?
  if (check_filters(record, format->filter_mgr) == 0) {
    return BGPSTREAM_PARSEBGP_FILTER_OUT;
  }

  // if this is pure BMP, then the record timestamps will be unset!

  // is this above our interval
  if (format->TIF != NULL &&
      format->TIF->end_time != BGPSTREAM_FOREVER &&
      ts_sec > format->TIF->end_time) {
    // force EOS
    return BGPSTREAM_PARSEBGP_EOS;
  }

  // check the filters
  if (is_wanted_time(ts_sec, format->filter_mgr) != 0) {
    // we want this entry
    return BGPSTREAM_PARSEBGP_KEEP;
  } else {
    return BGPSTREAM_PARSEBGP_FILTER_OUT;
  }
}

/* ==================== PUBLIC API BELOW HERE ==================== */

int bs_format_bmp_create(bgpstream_format_t *format, bgpstream_resource_t *res)
{
  BS_FORMAT_SET_METHODS(bmp, format);
  parsebgp_opts_t *opts = NULL;

  if ((format->state = malloc_zero(sizeof(state_t))) == NULL) {
    return -1;
  }

  STATE->decoder.msg_type = PARSEBGP_MSG_TYPE_BMP;

  opts = &STATE->decoder.parser_opts;
  parsebgp_opts_init(opts);
  bgpstream_parsebgp_opts_init(opts);

  // DEBUG: force parsebgp to ignore things that it doesn't know about
  opts->ignore_not_implemented = 1;
  // and not be chatty about them
  opts->silence_not_implemented = 1;

  return 0;
}

bgpstream_format_status_t
bs_format_bmp_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{
  bgpstream_format_status_t rc = bgpstream_parsebgp_populate_record(
    &STATE->decoder, RDATA->msg, format, record, populate_prep_cb,
    populate_filter_cb);

  if (record->status != BGPSTREAM_RECORD_STATUS_VALID_RECORD) {
    record->router_name[0] = '\0';
    record->router_ip.version = 0;
  }

  return rc;
}

int bs_format_bmp_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  parsebgp_bmp_msg_t *bmp;
  int rc = 0;
  *elem = NULL;

  if (RDATA == NULL || RDATA->end_of_elems != 0) {
    // end-of-elems
    return 0;
  }

  bmp = RDATA->msg->types.bmp;

  // assume we'll find at least something juicy, so process the peer header and
  // fill the common parts of the elem
  if (RDATA->peer_hdr_done == 0 && handle_peer_hdr(RDATA->elem, bmp) != 0) {
    return -1;
  }
  RDATA->peer_hdr_done = 1;

  // what kind of BMP message are we dealing with?
  switch (bmp->type) {
  case PARSEBGP_BMP_TYPE_ROUTE_MON:
    rc = handle_update(RDATA, bmp->types.route_mon);
    break;

  case PARSEBGP_BMP_TYPE_PEER_DOWN:
    rc = handle_peer_up_down(RDATA, 0);
    break;

  case PARSEBGP_BMP_TYPE_PEER_UP:
    rc = handle_peer_up_down(RDATA, 1);
    break;

  default:
    // not implemented
    return 0;
  }
  if (rc <= 0) {
    return rc;
  }

  // return a borrowed pointer to the elem we populated
  *elem = RDATA->elem;
  return 1;
}

int bs_format_bmp_init_data(bgpstream_format_t *format, void **data)
{
  rec_data_t *rd;
  *data = NULL;

  if ((rd = malloc_zero(sizeof(rec_data_t))) == NULL) {
    return -1;
  }

  if ((rd->elem = bgpstream_elem_create()) == NULL) {
    return -1;
  }

  if ((rd->msg = parsebgp_create_msg()) == NULL) {
    return -1;
  }

  *data = rd;
  return 0;
}

void bs_format_bmp_clear_data(bgpstream_format_t *format, void *data)
{
  rec_data_t *rd = (rec_data_t *)data;
  assert(rd != NULL);
  bgpstream_elem_clear(rd->elem);
  rd->end_of_elems = 0;
  rd->peer_hdr_done = 0;
  bgpstream_parsebgp_upd_state_reset(&rd->upd_state);
  parsebgp_clear_msg(rd->msg);
}

void bs_format_bmp_destroy_data(bgpstream_format_t *format, void *data)
{
  rec_data_t *rd = (rec_data_t *)data;
  if (rd == NULL) {
    return;
  }
  bgpstream_elem_destroy(rd->elem);
  rd->elem = NULL;
  parsebgp_destroy_msg(rd->msg);
  rd->msg = NULL;
  free(data);
}

void bs_format_bmp_destroy(bgpstream_format_t *format)
{
  free(format->state);
  format->state = NULL;
}
