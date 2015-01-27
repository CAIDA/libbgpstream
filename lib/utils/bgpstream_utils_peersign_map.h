/*
 * bgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpstream.
 *
 * bgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef _BL_PEERSIGN_MAP_H
#define _BL_PEERSIGN_MAP_H

#include "bgpstream_utils.h"

/** Type of a peer ID */
typedef uint16_t bl_peerid_t;

/** Each peer is uniquely identified by its 
 *  signature, i.e. the pair:
 *      <collector, peer ip address>
 */
typedef struct struct_peer_signature_t {
  char collector_str[BGPCOMMON_COLLECTOR_NAME_LEN];
  bl_addr_storage_t peer_ip_addr;
} bl_peer_signature_t;


typedef struct bl_peersign_map bl_peersign_map_t;


bl_peersign_map_t *bl_peersign_map_create();


bl_peerid_t bl_peersign_map_set_and_get(bl_peersign_map_t *map,
					char *collector_str,
					bl_addr_storage_t *peer_ip_addr);

/** Set the peer ID for the given collector/peer
 *
 * @param map           peersign map to set peer ID for
 * @param peerid        peer id to set
 * @param collector_str name of the collector the peer is associated with
 * @param peer_ip_addr  pointer to the peer IP address
 * @return 0 if the ID was associated successfully, -1 otherwise
 */
int bl_peersign_map_set(bl_peersign_map_t *map,
			bl_peerid_t peerid,
			char *collector_str,
			bl_addr_storage_t *peer_ip_addr);

bl_peer_signature_t* bl_peersign_map_get_peersign(bl_peersign_map_t *map,
						  bl_peerid_t id);

int bl_peersign_map_get_size(bl_peersign_map_t *map);

void bl_peersign_map_destroy(bl_peersign_map_t *map);

/** Empties the given peersign map
 *
 * @param map           peersign map
 */
void bl_peersign_map_clear(bl_peersign_map_t *map);



#endif /* _PEERSIGN_MAP_H */

