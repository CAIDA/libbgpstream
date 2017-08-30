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

typedef struct state {

  // parsebgp decode wrapper state
  bgpstream_parsebgp_decode_state_t decoder;

  // reusable elem instance
  bgpstream_elem_t *elem;

  // have we extracted all the possible elems out of the current message?
  int end_of_elems;

} state_t;

static void reset_generator(bgpstream_format_t *format)
{
  bgpstream_elem_clear(STATE->elem);
  STATE->end_of_elems = 0;
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

static int populate_filter_cb(bgpstream_format_t *format,
                              parsebgp_msg_t *msg,
                              uint32_t *ts_sec)
{
  parsebgp_bmp_msg_t *bmp = &msg->types.bmp;
  assert(msg->type == PARSEBGP_MSG_TYPE_BMP);

  // for now we only care about ROUTE_MON, PEER_DOWN, and PEER_UP messages
  if (bmp->type != PARSEBGP_BMP_TYPE_ROUTE_MON &&
      bmp->type != PARSEBGP_BMP_TYPE_PEER_DOWN &&
      bmp->type != PARSEBGP_BMP_TYPE_PEER_UP) {
    return 0;
  }

  // and we are only interested in UPDATE messages
  if (bmp->type == PARSEBGP_BMP_TYPE_ROUTE_MON &&
      bmp->types.route_mon.type != PARSEBGP_BGP_TYPE_UPDATE) {
    return 0;
  }

  // be careful! PARSEBGP_BMP_TYPE_INIT_MSG and PARSEBGP_BMP_TYPE_TERM_MSG
  // messages don't have the peer header, and so don't have a timestamp!
  // this format definitely wasn't made for data serialization...

  // check the filters
  if (is_wanted_time(bmp->peer_hdr.ts_sec, format->filter_mgr) != 0) {
    // we want this entry
    *ts_sec = bmp->peer_hdr.ts_sec;
    return 1;
  } else {
    return 0;
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

  if ((STATE->elem = bgpstream_elem_create()) == NULL) {
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
  reset_generator(format);
  return bgpstream_parsebgp_populate_record(&STATE->decoder, format, record,
                                            populate_filter_cb);
}

int bs_format_bmp_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  *elem = NULL;

  // TODO: actually implement this
  STATE->end_of_elems = 1;
  if (BGPSTREAM_PARSEBGP_FDATA == NULL || STATE->end_of_elems != 0) {
    // end-of-elems
    return 0;
  }

  // return a borrowed pointer to the elem we populated
  *elem = STATE->elem;
  return 1;
}

void bs_format_bmp_destroy_data(bgpstream_format_t *format, void *data)
{
  reset_generator(format);
  parsebgp_destroy_msg((parsebgp_msg_t*)data);
}

void bs_format_bmp_destroy(bgpstream_format_t *format)
{
  bgpstream_elem_destroy(STATE->elem);
  STATE->elem = NULL;

  free(format->state);
  format->state = NULL;
}
