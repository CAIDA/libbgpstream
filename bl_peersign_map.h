/*
 * bgp-common
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgp-common.
 *
 * bgp-common is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgp-common is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgp-common.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef _BL_PEERSIGN_MAP_H
#define _BL_PEERSIGN_MAP_H

#include "khash.h"
#include "bl_bgp_utils.h"


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


uint16_t bl_peersign_map_set_and_get(bl_peersign_map_t *map,
				     char *collector_str, bl_addr_storage_t *peer_ip_addr);

bl_peer_signature_t* bl_peersign_map_get_peersign(bl_peersign_map_t *map,
						  uint16_t id);

int bl_peersign_map_get_size(bl_peersign_map_t *map);

void bl_peersign_map_destroy(bl_peersign_map_t *map);



#endif /* _PEERSIGN_MAP_H */

