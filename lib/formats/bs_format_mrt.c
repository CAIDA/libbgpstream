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

#include "bs_format_mrt.h"
#include "bgpstream_format_interface.h"
#include "bgpstream_record_int.h"
#include "parsebgp.h"
#include "bgpstream_elem_generator.h"
#include "bgpstream_log.h"
#include "utils.h"
#include <assert.h>

#define STATE ((state_t*)(format->state))
#define FDATA ((parsebgp_msg_t*)(record->__format_data->data))

// read in chunks of 1MB to minimize the number of partial parses we end up
// doing
#define BUFLEN 1024 * 1024

typedef struct state {

  // options for libparsebgp
  parsebgp_opts_t parser_opts;

  // raw data buffer
  // TODO: once parsebgp supports reading using a read callback, just pass the
  // transport callback to the parser
  uint8_t buffer[BUFLEN];

  // number of bytes left to read in the buffer
  size_t remain;

  // pointer into buffer
  uint8_t *ptr;

  // elem generator instance
  bgpstream_elem_generator_t *elem_generator;

  // the total number of successful (filtered and not) reads
  uint64_t successful_read_cnt;

  // the number of non-filtered reads (i.e. "useful")
  uint64_t valid_read_cnt;

} state_t;

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

static int populate_elem_generator(bgpstream_elem_generator_t *gen,
                                   parsebgp_msg_t *msg)
{
  /* mark the generator as having no elems */
  bgpstream_elem_generator_empty(gen);

  if (msg == NULL) {
    return 0;
  }

  // TODO

  return 0;
}

static ssize_t refill_buffer(bgpstream_format_t *format, uint8_t *buf,
                             size_t buflen, size_t remain)
{
  size_t len = 0;
  int64_t new_read = 0;

  if (remain > 0) {
    // need to move remaining data to start of buffer
    memmove(buf, buf + buflen - remain, remain);
    len += remain;
  }

  // try and do a read
  if ((new_read = bgpstream_transport_read(format->transport, buf + len,
                                           buflen - len)) < 0) {
    // read failed
    return new_read;
  }

  // new_read could be 0, indicating EOF, so need to check returned len is
  // larger than passed in remain
  return len + new_read;
}

static bgpstream_format_status_t handle_eof(bgpstream_format_t *format,
                                            bgpstream_record_t *record,
                                            uint64_t skipped_cnt)
{
  // just to be kind, set the record time to the dump time
  record->attributes.record_time = record->attributes.dump_time;

  if (skipped_cnt == 0) {
    // signal that the previous record really was the last in the dump
    record->dump_pos = BGPSTREAM_DUMP_END;
  }
  // was this the first thing we tried to read?
  if (STATE->successful_read_cnt == 0) {
    // then it is an empty file
    record->status = BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE;
    record->dump_pos = BGPSTREAM_DUMP_END;
    return BGPSTREAM_FORMAT_EMPTY_DUMP;
  }

  // so we managed to read some things, but did we get anything useful from
  // this file?
  if (STATE->valid_read_cnt == 0) {
    // dump contained data, but we filtered all of them out
    record->status = BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE;
    record->dump_pos = BGPSTREAM_DUMP_END;
    return BGPSTREAM_FORMAT_FILTERED_DUMP;
  }

  // otherwise, signal end of dump (record has not been filled)
  return BGPSTREAM_FORMAT_END_OF_DUMP;
}

/* ==================== PUBLIC API BELOW HERE ==================== */

int bs_format_mrt_create(bgpstream_format_t *format,
                         bgpstream_resource_t *res)
{
  BS_FORMAT_SET_METHODS(mrt, format);
  parsebgp_opts_t *opts = NULL;

  if ((format->state = malloc_zero(sizeof(state_t))) == NULL) {
    return -1;
  }

  if ((STATE->elem_generator = bgpstream_elem_generator_create()) == NULL) {
    return -1;
  }

  opts = &STATE->parser_opts;
  parsebgp_opts_init(opts);

  // select only the Path Attributes that we care about
  opts->bgp.path_attr_filter_enabled = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_ORIGIN] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_AS_PATH] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_NEXT_HOP] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_MED] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_LOCAL_PREF] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_ATOMIC_AGGREGATE] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_AGGREGATOR] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_COMMUNITIES] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_UNREACH_NLRI] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_AS4_PATH] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_AS4_AGGREGATOR] = 1;

  // DEBUG: (switch to ignore in production)
  opts->ignore_not_implemented = 0;

  return 0;
}

bgpstream_format_status_t
bs_format_mrt_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{
  assert(record->__format_data->format == format);
  assert(FDATA == NULL);

  int refill = 0;
  ssize_t fill_len = 0;
  size_t dec_len = 0;
  uint64_t skipped_cnt = 0;
  parsebgp_error_t err;

 refill:
  if (STATE->remain == 0 || refill != 0) {
    // try to refill the buffer
    if ((fill_len =
         refill_buffer(format, STATE->buffer, BUFLEN, STATE->remain)) == 0) {
      // EOF
      return handle_eof(format, record, skipped_cnt);
    }
    if (fill_len < 0) {
      // read error
      // TODO: create a specific read error failure so that perhaps BGPStream
      // can retry
      bgpstream_log(BGPSTREAM_LOG_WARN, "DEBUG: Failed read");
      return -1;
    }
    if (fill_len == STATE->remain) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "DEBUG: Corrupted dump or failed read\n");
      return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
    }
    // here we have something new to read
    STATE->remain = fill_len;
    STATE->ptr = STATE->buffer;
  }

  while (STATE->remain > 0) {
    if (FDATA == NULL &&
        (record->__format_data->data = parsebgp_create_msg()) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "ERROR: Failed to create message structure\n");
      return -1;
    }

    dec_len = STATE->remain;
    if ((err = parsebgp_decode(STATE->parser_opts, PARSEBGP_MSG_TYPE_MRT,
                               FDATA, STATE->ptr, &dec_len)) != PARSEBGP_OK) {
      if (err == PARSEBGP_PARTIAL_MSG) {
        // refill the buffer and try again
        refill = 1;
        goto refill;
      }
      // else: its a fatal error
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "ERROR: Failed to parse message (%d:%s)\n", err,
                    parsebgp_strerror(err));
      return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
    }
    // else: successful read
    STATE->ptr += dec_len;
    STATE->remain -= dec_len;

    // got a message!
    assert(FDATA != NULL);
    assert(FDATA->type == PARSEBGP_MSG_TYPE_MRT);

    STATE->successful_read_cnt++;

    // check the filters
    if (is_wanted_time(FDATA->types.mrt.timestamp_sec, format->filter_mgr) !=
        0) {
      // we want this entry
      STATE->valid_read_cnt++;
      break;
    } else {
      // we dont want this entry, destroy it
      parsebgp_destroy_msg(FDATA);
      record->__format_data->data = NULL;
      if (skipped_cnt == UINT64_MAX) {
        // probably this will never happen, but lets just be careful we don't
        // wrap and think we haven't skipped anything
        skipped_cnt = 0;
      }
      skipped_cnt++;
      // fall through and repeat loop
    }
  }

  if (STATE->remain == 0 && FDATA == NULL) {
    // EOF
    return handle_eof(format, record, skipped_cnt);
  }

  // valid message, and it passes our filters
  record->status = BGPSTREAM_RECORD_STATUS_VALID_RECORD;

  // if this is the first record we read and no previous
  // valid record has been discarded because of time
  if (STATE->valid_read_cnt == 1 && STATE->successful_read_cnt == 1) {
    record->dump_pos = BGPSTREAM_DUMP_START;
  } else {
    record->dump_pos = BGPSTREAM_DUMP_MIDDLE;
    // NB when the *next* record is pre-fetched, this may be changed to
    // end-of-dump by the reader (since we'll discover that there are no more
    // records)
  }

  // update the record time
  record->attributes.record_time = FDATA->types.mrt.timestamp_sec;

  // we successfully read a record, return it
  return BGPSTREAM_FORMAT_OK;
}

int bs_format_mrt_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  if (bgpstream_elem_generator_is_populated(STATE->elem_generator) == 0 &&
      populate_elem_generator(STATE->elem_generator, FDATA) != 0) {
    return -1;
  }
  *elem = bgpstream_elem_generator_get_next_elem(STATE->elem_generator);
  if (*elem == NULL) {
    return 0;
  }
  return 1;
}

void bs_format_mrt_destroy_data(bgpstream_format_t *format, void *data)
{
  bgpstream_elem_generator_clear(STATE->elem_generator);
  parsebgp_destroy_msg((parsebgp_msg_t*)data);
}

void bs_format_mrt_destroy(bgpstream_format_t *format)
{
  bgpstream_elem_generator_destroy(STATE->elem_generator);
  STATE->elem_generator = NULL;

  free(format->state);
  format->state = NULL;
}
