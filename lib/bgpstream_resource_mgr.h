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

#ifndef __BGPSTREAM_RESOURCE_MGR_H
#define __BGPSTREAM_RESOURCE_MGR_H

#include <stdint.h>
#include "bgpstream_record.h"
#include "bgpstream_transport.h"
#include "bgpstream_format.h"
#include "bgpstream_resource.h"

/** Opaque pointer representing a resource manager */
typedef struct bgpstream_resource_mgr bgpstream_resource_mgr_t;

/** Create a new resource queue */
bgpstream_resource_mgr_t *
bgpstream_resource_mgr_create();

/** Destroy the given resource queue */
void
bgpstream_resource_mgr_destroy(bgpstream_resource_mgr_t *q);

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
 * @return if successful, a borrowed pointer to the added resource object to
 * allow addition of attributes, NULL if the push fails.
 */
bgpstream_resource_t *
bgpstream_resource_mgr_push(bgpstream_resource_mgr_t *q,
                            bgpstream_transport_type_t transport_type,
                            bgpstream_format_type_t format_type,
                            const char *uri,
                            uint32_t initial_time,
                            uint32_t duration,
                            const char *project,
                            const char *collector,
                            bgpstream_record_dump_type_t record_type);

/** Check if the resource manager queue contains any resources
 *
 * @param q             pointer to the queue
 * @return 1 if the queue is empty, 0 otherwise
 */
int
bgpstream_resource_mgr_empty(bgpstream_resource_mgr_t *q);

/** Get the next record from the stream
 *
 * @param q             pointer to the queue
 * @param record        pointer to a record to fill
 * @return >0 if a record was read successfully, 0 if end-of-stream has been
 * reached, <0 if an error occurred.
 *
 */
int
bgpstream_resource_mgr_get_record(bgpstream_resource_mgr_t *q,
                                  bgpstream_record_t *record);


#endif /* __BGPSTREAM_RESOURCE_MGR_H */
