/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>

#include <czmq.h>

#include "bgpwatcher_view_int.h"

/* ========== PRIVATE FUNCTIONS ========== */

static bwv_peerid_pfxinfo_t* peerid_pfxinfo_create()
{
  bwv_peerid_pfxinfo_t *v;

  if((v = malloc(sizeof(bwv_peerid_pfxinfo_t))) == NULL)
    {
      return NULL;
    }

  if((v->peers = kh_init(bwv_peerid_pfxinfo)) == NULL)
    {
      free(v);
      return NULL;
    }

  v->peers_cnt = 0;

  return v;
}

static int peerid_pfxinfo_insert(bwv_peerid_pfxinfo_t *v,
                                 bl_peerid_t peerid,
                                 bgpwatcher_pfx_peer_info_t *pfx_info)
{
  khiter_t k;
  int khret;

  /* if we are the first to insert a peer for this prefix after it was cleared,
     we are also responsible for clearing all the peer info */
  if(v->peers_cnt == 0)
    {
      for (k = kh_begin(v->peers); k != kh_end(v->peers); ++k)
	{
	  if (kh_exist(v->peers, k))
	    {
	      kh_value(v->peers, k).in_use = 0;
	    }
	}
    }

  if((k = kh_get(bwv_peerid_pfxinfo, v->peers, peerid)) == kh_end(v->peers))
    {
      k = kh_put(bwv_peerid_pfxinfo,v->peers, peerid, &khret);

      /* we need to at least mark this info as unused */
      kh_value(v->peers, k).in_use = 0;
    }

  /* if this peer was not previously used, we need to count it */
  if(kh_value(v->peers, k).in_use == 0)
    {
      v->peers_cnt++;
    }

  kh_value(v->peers, k) = *pfx_info;
  kh_value(v->peers, k).in_use = 1;
  return 0;
}

static void peerid_pfxinfo_destroy(bwv_peerid_pfxinfo_t *v)
{
  if(v == NULL)
    {
      return;
    }

  kh_destroy(bwv_peerid_pfxinfo, v->peers);
  v->peers_cnt = 0;
  free(v);
}

/** @todo consider making these macros? */
static bwv_peerid_pfxinfo_t *get_v4pfx_peerids(bgpwatcher_view_t *view,
                                               bl_ipv4_pfx_t *v4pfx)
{
  bwv_peerid_pfxinfo_t *peerids_pfxinfo;
  khiter_t k;
  int khret;

  if((k = kh_get(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs, *v4pfx))
     == kh_end(view->v4pfxs))
    {
      if((peerids_pfxinfo = peerid_pfxinfo_create()) == NULL)
	{
	  return NULL;
	}
      k = kh_put(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs, *v4pfx, &khret);
      kh_value(view->v4pfxs, k) = peerids_pfxinfo;
    }
  else
    {
      peerids_pfxinfo =  kh_value(view->v4pfxs, k);
    }

  if(peerids_pfxinfo->peers_cnt == 0)
    {
      view->v4pfxs_cnt++;
    }

  return peerids_pfxinfo;
}

static bwv_peerid_pfxinfo_t *get_v6pfx_peerids(bgpwatcher_view_t *view,
                                               bl_ipv6_pfx_t *v6pfx)
{
  bwv_peerid_pfxinfo_t *peerids_pfxinfo;
  khiter_t k;
  int khret;

  if((k = kh_get(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs, *v6pfx))
     == kh_end(view->v6pfxs))
    {
      if((peerids_pfxinfo = peerid_pfxinfo_create()) == NULL)
	{
	  return NULL;
	}
      k = kh_put(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs, *v6pfx, &khret);
      kh_value(view->v6pfxs, k) = peerids_pfxinfo;
    }
  else
    {
      peerids_pfxinfo = kh_value(view->v6pfxs, k);
    }

  if(peerids_pfxinfo->peers_cnt == 0)
    {
      view->v6pfxs_cnt++;
    }

  return peerids_pfxinfo;
}

static bwv_peerid_pfxinfo_t *get_pfx_peerids(bgpwatcher_view_t *view,
                                             bl_pfx_storage_t *prefix)
{
  if(prefix->address.version == BL_ADDR_IPV4)
    {
      return get_v4pfx_peerids(view, bl_pfx_storage2ipv4(prefix));
    }
  else if(prefix->address.version == BL_ADDR_IPV6)
    {
      return get_v6pfx_peerids(view, bl_pfx_storage2ipv6(prefix));
    }

  return NULL;
}

/* ========== PROTECTED FUNCTIONS ========== */

void bgpwatcher_view_clear(bgpwatcher_view_t *view)
{
  khiter_t k;

  view->time = 0;

  /* mark all ipv4 prefixes as unused */
  for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); ++k)
    {
      if(kh_exist(view->v4pfxs, k))
	{
	  kh_value(view->v4pfxs, k)->peers_cnt = 0;
	}
    }
  view->v4pfxs_cnt = 0;

  /* mark all ipv4 prefixes as unused */
  for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); ++k)
    {
      if(kh_exist(view->v6pfxs, k))
	{
	  kh_value(view->v6pfxs, k)->peers_cnt = 0;
	}
    }
  view->v4pfxs_cnt = 0;
}

int bgpwatcher_view_add_prefix(bgpwatcher_view_t *view,
                               bl_pfx_storage_t *prefix,
                               bl_peerid_t peerid,
                               bgpwatcher_pfx_peer_info_t *pfx_info,
			       void **cache)
{
  bwv_peerid_pfxinfo_t *peerids_pfxinfo;

  if(cache == NULL || *cache == NULL)
    {
      if((peerids_pfxinfo = get_pfx_peerids(view, prefix)) == NULL)
	{
	  fprintf(stderr, "Unknown prefix provided!\n");
	  return -1;
	}
      if(cache != NULL)
	{
	  *cache = peerids_pfxinfo;
	}
    }
  else
    {
      peerids_pfxinfo = *cache;
    }

  if(peerid_pfxinfo_insert(peerids_pfxinfo, peerid, pfx_info) < 0)
    {
      return -1;
    }

  return 0;
}

/* ========== PUBLIC FUNCTIONS ========== */

bgpwatcher_view_t *bgpwatcher_view_create(bl_peersign_map_t *peersigns)
{
  bgpwatcher_view_t *view;

  if((view = malloc_zero(sizeof(bgpwatcher_view_t))) == NULL)
    {
      return NULL;
    }

  if((view->v4pfxs = kh_init(bwv_v4pfx_peerid_pfxinfo)) == NULL)
    {
      goto err;
    }

  if((view->v6pfxs = kh_init(bwv_v6pfx_peerid_pfxinfo)) == NULL)
    {
      goto err;
    }

  if(peersigns != NULL)
    {
      view->peersigns = peersigns;
      view->peersigns_shared = 1;
    }
  else if((view->peersigns = bl_peersign_map_create()) == NULL)
    {
      fprintf(stderr, "Failed to create peersigns table\n");
      goto err;
    }

  gettimeofday(&view->time_created, NULL);

  return view;

 err:
  fprintf(stderr, "Failed to create BGP Watcher View\n");
  bgpwatcher_view_destroy(view);
  return NULL;
}

void bgpwatcher_view_destroy(bgpwatcher_view_t *view)
{
  if(view == NULL)
    {
      return;
    }

  if(view->v4pfxs != NULL)
    {
      kh_free_vals(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs,
                   peerid_pfxinfo_destroy);
      kh_destroy(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs);
      view->v4pfxs = NULL;
    }

  if(view->v6pfxs != NULL)
    {
      kh_free_vals(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs,
                   peerid_pfxinfo_destroy);
      kh_destroy(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs);
      view->v6pfxs = NULL;
    }

  if(view->peersigns_shared == 0 && view->peersigns != NULL)
    {
      bl_peersign_map_destroy(view->peersigns);
      view->peersigns = NULL;
    }

  free(view);
}

/* ==================== SIMPLE ACCESSOR FUNCTIONS ==================== */

uint32_t bgpwatcher_view_v4pfx_size(bgpwatcher_view_t *view)
{
  return view->v4pfxs_cnt;
}

uint32_t bgpwatcher_view_v6pfx_size(bgpwatcher_view_t *view)
{
  return view->v6pfxs_cnt;
}

uint32_t bgpwatcher_view_pfx_size(bgpwatcher_view_t *view)
{
  return bgpwatcher_view_v4pfx_size(view) + bgpwatcher_view_v6pfx_size(view);
}

uint32_t bgpwatcher_view_peer_size(bgpwatcher_view_t *view)
{
  /** @todo fixme */
  return bl_peersign_map_get_size(view->peersigns);
}

uint32_t bgpwatcher_view_time(bgpwatcher_view_t *view)
{
  return view->time;
}
