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
#include "bgpdump_lib.h"
#include "bgpstream_elem_generator.h"
#include "bgpstream_log.h"
#include "utils.h"
#include <assert.h>

#define STATE ((state_t*)(format->state))
#define FDATA ((BGPDUMP_ENTRY*)(record->__format_data->data))

typedef struct state {

  // bgpdump instance (TODO: replace with parsebgp instance)
  BGPDUMP *bgpdump;

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

/* ==================== PUBLIC API BELOW HERE ==================== */

int bs_format_mrt_create(bgpstream_format_t *format,
                         bgpstream_resource_t *res)
{
  BS_FORMAT_SET_METHODS(mrt, format);

  if ((format->state = malloc_zero(sizeof(state_t))) == NULL) {
    return -1;
  }

  if ((STATE->elem_generator = bgpstream_elem_generator_create()) == NULL) {
    return -1;
  }

  bgpstream_log(BGPSTREAM_LOG_FINE, "Opening %s", res->uri);
  if ((STATE->bgpdump = bgpdump_open_dump(format->transport)) == NULL) {
    return -1;
  }

  return 0;
}

bgpstream_format_status_t
bs_format_mrt_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{
  assert(record->__format_data->format == format);
  assert(FDATA == NULL);
  // DEBUG: testing an assumption:
  assert(record->dump_pos != BGPSTREAM_DUMP_END);
  uint64_t skipped_cnt = 0;

  while (1) {
    // read until we either get a successful read, or some kind of explicit error,
    // don't return if its a "normal" empty read (as happens when bgpdump reads
    // the peer index table in a RIB)
    while ((record->__format_data->data = bgpdump_read_next(STATE->bgpdump)) ==
           NULL) {
      // didn't read anything... why?
      if (STATE->bgpdump->corrupted_read) {
        record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD;
        return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
      }
      if (STATE->bgpdump->eof) {
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
      // otherwise, just keep reading
    }
    assert(FDATA != NULL);

    // successful read, check the filters
    STATE->successful_read_cnt++;

    // check the filters
    if (is_wanted_time(FDATA->time, format->filter_mgr) != 0) {
      // we want this entry
      STATE->valid_read_cnt++;
      break;
    } else {
      // we dont want this entry, destroy it
      bgpdump_free_mem(FDATA);
      skipped_cnt++;
      // fall through and repeat loop
    }
  }

  // the only thing left is a good, valid read
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
  record->attributes.record_time = FDATA->time;

  // we successfully read a record, return it
  return BGPSTREAM_FORMAT_OK;
}

int bs_format_mrt_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  if (bgpstream_elem_generator_is_populated(STATE->elem_generator) == 0) {
    bgpstream_log(BGPSTREAM_LOG_WARN, "populating elem generator");
    if (bgpstream_elem_generator_populate(STATE->elem_generator, FDATA) != 0) {
      return -1;
    }
  }
  *elem = bgpstream_elem_generator_get_next_elem(STATE->elem_generator);
  if (*elem == NULL) {
      bgpstream_log(BGPSTREAM_LOG_WARN, "no more elems");
    return 0;
  }
  bgpstream_log(BGPSTREAM_LOG_WARN, "returning useful elem");
  return 1;
}

void bs_format_mrt_destroy_data(bgpstream_format_t *format, void *data)
{
  bgpstream_elem_generator_clear(STATE->elem_generator);
  bgpdump_free_mem((BGPDUMP_ENTRY*)data);
}

void bs_format_mrt_destroy(bgpstream_format_t *format)
{
  bgpdump_close_dump(STATE->bgpdump);
  STATE->bgpdump = NULL;

  bgpstream_elem_generator_destroy(STATE->elem_generator);
  STATE->elem_generator = NULL;

  free(format->state);
  format->state = NULL;
}
