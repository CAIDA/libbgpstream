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

typedef enum {BL_PEERSTATE_UNKNOWN     = 0,
	      BL_PEERSTATE_IDLE        = 1,
	      BL_PEERSTATE_CONNECT     = 2,
	      BL_PEERSTATE_ACTIVE      = 3,
	      BL_PEERSTATE_OPENSENT    = 4,
	      BL_PEERSTATE_OPENCONFIRM = 5,
	      BL_PEERSTATE_ESTABLISHED = 6, 
	      BL_PEERSTATE_NULL        = 7 
} bl_peerstate_type_t;

#define BL_PEERSTATE_TYPE_MAX 8

typedef enum {BL_UNKNOWN_ELEM      = 0,
	      BL_RIB_ELEM          = 1,
	      BL_ANNOUNCEMENT_ELEM = 2,
	      BL_WITHDRAWAL_ELEM   = 3,
	      BL_PEERSTATE_ELEM    = 4
} bl_elem_type_t;

#define BL_ELEM_TYPE_MAX 5

typedef struct struct_bl_elem_t {

  /** type of bgp elem */
  bl_elem_type_t type;
  /** epoch time that refers to when this
   *  elem was generated on the peer */
  uint32_t timestamp;
  /** peer IP address */
  bl_addr_storage_t peer_address;
  /** peer AS number */
  uint32_t peer_asnumber;

  /** type-dependent fields */
  /** IP prefix */
  bl_pfx_storage_t prefix;
  /** next hop */
  bl_addr_storage_t nexthop;
  /** AS path */
  bl_aspath_storage_t aspath;
  /** old state of the peer */
  bl_peerstate_type_t old_state;
  /** new state of the peer */
  bl_peerstate_type_t new_state;

  /** a pointer in case we want to keep
   *  elems in a queue*/
  struct struct_bl_elem_t *next;
} bl_elem_t;

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

char *bl_print_elemtype(bl_elem_type_t type);

char *bl_print_peerstate(bl_peerstate_type_t state);

char *bl_print_elem(bl_elem_t *elem);

#endif /* __BGPSTREAM_ELEM_H */
