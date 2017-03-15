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

#ifndef _BGPSTREAM_READER_MGR_H
#define _BGPSTREAM_READER_MGR_H

#include "bgpstream_filter.h"
#include "bgpstream_record.h"
#include "bgpstream_resource.h"

/** Opaque structure representting a reader manager instance */
typedef struct bgpstream_reader_mgr bgpstream_reader_mgr_t;

/** Create a new Reader Manager instance
 *
 * @param filter_mgr    pointer to a filter manager instance
 * @return pointer to a manager instance if successful, NULL otherwise
 */
bgpstream_reader_mgr_t *
bgpstream_reader_mgr_create(bgpstream_filter_mgr_t *filter_mgr);

/** Check if the given Reader Manager has an empty queue
 *
 * @param reader_mgr    pointer to a reader manager instance
 * @return 1 if the queue is empty, 0 otherwise
 */
int bgpstream_reader_mgr_is_empty(bgpstream_reader_mgr_t *reader_mgr);

/** Add the given resource batch to the reader manager
 *
 * @param reader_mgr    pointer to a reader manager instance
 * @param res_batch     pointer to an array of resource pointers
 * @param res_batch_cnt number of resource pointers in the res_batch array
 * @return 0 if the batch was added successfully, -1 otherwise
 */
int bgpstream_reader_mgr_add(bgpstream_reader_mgr_t *reader_mgr,
                             bgpstream_resource_t **res_batch,
                             int res_batch_cnt);

/** Extract the next record from the reader manager
 *
 * @param reader_mgr    pointer to a reader manager instance
 * @param record        pointer to a record instance to populate
 * @return 1 if the record was successfully populated, 0 if there are no more
 * records to be read, and -1 if an error occurred.
 */
int bgpstream_reader_mgr_get_next_record(bgpstream_reader_mgr_t *reader_mgr,
                                         bgpstream_record_t *record);

/** Destroy the given reader manager
 *
 * @param reader_mgr    pointer to a reader manager instance
 */
void bgpstream_reader_mgr_destroy(bgpstream_reader_mgr_t *reader_mgr);

#endif /* _BGPSTREAM_READER_MGR_H */
