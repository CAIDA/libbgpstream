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

#ifndef __BGPSTREAM_FORMAT_H
#define __BGPSTREAM_FORMAT_H

#include "bgpstream_elem_generator.h"
#include "bgpstream_filter.h"
#include "bgpstream_resource.h"

/** Generic interface to specific data format modules */
typedef struct bgpstream_format bgpstream_format_t;

/** Return codes for the populate_record method */
typedef enum {
  BGPSTREAM_FORMAT_OK,
  // all status codes other than OK signal EOF
  BGPSTREAM_FORMAT_FILTERED_DUMP,
  BGPSTREAM_FORMAT_EMPTY_DUMP,
  BGPSTREAM_FORMAT_CANT_OPEN_DUMP,
  BGPSTREAM_FORMAT_CORRUPTED_DUMP,
  BGPSTREAM_FORMAT_END_OF_DUMP,
} bgpstream_format_status_t;

/** Create a format handler for the given resource
 *
 * @param res           pointer to a resource
 * @param filter_mgr    pointer to filter manager to use for filtering records
 * @return pointer to a format module instance if successful, NULL otherwise
 */
bgpstream_format_t *
bgpstream_format_create(bgpstream_resource_t *res,
                        bgpstream_filter_mgr_t *filter_mgr);

/** Populate the given record with the next available record from this resource
 *
 * @param format        pointer to the format object to use
 * @param record        pointer to a record instance to populate
 * @return format status code, or -1 if an unhandled error occurred
 *
 * This function will respect filters set in the filter manager, only returning
 * records that match the filters. (Note that some filters like project and
 * collector are applied at a higher level, and will not be considered by this
 * function.)
 */
bgpstream_format_status_t
bgpstream_format_populate_record(bgpstream_format_t *format,
                                 bgpstream_record_t *record);

/** Populate the given elem generator with information from the given record
 *
 * @param format        pointer to the format object to use
 * @param record        pointer to the record to use
 * @param gen           pointer to the elem generator to populate
 * @return 0 if the generator was populated successfully, -1 otherwise
 */
int bgpstream_format_populate_elem_generator(bgpstream_format_t *format,
                                             bgpstream_record_t *record,
                                             bgpstream_elem_generator_t *gen);

/** Destroy the format data in a given record
 *
 * @param record        pointer to the record to destroy data for
 */
void bgpstream_format_destroy_data(bgpstream_record_t *record);

/** Destroy the given format module
 *
 * @param format        pointer to the format instance to destroy
 */
void bgpstream_format_destroy(bgpstream_format_t *format);


#endif /* __BGPSTREAM_FORMAT_H */
