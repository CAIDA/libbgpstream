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

#include <bgpstream_elem.h>
#include <bgpstream_record.h>
#include <bgpstream_options.h>
#include <bl_bgp_utils.h>


// Opaque Data Structures
typedef struct struct_bgpstream_t bgpstream_t;

/* allocate memory for bgpstream interface */
bgpstream_t *bgpstream_create();

/* configure filters in order to select a subset of the bgp data available */
void bgpstream_add_filter(bgpstream_t *bs,
                          bgpstream_filter_type filter_type,
			  const char* filter_value);

void bgpstream_add_interval_filter(bgpstream_t *bs,
                                   bgpstream_filter_type filter_type,
				   const char *filter_start,
                                   const char *filter_stop);

/* configure the data interface */
void bgpstream_set_data_interface(bgpstream_t *bs,
                                  bgpstream_datasource_type datasource);

/* set up options for the data interface */
void bgpstream_set_data_interface_options(bgpstream_t *bs,
                                        bgpstream_datasource_option option_type,
				        char *option);

/* configure the interface so that it blocks waiting for new data */
void bgpstream_set_blocking(bgpstream_t *bs);

/* turn on the bgpstream interface, i.e.: it makes the interface ready
 * for a new get next call */
int bgpstream_start(bgpstream_t *bs);

/* assign to bs_record the next record ordered by time among all those available
 * (data collected are first filtered using the filters if set)
 * return:
 * - > 0 if a new record has been read correctly
 * -   0 if no new data are available
 * - < 0 if an error occurred
 */
int bgpstream_get_next_record(bgpstream_t *bs,
                              bgpstream_record_t *bs_record);

/* turn off the bgpstream interface */
void bgpstream_stop(bgpstream_t *bs);

/* destroy the memory allocated for bgpstream interface */
void bgpstream_destroy(bgpstream_t *bs);


#endif /* _BGPSTREAM_LIB_H */
