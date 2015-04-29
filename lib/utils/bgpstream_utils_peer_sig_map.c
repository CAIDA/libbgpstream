/*
 * This file is part of bgpstream
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#include <assert.h>
#include <stdio.h>

#include "utils.h"

#include "bgpstream_utils_peer_sig_map_int.h"

/* PRIVATE FUNCTIONS (static) */

static void sig_free(bgpstream_peer_sig_t *sig)
{
  free(sig);
}

static bgpstream_peer_id_t bgpstream_peer_sig_map_set_and_get_ps(
                                                  bgpstream_peer_sig_map_t *map,
						  bgpstream_peer_sig_t *ps)
{
  khiter_t k;
  int khret;
  bgpstream_peer_id_t next_id = kh_size(map->id_ps) + 1;
  if((k = kh_get(bgpstream_peer_sig_id_map, map->ps_id, ps)) ==
     kh_end(map->ps_id))
    {
      k = kh_put(bgpstream_peer_sig_id_map, map->ps_id, ps, &khret);
      kh_value(map->ps_id,k) = next_id;
      k = kh_put(bgpstream_peer_id_sig_map, map->id_ps, next_id, &khret);
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


/* PROTECTED FUNCTIONS (_int.h) */


khint64_t bgpstream_peer_sig_hash(bgpstream_peer_sig_t *ps)
{
  /* assuming that the number of peers that have the same ip and belong to two
   * different collectors is low (in this specific case there will be a
   * collision in terms of hash). */
  return bgpstream_addr_storage_hash(&ps->peer_ip_addr);
}

/** @note we do not need to take into account the peer AS number
 *  to check whether a peer differs or not */
int bgpstream_peer_sig_equal(bgpstream_peer_sig_t *ps1,
                             bgpstream_peer_sig_t *ps2)
{
  return (bgpstream_addr_storage_equal(&ps1->peer_ip_addr, &ps2->peer_ip_addr) &&
	  (strcmp(ps1->collector_str,ps2->collector_str) == 0));
}

int bgpstream_peer_sig_map_set(bgpstream_peer_sig_map_t *map,
                               bgpstream_peer_id_t peerid,
                               char *collector_str,
                               bgpstream_addr_storage_t *peer_ip_addr,
                               uint32_t peer_asnumber)
{
  khiter_t k;
  int khret;
  bgpstream_peer_sig_t ps;
  bgpstream_peer_sig_t *new_ps;
  ps.peer_ip_addr = *peer_ip_addr;
  strcpy(ps.collector_str, collector_str);
  ps.peer_asnumber = peer_asnumber;

  /* check if this peer id is in the map already */
  if((k = kh_get(bgpstream_peer_id_sig_map, map->id_ps, peerid)) !=
     kh_end(map->id_ps))
    {
      /* peer id exists */
      /* check that the signature is the same */
      if(bgpstream_peer_sig_equal(&ps, kh_val(map->id_ps, k)) != 0)
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
  if((k = kh_get(bgpstream_peer_sig_id_map, map->ps_id, &ps)) !=
     kh_end(map->ps_id))
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
	  /* this signature exists, but it has a different id.. this is a
             problem */
	  return -1;
	}
    }

  /* finally, now we can add it to the map */
  if((new_ps = malloc(sizeof(bgpstream_peer_sig_t))) == NULL)
    {
      return -1;
    }
  memcpy(new_ps, &ps, sizeof(bgpstream_peer_sig_t));
  k = kh_put(bgpstream_peer_sig_id_map, map->ps_id, new_ps, &khret);
  kh_value(map->ps_id,k) = peerid;
  k = kh_put(bgpstream_peer_id_sig_map, map->id_ps, peerid, &khret);
  kh_value(map->id_ps,k) = new_ps;

  return 0;
}

/* PUBLIC FUNCTIONS */

bgpstream_peer_sig_map_t *bgpstream_peer_sig_map_create()
{
  bgpstream_peer_sig_map_t *map = NULL;
  if((map =
      (bgpstream_peer_sig_map_t *)malloc_zero(sizeof(bgpstream_peer_sig_map_t)))
     == NULL)
    {
      return NULL;
    }

  if((map->ps_id = kh_init(bgpstream_peer_sig_id_map)) == NULL)
    {
      goto err;
    }

  if((map->id_ps = kh_init(bgpstream_peer_id_sig_map)) == NULL)
    {
      goto err;
    }

  return map;

 err:
  bgpstream_peer_sig_map_destroy(map);
  return NULL;
}

bgpstream_peer_id_t bgpstream_peer_sig_map_get_id(bgpstream_peer_sig_map_t *map,
                                                  char *collector_str,
                                                  bgpstream_addr_storage_t *peer_ip_addr,
                                                  uint32_t peer_asnumber)
{
  bgpstream_peer_sig_t *new_ps;
  if((new_ps = malloc(sizeof(bgpstream_peer_sig_t))) == NULL)
    {
      return -1;
    }

  new_ps->peer_ip_addr = *peer_ip_addr;
  strcpy(new_ps->collector_str, collector_str);
  new_ps->peer_asnumber = peer_asnumber;

  return bgpstream_peer_sig_map_set_and_get_ps(map,new_ps);
}

bgpstream_peer_sig_t *bgpstream_peer_sig_map_get_sig(
                                                  bgpstream_peer_sig_map_t *map,
						  bgpstream_peer_id_t id)
{
  bgpstream_peer_sig_t *ps = NULL;
  khiter_t k;
  if((k = kh_get(bgpstream_peer_id_sig_map, map->id_ps, id)) !=
     kh_end(map->id_ps))
    {
      ps = kh_value(map->id_ps,k);
    }
  return ps;
}

int bgpstream_peer_sig_map_get_size(bgpstream_peer_sig_map_t *map)
{
  assert(kh_size(map->id_ps) == kh_size(map->ps_id));
  return kh_size(map->id_ps);
}

void bgpstream_peer_sig_map_destroy(bgpstream_peer_sig_map_t *map)
{
  if(map != NULL)
    {
      if(map->ps_id != NULL)
	{
	  kh_destroy(bgpstream_peer_sig_id_map, map->ps_id);
	  map->ps_id = NULL;
	}
      if(map->id_ps != NULL)
	{
	  /* only call free vals on ONE map, they are shared */
	  kh_free_vals(bgpstream_peer_id_sig_map, map->id_ps, sig_free);
	  kh_destroy(bgpstream_peer_id_sig_map, map->id_ps);
	  map->id_ps = NULL;
	}
      free(map);
    }
}

void bgpstream_peer_sig_map_clear(bgpstream_peer_sig_map_t *map)
{
  /* only call free vals on ONE map, they are shared */
  kh_free_vals(bgpstream_peer_id_sig_map, map->id_ps, sig_free);
  kh_clear(bgpstream_peer_id_sig_map, map->id_ps);
  kh_clear(bgpstream_peer_sig_id_map, map->ps_id);
}
