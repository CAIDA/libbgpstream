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

#include <bgpstream_record.h>
#include <bl_bgp_utils.h>

/** @file
 *
 * @brief Header file that exposes the public interface of a bgpstream elem. For
 * details about an elem structure, see bl_bgp_utils.h.
 *
 * @author Chiara Orsini
 *
 */

/** Extract a list of elements from the given BGP Stream Record
 *
 * @param record        pointer to a BGP Stream Record instance
 * @return pointer to a linked-list of bl_elem_t objects
 *
 * @note the returned elem list must be destroyed using
 * bgpstream_elem_queue_destroy
 */
bl_elem_t *bgpstream_elem_queue_create(bgpstream_record_t *record);

/** Destroy the given linked-list of Elem instances
 *
 * @param elem_queue    pointer to a linked-list of elems
 */
void bgpstream_elem_queue_destroy(bl_elem_t *elem_queue);

#endif /* __BGPSTREAM_ELEM_H */
