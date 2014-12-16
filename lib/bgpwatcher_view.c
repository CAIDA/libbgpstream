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
  return kh_init(bwv_peerid_pfxinfo);
}

static int peerid_pfxinfo_insert(bwv_peerid_pfxinfo_t *peerid_pfxinfo,
                                 bl_peerid_t peerid,
                                 bgpwatcher_pfx_peer_info_t *pfx_info)
{
  khiter_t k;
  int khret;
  if((k = kh_get(bwv_peerid_pfxinfo, peerid_pfxinfo, peerid))
     == kh_end(peerid_pfxinfo))
    {
      k = kh_put(bwv_peerid_pfxinfo, peerid_pfxinfo, peerid, &khret);
    }
  kh_value(peerid_pfxinfo, k) = *pfx_info;
  return 0;
}

static void peerid_pfxinfo_destroy(bwv_peerid_pfxinfo_t *pv)
{
  if(pv != NULL)
    {
      kh_destroy(bwv_peerid_pfxinfo, pv);
    }
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
      peerids_pfxinfo =  kh_value(view->v6pfxs, k);
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

int bgpwatcher_view_add_prefix(bgpwatcher_view_t *view,
                               bl_pfx_storage_t *prefix,
                               bl_peerid_t peerid,
                               bgpwatcher_pfx_peer_info_t *pfx_info)
{
  bwv_peerid_pfxinfo_t *peerids_pfxinfo;

  if((peerids_pfxinfo = get_pfx_peerids(view, prefix)) == NULL)
    {
      fprintf(stderr, "Unknown prefix provided!\n");
      return -1;
    }

  if(peerid_pfxinfo_insert(peerids_pfxinfo, peerid, pfx_info) < 0)
    {
      return -1;
    }

  return 0;
}

int bgpwatcher_view_send(void *dest, bgpwatcher_view_t *view)
{
  uint32_t u32;
  /* send the time */
  u32 = htonl(view->time);
  if(zmq_send(dest, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      goto err;
    }

  /* @todo replace with actual fields (FIX SNDMORE ABOVE) */
  fprintf(stderr, "DEBUG: Sending dummy view...\n");

  return 0;

 err:
  return -1;
}

bgpwatcher_view_t *bgpwatcher_view_recv(void *src)
{
  bgpwatcher_view_t *view;
  uint32_t u32;

  /* create a new independent view (no external peers table) */
  if((view = bgpwatcher_view_create(NULL)) == NULL)
    {
      goto err;
    }

  /* recv the time */
  if(zmq_recv(src, &u32, sizeof(u32), 0) != sizeof(u32))
    {
      goto err;
    }
  view->time = ntohl(u32);

  /* @todo replace with actual fields */
  fprintf(stderr, "DEBUG: Receiving dummy view...\n");

  return view;

 err:
  bgpwatcher_view_destroy(view);
  return NULL;
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

void bgpwatcher_view_dump(bgpwatcher_view_t *view)
{
      fprintf(stdout,
	      "Time:\t%"PRIu32"\n"
	      "------------------------------\n",
              view->time);
}
