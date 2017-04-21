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

/** Create a new reader for the given resource */
bgpstream_reader_t *
bgpstream_reader_create(bgpstream_resource_t *resource,
                        bgpstream_filter_mgr_t *filter_mgr);

/** Block until the resource has opened */
int bgpstream_reader_open_wait(bgpstream_reader_t *reader);

/** Destroy the given reader */
void bgpstream_reader_destroy(bgpstream_reader_t *reader);

/** Populate the given record with the next data available
 *
 * @param reader        pointer to a reader instance
 * @param record        pointer to a record to populate
 * @return <0 if an error occurred, 0 if the record is populated correctly but
 * there are no further records to be read (i.e. EOF has been reached), >0 if
 * the record has been populated correctly, and there is at least one more
 * record to be read
 *
 * This function also updates the `current_time` field of the resource
 * associated with the reader to reflect the time of the _next_ record available
 * from the reader
 */
int bgpstream_reader_get_next_record(bgpstream_reader_t *reader,
                                     bgpstream_record_t *record);


#endif /* __BGPSTREAM_READER_H */
