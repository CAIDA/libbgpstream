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

#include <assert.h>
#include <stdio.h>

#include "utils.h"

#include "bgpstream_utils_peersign_map.h"
#include "bgpstream_utils_peersign_map_int.h"

khint64_t bl_peer_signature_hash_func(bl_peer_signature_t *ps)
{
  /* assuming that the number of peers that have the same ip
   * and belong to two different collectors is low
   * (in this specific case there will be a collision in terms
   * of hash). */
  return bgpstream_addr_storage_hash(&ps->peer_ip_addr);
  // the following hash is slower but would decrease the collision chance
  /* assert(strlen(ps.collector_str) > 2); */
  /* uint16_t *last_chars =(uint16_t *) &(ps.collector_str[strlen(ps.collector_str)-2]); */
  /* return bgpstream_addr_storage_hash_func(ps.peer_ip_addr) & (uint64_t) (*last_chars); */
}

int bl_peer_signature_hash_equal(bl_peer_signature_t *ps1,bl_peer_signature_t *ps2)
{
  return (bgpstream_addr_storage_equal(&ps1->peer_ip_addr, &ps2->peer_ip_addr) &&
	  (strcmp(ps1->collector_str,ps2->collector_str) == 0));
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

int bl_peersign_map_set(bl_peersign_map_t *map,
			bl_peerid_t peerid,
			char *collector_str,
			bgpstream_addr_storage_t *peer_ip_addr)
{
  khiter_t k;
  int khret;
  bl_peer_signature_t ps;
  bl_peer_signature_t *new_ps;
  ps.peer_ip_addr = *peer_ip_addr;
  strcpy(ps.collector_str, collector_str);

  /* check if this peer id is in the map already */
  if((k = kh_get(bl_bsid_peersign_map, map->id_ps, peerid)) != kh_end(map->id_ps))
    {
      /* peer id exists */
      /* check that the signature is the same */
      if(bl_peer_signature_hash_equal(&ps, kh_val(map->id_ps, k)) != 0)
	{
	  /* it was already here... */
	  return 0;
	}
      else
	{
	  /* another signature has this same id.. this is a problem */
	  return -1;
	}
    }

  /* check if this signature exists already */
  if((k = kh_get(bl_peersign_bsid_map, map->ps_id, &ps)) != kh_end(map->ps_id))
    {
      /* signature exists */

      /* check that the peerid is the same */
      if(peerid == kh_val(map->ps_id, k))
	{
	  /* it was already here.. */
	  return 0;
	}
      else
	{
	  /* this signature exists, but it has a different id.. this is a problem */
	  return -1;
	}
    }

  /* finally, now we can add it to the map */
  if((new_ps = malloc(sizeof(bl_peer_signature_t))) == NULL)
    {
      return -1;
    }
  memcpy(new_ps, &ps, sizeof(bl_peer_signature_t));
  k = kh_put(bl_peersign_bsid_map, map->ps_id, new_ps, &khret);
  kh_value(map->ps_id,k) = peerid;
  k = kh_put(bl_bsid_peersign_map, map->id_ps, peerid, &khret);
  kh_value(map->id_ps,k) = new_ps;

  return 0;
}

static bl_peerid_t bl_peersign_map_set_and_get_ps(bl_peersign_map_t *map,
						  bl_peer_signature_t *ps)
{
  khiter_t k;
  int khret;
  bl_peerid_t next_id = kh_size(map->id_ps) + 1;
  if((k = kh_get(bl_peersign_bsid_map, map->ps_id, ps)) == kh_end(map->ps_id))
    {
      k = kh_put(bl_peersign_bsid_map, map->ps_id, ps, &khret);
      kh_value(map->ps_id,k) = next_id;
      k = kh_put(bl_bsid_peersign_map, map->id_ps, next_id, &khret);
      kh_value(map->id_ps,k) = ps;

      return next_id;
    }
  else {
    /* already exists... */
    free(ps); /* it was mallocd for us...*/
    return kh_value(map->ps_id,k);
  }
  return 0;
}

bl_peerid_t bl_peersign_map_set_and_get(bl_peersign_map_t *map,
					char *collector_str,
					bgpstream_addr_storage_t *peer_ip_addr)
{
  bl_peer_signature_t *new_ps;
  if((new_ps = malloc(sizeof(bl_peer_signature_t))) == NULL)
    {
      return -1;
    }

  new_ps->peer_ip_addr = *peer_ip_addr;
  strcpy(new_ps->collector_str, collector_str);

  return bl_peersign_map_set_and_get_ps(map,new_ps);
}


bl_peer_signature_t* bl_peersign_map_get_peersign(bl_peersign_map_t *map,
						  bl_peerid_t id)
{
  bl_peer_signature_t *ps = NULL;
  khiter_t k;
  if((k = kh_get(bl_bsid_peersign_map, map->id_ps, id)) != kh_end(map->id_ps))
    {
      ps = kh_value(map->id_ps,k);
    }
  return ps;
}

int bl_peersign_map_get_size(bl_peersign_map_t *map)
{
  return kh_size(map->id_ps);
}

static void sig_free(bl_peer_signature_t *s)
{
  free(s);
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
	  /* only call free vals on ONE map, they are shared */
	  kh_free_vals(bl_bsid_peersign_map, map->id_ps, sig_free);
	  kh_destroy(bl_bsid_peersign_map, map->id_ps);
	  map->id_ps = NULL;
	}
      free(map);
    }
}

void bl_peersign_map_clear(bl_peersign_map_t *map)
{
  /* only call free vals on ONE map, they are shared */
  kh_free_vals(bl_bsid_peersign_map, map->id_ps, sig_free);
  kh_clear(bl_bsid_peersign_map, map->id_ps);
  kh_clear(bl_peersign_bsid_map, map->ps_id);
}
