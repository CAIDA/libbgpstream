/*
 * Copyright (C) 2017 The Regents of the University of California.
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

#ifndef __BGPSTREAM_FORMAT_H
#define __BGPSTREAM_FORMAT_H

#include "bgpstream_filter.h"
#include "bgpstream_resource.h"

/** Generic interface to specific data format modules */
typedef struct bgpstream_format bgpstream_format_t;

/** Return codes for the populate_record method */
typedef enum {
  BGPSTREAM_FORMAT_OK,
  BGPSTREAM_FORMAT_CORRUPTED_MSG,
  BGPSTREAM_FORMAT_UNSUPPORTED_MSG,
  // all status codes below signal EOF
  BGPSTREAM_FORMAT_FILTERED_DUMP,
  BGPSTREAM_FORMAT_EMPTY_DUMP,
  BGPSTREAM_FORMAT_CANT_OPEN_DUMP,
  BGPSTREAM_FORMAT_CORRUPTED_DUMP,
  BGPSTREAM_FORMAT_END_OF_DUMP,
  BGPSTREAM_FORMAT_OUTSIDE_TIME_INTERVAL,
  BGPSTREAM_FORMAT_READ_ERROR,
  BGPSTREAM_FORMAT_UNKNOWN_ERROR,
} bgpstream_format_status_t;

/** Create a format handler for the given resource
 *
 * @param res           pointer to a resource
 * @param filter_mgr    pointer to filter manager to use for filtering records
 * @return pointer to a format module instance if successful, NULL otherwise
 */
bgpstream_format_t *bgpstream_format_create(bgpstream_resource_t *res,
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

/** Get the next elem from the given record
 *
 * @param format        pointer to the format object to use
 * @param record        pointer to the record to use
 * @param[out] elem     set to point to a borrowed elem structure or NULL if
 *                      there are no more elems
 * @return 1 if a valid elem was returned, 0 if there are no more elems, -1 if
 * an error occurred.
 */
int bgpstream_format_get_next_elem(bgpstream_format_t *format,
                                   bgpstream_record_t *record,
                                   bgpstream_elem_t **elem);

/** Initialize/create the format data in a given record
 *
 * @param record        pointer to the record to init data for
 * @return 0 if the data was initialized successfully, -1 otherwise
 */
int bgpstream_format_init_data(bgpstream_record_t *record);

/** Clear the format data in a given record
 *
 * @param record        pointer to the record to clear data for
 */
void bgpstream_format_clear_data(bgpstream_record_t *record);

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
