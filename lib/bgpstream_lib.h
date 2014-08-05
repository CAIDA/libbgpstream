/*
 * libbgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2013 The Regents of the University of California.
 *
 * This file is part of libbgpstream.
 *
 * libbgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libbgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _BGPSTREAM_LIB_H
#define _BGPSTREAM_LIB_H

#include "bgpstream_datasource.h"
#include "bgpstream_input.h"
#include "bgpstream_reader.h"
#include "bgpstream_filter.h"
#include "bgpstream_record.h"
#include "bgpstream_elem.h"


/* bgpstream_record_t type defined in
 *  -> bgpstream_record.h 
 *
 * bgpstream_elem_t type defined in
 *  -> bgpstream_elem.h 
 *
 * bgpstream_filter_type defined in 
 * -> bgpstream_filter.h
 *
 */

typedef enum {ALLOCATED, ON, OFF} bgpstream_status;

typedef struct struct_bgpstream_t {  
  bgpstream_input_mgr_t *input_mgr;
  bgpstream_reader_mgr_t *reader_mgr;
  bgpstream_filter_mgr_t *filter_mgr;
  bgpstream_datasource_mgr_t *datasource_mgr;
  bgpstream_status status;  
} bgpstream_t;


/* prototypes */

/* allocate memory for bgpstream interface */
bgpstream_t *bgpstream_create();

/* configure filters in order to select a subset of the bgp data available */
void bgpstream_add_filter(bgpstream_t * const bs, bgpstream_filter_type filter_type,
			  const char* filter_value);
void bgpstream_add_interval_filter(bgpstream_t * const bs, bgpstream_filter_type filter_type,
				   const char* filter_start, const char* filter_stop);

/* configure the data interface */
void bgpstream_set_data_interface(bgpstream_t * const bs, const char *datasource_name);

/* configure the interface so that it blocks waiting for new data */
void bgpstream_set_blocking(bgpstream_t * const bs);

/* turn on the bgpstream interface, i.e.: it makes the interface ready
 * for a new get next call */
int bgpstream_init(bgpstream_t * const bs);

/* allocate memory for a bs_record (the client can refer to this
 * memory, however, if it has to save this object, it needs to
 * copy the memory itself) */
bgpstream_record_t *bgpstream_create_record();

/* assign to bs_record the next record ordered by time among all those available
 * (data collected are first filtered using the filters if set) 
 * return:
 * - > 0 if a new record has been read correctly
 * -   0 if no new data are available
 * - < 0 if an error occurred
 */
int bgpstream_get_next_record(bgpstream_t * const bs, bgpstream_record_t * const bs_record);

/* extract a list of elements from the bgpstream record  */
bgpstream_elem_t * bgpstream_get_elem_queue(bgpstream_record_t * const bs_record);

/* destroy the queue */
void bgpstream_destroy_elem_queue(bgpstream_elem_t * bse_queue);

/* deallocate memory for the bs_record */
void bgpstream_destroy_record(bgpstream_record_t * const bs_record);

/* turn off the bgpstream interface */
void bgpstream_close(bgpstream_t * const bs);

/* destroy the memory allocated for bgpstream interface */
void bgpstream_destroy(bgpstream_t * const bs);


#endif /* _BGPSTREAM_LIB_H */
