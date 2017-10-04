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

#ifndef __BGPSTREAM_READER_H
#define __BGPSTREAM_READER_H

#include "bgpstream_resource.h"
#include "bgpstream_filter.h"

/** Opaque structure representing a reader instance */
typedef struct bgpstream_reader bgpstream_reader_t;

/** Return codes for get_next_record */
typedef enum {

  /** An error occurred */
  BGPSTREAM_READER_STATUS_ERROR = -1,

  /** End-Of-Stream */
  BGPSTREAM_READER_STATUS_EOS = 0,

  /** Empty stream (AGAIN) */
  BGPSTREAM_READER_STATUS_AGAIN = 1,

  /** Successful read */
  BGPSTREAM_READER_STATUS_OK = 2,

} bgpstream_reader_status_t;

/** Create a new reader for the given resource */
bgpstream_reader_t *
bgpstream_reader_create(bgpstream_resource_t *resource,
                        bgpstream_filter_mgr_t *filter_mgr);

/** Get the time of the next record available in the reader
 *
 * @param reader        pointer to the format object
 * @return the time of the next record to be returned by the reader
 */
uint32_t bgpstream_reader_get_next_time(bgpstream_reader_t *reader);

/** Block until the resource has opened */
int bgpstream_reader_open_wait(bgpstream_reader_t *reader);

/** Destroy the given reader */
void bgpstream_reader_destroy(bgpstream_reader_t *reader);

/** Populate the given record with the next data available
 *
 * @param reader        pointer to a reader instance
 * @param[out] record   set to a borrowed pointer to a record if the return
 *                      code is >0
 * @return -1 if an unrecoverable error occurred, 0 if there are no further
 * records to be read (i.e. EOS has been reached), 1 if the record has not been
 * populated, but a future call yield data (only used by stream resource), and 2
 * if the record has been populated correctly, and there is at least one more
 * record to be read.
 */
bgpstream_reader_status_t
bgpstream_reader_get_next_record(bgpstream_reader_t *reader,
                                 bgpstream_record_t **record);

#endif /* __BGPSTREAM_READER_H */
