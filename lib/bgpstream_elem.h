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

#ifndef __BGPSTREAM_ELEM_H
#define __BGPSTREAM_ELEM_H

#include "bgpstream_record.h"
#include "bl_bgp_utils.h"

/* extract a list of elements from the bgpstream record  */
bl_elem_t *bgpstream_elem_queue_create(bgpstream_record_t *bs_record);

/* destroy the queue */
void bgpstream_elem_queue_destroy(bl_elem_t *elem_queue);

#endif /* __BGPSTREAM_ELEM_H */
