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

#ifndef __BGPSTREAM_RESOURCE_MGR_H
#define __BGPSTREAM_RESOURCE_MGR_H

#include "bgpstream_filter.h"
#include "bgpstream_format.h"
#include "bgpstream_record.h"
#include "bgpstream_resource.h"
#include "bgpstream_transport.h"
#include <stdint.h>

/** Opaque pointer representing a resource manager */
typedef struct bgpstream_resource_mgr bgpstream_resource_mgr_t;

/** Create a new resource queue */
bgpstream_resource_mgr_t *
bgpstream_resource_mgr_create(bgpstream_filter_mgr_t *filter_mgr);

/** Destroy the given resource queue */
void bgpstream_resource_mgr_destroy(bgpstream_resource_mgr_t *q);

/** Add a resource item to the queue
 *
 * @param q               pointer to the queue
 * @param transport_type  transport protocol type
 * @param format_type     format type structure
 * @param uri             borrowed pointer to a URI string
 * @param initial_time    time of the first record in the resource
 * @param duration        duration of data in the resource
 * @param project         borrowed pointer to a project name string
 * @param collector       borrowed pointer to a collector name string
 * @param record_type     type of records provided by resource
 * @param[out] res        set to a pointer to the created resource
 * @return 1 if the item was added to the queue, 0 if it was filtered out, and
 * -1 if an error occurred
 */
int bgpstream_resource_mgr_push(
  bgpstream_resource_mgr_t *q,
  bgpstream_resource_transport_type_t transport_type,
  bgpstream_resource_format_type_t format_type, const char *uri,
  uint32_t initial_time, uint32_t duration, const char *project,
  const char *collector, bgpstream_record_type_t record_type,
  bgpstream_resource_t **res);

/** Check if the resource manager queue contains any resources
 *
 * @param q             pointer to the queue
 * @return 1 if the queue is empty, 0 otherwise
 */
int bgpstream_resource_mgr_empty(bgpstream_resource_mgr_t *q);

/** Get the next record from the stream
 *
 * @param q             pointer to the queue
 * @param[out] record   set to a borrowed pointer to a record if the return
 *                      code is >0
 * @return >0 if a record was read successfully, 0 if end-of-stream has been
 * reached, <0 if an error occurred.
 *
 */
int bgpstream_resource_mgr_get_record(bgpstream_resource_mgr_t *q,
                                      bgpstream_record_t **record);

#endif /* __BGPSTREAM_RESOURCE_MGR_H */
