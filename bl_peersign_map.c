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

#include "bl_peersign_map.h"
#include "bl_peersign_map_int.h"

#include <stdio.h>
#include "utils.h"
#include <assert.h>


khint64_t bl_peer_signature_hash_func(bl_peer_signature_t ps)
{
  /* assuming that the number of peers that have the same ip
   * and belong to two different collectors is low
   * (in this specific case there will be a collision in terms
   * of hash. */
  return bl_addr_storage_hash_func(ps.peer_ip_addr);
}

int bl_peer_signature_hash_equal(bl_peer_signature_t ps1,bl_peer_signature_t ps2)
{
  return (bl_addr_storage_hash_equal(ps1.peer_ip_addr, ps2.peer_ip_addr) &&
	  (strcmp(ps1.collector_str,ps2.collector_str) == 0));    
}


bl_peersign_map_t *bl_peersign_map_create()
{
  bl_peersign_map_t *map = NULL;
  if( (map = (bl_peersign_map_t *)malloc_zero(sizeof(bl_peersign_map_t))) != NULL)
    {
      if( (map->ps_id =kh_init(bl_peersign_bsid_map)) != NULL)
	{
	  if( (map->id_ps =kh_init(bl_bsid_peersign_map)) != NULL)
	    {
	      return map;
	    }
	}
    }
  bl_peersign_map_destroy(map);
  return NULL;
}

static uint16_t bl_peersign_map_set_and_get_ps(bl_peersign_map_t *map, bl_peer_signature_t ps)
{
  khiter_t k;
  int khret;
  uint16_t next_id = kh_size(map->id_ps) + 1;
  if((k = kh_get(bl_peersign_bsid_map, map->ps_id, ps)) == kh_end(map->ps_id))
    {
      k = kh_put(bl_peersign_bsid_map, map->ps_id, ps, &khret);      
      kh_value(map->ps_id,k) = next_id;
      k = kh_put(bl_bsid_peersign_map, map->id_ps, next_id, &khret);      
      kh_value(map->id_ps,k) = ps;
      return next_id;
    }
  else {
    return kh_value(map->ps_id,k);
  }
  return 0;
}


uint16_t bl_peersign_map_set_and_get(bl_peersign_map_t *map, char *collector_str, bl_addr_storage_t *peer_ip_addr)
{
  bl_peer_signature_t ps;
  ps.peer_ip_addr = *peer_ip_addr;
  strcpy(ps.collector_str, collector_str);
  return bl_peersign_map_set_and_get_ps(map,ps);
}


bl_peer_signature_t* bl_peersign_map_get_peersign(bl_peersign_map_t *map,
						       uint16_t id)
{
  bl_peer_signature_t *ps = NULL;
  khiter_t k;
  if((k = kh_get(bl_bsid_peersign_map, map->id_ps, id)) != kh_end(map->id_ps))
    {
      ps = &(kh_value(map->id_ps,k));
    }
  return ps;
}


void bl_peersign_map_destroy(bl_peersign_map_t *map)
{
  if(map != NULL)
    {
      if(map->ps_id != NULL)
	{
	  kh_destroy(bl_peersign_bsid_map, map->ps_id);
	  map->ps_id = NULL;
	}
      if(map->id_ps != NULL)
	{
	  kh_destroy(bl_bsid_peersign_map, map->id_ps);
	  map->id_ps = NULL;
	}
      free(map);
    }
}


