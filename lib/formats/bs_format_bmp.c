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

#include "bs_format_bmp.h"
#include "bgpstream_format_interface.h"
#include "bgpstream_record_int.h"
#include "bgpstream_log.h"
#include "bgpstream_parsebgp_common.h"
#include "utils.h"
#include <assert.h>

#define STATE ((state_t*)(format->state))

#define RDATA ((rec_data_t *)(record->__format_data->data))

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

  if ((rc = bgpstream_parsebgp_process_update(&rd->upd_state, rd->elem,
                                              bgp)) < 0) {
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

  // TODO: fix this after talking with Tim
  // it is possible we can assume UP means IDLE->ACTIVE
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
  el->timestamp = hdr->ts_sec;
  el->timestamp_usec = hdr->ts_usec;

  // Peer Address
  COPY_IP(&el->peer_address, hdr->afi, hdr->addr, return -1);

  // Peer ASN
  el->peer_asnumber = hdr->asn;

  return 0;
}

/* -------------------- RECORD FILTERING -------------------- */

static int is_wanted_time(uint32_t record_time,
                          bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_interval_filter_t *tif;

  if (filter_mgr->time_intervals == NULL) {
    // no time filtering
    return 1;
  }

  tif = filter_mgr->time_intervals;

  while (tif != NULL) {
    if (record_time >= tif->begin_time &&
        (tif->end_time == BGPSTREAM_FOREVER || record_time <= tif->end_time)) {
      // matches a filter interval
      return 1;
    }
    tif = tif->next;
  }

  return 0;
}

enum header_row_types {
  OBMP_VERSION = 0,
  OBMP_COLLECTOR_HASH_ID = 1,
  OBMP_ROUTER_HASH = 2,
  OBMP_ROUTER_IP = 3,
  OBMP_LEN = 4,
  OBMP_EMPTY = 5,
};

static int populate_prep_cb(bgpstream_format_t *format, uint8_t *buf,
                            size_t *lenp, bgpstream_record_t *record)
{
  size_t len = *lenp, nread = 0;
  int line_n = 0, done = 0;

  /* Parse a gory openbmp raw header that looks like this:
     V: 1.6\n
     C_HASH_ID: 31391b6fca3a27567f55f56d113a204a\n
     R_HASH: 53be25f18ec900afc90cb90bb942b889\n
     R_IP: 128.223.51.103\n
     L: 130\n
     \n
  */

  if (*buf != 'V') {
    // assume this is a raw BMP message (not OpenBMP)
    *lenp = 0;
    return 0;
  }

  // read each of the header rows
  for (line_n = 0; line_n <= OBMP_EMPTY; line_n++) {
    if (nread > len) {
      return -1;
    }

    // TODO: do something with the data
    // for now, just skip it

    if (line_n == OBMP_EMPTY) {
      done = 1;
    }

    // fast-forward to the end of the line
    for (; nread <= len; nread++) {
      if (buf[nread] == '\n') {
        nread++;
        break;
      }
    }
  }

  if (done != 0) {
    *lenp = nread;
    return 0;
  }

  return -1;
}

static bgpstream_parsebgp_check_filter_rc_t
populate_filter_cb(bgpstream_format_t *format, parsebgp_msg_t *msg,
                   uint32_t *ts_sec)
{
  parsebgp_bmp_msg_t *bmp = msg->types.bmp;
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

  // be careful! PARSEBGP_BMP_TYPE_INIT_MSG and PARSEBGP_BMP_TYPE_TERM_MSG
  // messages don't have the peer header, and so don't have a timestamp!
  // this format definitely wasn't made for data serialization...

  // check the filters
  if (is_wanted_time(bmp->peer_hdr.ts_sec, format->filter_mgr) != 0) {
    // we want this entry
    *ts_sec = bmp->peer_hdr.ts_sec;
    return BGPSTREAM_PARSEBGP_KEEP;
  } else {
    return BGPSTREAM_PARSEBGP_FILTER_OUT;
  }
}

/* ==================== PUBLIC API BELOW HERE ==================== */

int bs_format_bmp_create(bgpstream_format_t *format,
                         bgpstream_resource_t *res)
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
  return bgpstream_parsebgp_populate_record(&STATE->decoder, RDATA->msg, format,
                                            record,
                                            populate_prep_cb,
                                            populate_filter_cb);
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
    // TODO: explicitly handle end-of-RIB marker
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
  rec_data_t *rd = (rec_data_t*)data;
  assert(rd != NULL);
  bgpstream_elem_clear(rd->elem);
  rd->end_of_elems = 0;
  rd->peer_hdr_done = 0;
  bgpstream_parsebgp_upd_state_reset(&rd->upd_state);
  parsebgp_clear_msg(rd->msg);
}

void bs_format_bmp_destroy_data(bgpstream_format_t *format, void *data)
{
  rec_data_t *rd = (rec_data_t*)data;
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
