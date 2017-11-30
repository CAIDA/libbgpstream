/*
 * Copyright (C) 2014 The Regents of the University of California.
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
