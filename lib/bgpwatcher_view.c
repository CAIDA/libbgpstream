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

/* we need to poke our fingers into the peersign map */
#include "bl_peersign_map_int.h"

#include "bgpwatcher_view_int.h"

struct bgpwatcher_view_iter {

  /** Pointer to the view instance we are iterating over */
  bgpwatcher_view_t *view;

  /** Current v4pfx */
  khiter_t v4pfx_it;

  /** Current v6pfx */
  khiter_t v6pfx_it;

  /** Current peersig */
  khiter_t peer_it;

  /** Current v4pfx_peer */
  khiter_t v4pfx_peer_it;
  /** Is the v4pfx_peer iterator valid? */
  int v4pfx_peer_it_valid;

  /** Current v6pfx_peer */
  khiter_t v6pfx_peer_it;
  /** Is the v6pfx_peer iterator valid? */
  int v6pfx_peer_it_valid;

};

/* ========== PRIVATE FUNCTIONS ========== */

static int peerinfo_add_pfx(bgpwatcher_view_t *view, bl_peerid_t peerid,
                            bl_pfx_storage_t *prefix)
{
  khiter_t k;
  int khret;
  /* for now, just increment the prefix count */

  /* this MUST only be called when first adding a peer to a prefix, otherwise we
     will double-count prefixes that are added twice */

  if((k = kh_get(bwv_peerid_peerinfo, view->peerinfo, peerid))
     == kh_end(view->peerinfo))
    {
      /* first prefix for this peer */
      k = kh_put(bwv_peerid_peerinfo, view->peerinfo, peerid, &khret);
      kh_val(view->peerinfo, k).id = peerid;
      kh_val(view->peerinfo, k).v4_pfx_cnt = 0;
      kh_val(view->peerinfo, k).v6_pfx_cnt = 0;
    }

  switch(prefix->address.version)
    {
    case BL_ADDR_IPV4:
      kh_val(view->peerinfo, k).v4_pfx_cnt++;
      break;

    case BL_ADDR_IPV6:
      kh_val(view->peerinfo, k).v6_pfx_cnt++;
      break;

    default:
      return -1;
      break;
    }

  return 0;
}

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
  v->user = NULL;

  return v;
}

static int peerid_pfxinfo_insert(bgpwatcher_view_t *view,
                                 bl_pfx_storage_t *prefix,
                                 bwv_peerid_pfxinfo_t *v,
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

      /* and we need to add this prefix to the peerinfo counter */
      peerinfo_add_pfx(view, peerid, prefix);
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
  view->v6pfxs_cnt = 0;

  /* clear out the peerinfo table */
  kh_clear(bwv_peerid_peerinfo, view->peerinfo);
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

  if(peerid_pfxinfo_insert(view, prefix, peerids_pfxinfo, peerid, pfx_info) < 0)
    {
      return -1;
    }

  return 0;
}

/* ========== PUBLIC FUNCTIONS ========== */

bgpwatcher_view_t *bgpwatcher_view_create_shared(bl_peersign_map_t *peersigns)
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
      view->peersigns_shared = 1;
      view->peersigns = peersigns;
    }
  else
    {
      if((view->peersigns = bl_peersign_map_create()) == NULL)
	{
	  fprintf(stderr, "Failed to create peersigns table\n");
	  goto err;
	}
      view->peersigns_shared = 0;
    }

  if((view->peerinfo = kh_init(bwv_peerid_peerinfo)) == NULL)
    {
      fprintf(stderr, "Failed to create peer info table\n");
      goto err;
    }

  gettimeofday(&view->time_created, NULL);

  return view;

 err:
  fprintf(stderr, "Failed to create BGP Watcher View\n");
  bgpwatcher_view_destroy(view);
  return NULL;
}

bgpwatcher_view_t *bgpwatcher_view_create()
{
  return bgpwatcher_view_create_shared(NULL);
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

  if(view->peerinfo != NULL)
    {
      kh_destroy(bwv_peerid_peerinfo, view->peerinfo);
      view->peerinfo = NULL;
    }

  free(view);
}


void bgpwatcher_view_destroy_user(bgpwatcher_view_t *view,
				  bgpwatcher_view_destroy_user_cb *call_back)
{
  khiter_t k;
  assert(call_back != NULL);
  for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); k++)
    {
      if(kh_exist(view->v4pfxs,k))
	{
	  call_back(kh_val(view->v4pfxs,k)->user);
	  kh_val(view->v4pfxs,k)->user = NULL;
	}
    }
  for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); k++)
    {
      if(kh_exist(view->v6pfxs,k))
	{
	  call_back(kh_val(view->v6pfxs,k)->user);
	  kh_val(view->v6pfxs,k)->user = NULL;
	}
    } 
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
  /* note that while the peersigns table may be shared and thus have many more
     peers in it than are in use in this table, the peerinfo table is cleared
     completely, so every peer it contains is in use */
  return kh_size(view->peerinfo);
}

uint32_t bgpwatcher_view_time(bgpwatcher_view_t *view)
{
  return view->time;
}

/* ==================== ITERATOR FUNCTIONS ==================== */

bgpwatcher_view_iter_t *bgpwatcher_view_iter_create(bgpwatcher_view_t *view)
{
  bgpwatcher_view_iter_t *iter;

  if((iter = malloc_zero(sizeof(bgpwatcher_view_iter_t))) == NULL)
    {
      return NULL;
    }

  iter->view = view;

  iter->v4pfx_it = kh_end(iter->view->v4pfxs);
  iter->v6pfx_it = kh_end(iter->view->v6pfxs);
  iter->peer_it  = kh_end(iter->view->peerinfo);

  iter->v4pfx_peer_it_valid = 0;
  iter->v6pfx_peer_it_valid = 0;

  return iter;
}

void bgpwatcher_view_iter_destroy(bgpwatcher_view_iter_t *iter)
{
  free(iter);
}

void bgpwatcher_view_iter_first(bgpwatcher_view_iter_t *iter,
				bgpwatcher_view_iter_field_t field)
{
  switch(field)
    {
    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX:
      iter->v4pfx_it = kh_begin(iter->view->v4pfxs);

      /* keep searching if this does not exist */
      while(iter->v4pfx_it != kh_end(iter->view->v4pfxs) &&
	    (!kh_exist(iter->view->v4pfxs, iter->v4pfx_it) ||
	     !kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_cnt))
	{
	  iter->v4pfx_it++;
	}
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX:
      iter->v6pfx_it = kh_begin(iter->view->v6pfxs);

      /* keep searching if this does not exist */
      while(iter->v6pfx_it != kh_end(iter->view->v6pfxs) &&
	    (!kh_exist(iter->view->v6pfxs, iter->v6pfx_it) ||
	     !kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_cnt))
	{
	  iter->v6pfx_it++;
	}
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_PEER:
      iter->peer_it = kh_begin(iter->view->peerinfo);

      /* keep searching if this does not exist */
      while(iter->peer_it != kh_end(iter->view->peerinfo) &&
	    !kh_exist(iter->view->peerinfo, iter->peer_it))
	{
	  iter->peer_it++;
	}
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER:
      assert(iter->v4pfx_it != kh_end(iter->view->v4pfxs));
      iter->v4pfx_peer_it = kh_begin(kh_val(iter->view->v4pfxs, iter->v4pfx_it));
      iter->v4pfx_peer_it_valid = 1;

      while(iter->v4pfx_peer_it !=
	    kh_end(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers) &&
	    (!kh_exist(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers,
		       iter->v4pfx_peer_it) ||
	     !kh_val(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers,
		     iter->v4pfx_peer_it).in_use))
	{
	  iter->v4pfx_peer_it++;
	}
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER:
      assert(iter->v6pfx_it != kh_end(iter->view->v6pfxs));
      iter->v6pfx_peer_it = kh_begin(kh_val(iter->view->v6pfxs, iter->v6pfx_it));
      iter->v6pfx_peer_it_valid = 1;

      while(iter->v6pfx_peer_it !=
	    kh_end(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers) &&
	    (!kh_exist(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers,
		       iter->v6pfx_peer_it) ||
	     !kh_val(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers,
		     iter->v6pfx_peer_it).in_use))
	{
	  iter->v6pfx_peer_it++;
	}
      break;

    default:
      /* programming error */
      assert(0);
    }
}

int bgpwatcher_view_iter_is_end(bgpwatcher_view_iter_t *iter,
				bgpwatcher_view_iter_field_t field)
{
  switch(field)
    {
    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX:
      return iter->v4pfx_it == kh_end(iter->view->v4pfxs);
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX:
      return iter->v6pfx_it == kh_end(iter->view->v6pfxs);
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_PEER:
      return iter->peer_it == kh_end(iter->view->peerinfo);
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER:
      assert(iter->v4pfx_it != kh_end(iter->view->v4pfxs));
      if(!iter->v4pfx_peer_it_valid ||
	 iter->v4pfx_peer_it
	 == kh_end(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers))
	{
	  iter->v4pfx_peer_it_valid = 0;
	  return 1;
	}
      else
	{
	  return 0;
	}
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER:
      assert(iter->v6pfx_it != kh_end(iter->view->v6pfxs));
      if(!iter->v6pfx_peer_it_valid ||
	 iter->v6pfx_peer_it
	 == kh_end(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers))
	{
	  iter->v6pfx_peer_it_valid = 0;
	  return 1;
	}
      else
	{
	  return 0;
	}
      break;

    default:
      /* programming error */
      assert(0);
    }
}

void bgpwatcher_view_iter_next(bgpwatcher_view_iter_t *iter,
			       bgpwatcher_view_iter_field_t field)
{
  if(bgpwatcher_view_iter_is_end(iter, field))
    {
      return;
    }

  switch(field)
    {
    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX:
      do {
	iter->v4pfx_it++;
      } while(iter->v4pfx_it != kh_end(iter->view->v4pfxs) &&
	      (!kh_exist(iter->view->v4pfxs, iter->v4pfx_it) ||
	       !kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_cnt));
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX:
      do {
	iter->v6pfx_it++;
      } while(iter->v6pfx_it != kh_end(iter->view->v6pfxs) &&
	      (!kh_exist(iter->view->v6pfxs, iter->v6pfx_it) ||
	       !kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_cnt));
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_PEER:
      do {
	iter->peer_it++;
      } while(iter->peer_it != kh_end(iter->view->peerinfo) &&
	      !kh_exist(iter->view->peerinfo, iter->peer_it));
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER:
      /* increment the iterator until:
	 1. we reach the end of the hash, or
	 2. (we find a peer valid in the hash, and
	 3.  we find a peer valid in the view)
      */
      do {
	iter->v4pfx_peer_it++;
      } while(iter->v4pfx_peer_it !=
	      kh_end(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers) &&
	      (!kh_exist(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers,
			iter->v4pfx_peer_it) ||
	      !kh_val(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers,
		      iter->v4pfx_peer_it).in_use));
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER:
      /* increment the iterator until:
	 1. we reach the end of the hash, or
	 2. (we find a peer valid in the hash, and
	 3.  we find a peer valid in the view)
      */
      do {
	iter->v6pfx_peer_it++;
      } while(iter->v6pfx_peer_it !=
	      kh_end(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers) &&
	      (!kh_exist(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers,
			iter->v6pfx_peer_it) ||
	      !kh_val(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers,
		      iter->v6pfx_peer_it).in_use));
      break;

    default:
      /* programming error */
      assert(0);
    }
}

uint64_t bgpwatcher_view_iter_size(bgpwatcher_view_iter_t *iter,
				   bgpwatcher_view_iter_field_t field)
{
  switch(field)
    {
    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX:
      return bgpwatcher_view_v4pfx_size(iter->view);
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX:
      return bgpwatcher_view_v6pfx_size(iter->view);
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_PEER:
      return bgpwatcher_view_peer_size(iter->view);
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER:
      if(!iter->v4pfx_peer_it_valid)
	{
	  bgpwatcher_view_iter_first(iter, field);
	}
      assert(iter->v4pfx_it != kh_end(iter->view->v4pfxs));
      assert(iter->v4pfx_peer_it_valid);
      return kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_cnt;
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER:
      if(!iter->v6pfx_peer_it_valid)
	{
	  bgpwatcher_view_iter_first(iter, field);
	}
      assert(iter->v6pfx_it != kh_end(iter->view->v6pfxs));
      assert(iter->v6pfx_peer_it_valid);
      return kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_cnt;
      break;

    default:
      /* programming error */
      assert(0);
    }
}

bl_ipv4_pfx_t *bgpwatcher_view_iter_get_v4pfx(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {
      return NULL;
    }
  return &kh_key(iter->view->v4pfxs, iter->v4pfx_it);
}

bl_ipv6_pfx_t *bgpwatcher_view_iter_get_v6pfx(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX))
    {
      return NULL;
    }
  return &kh_key(iter->view->v6pfxs, iter->v6pfx_it);
}

void *bgpwatcher_view_iter_get_v4pfx_user(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {
      return NULL;
    }
  return kh_val(iter->view->v4pfxs, iter->v4pfx_it)->user;
}

void *bgpwatcher_view_iter_get_v6pfx_user(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX))
    {
      return NULL;
    }
  return kh_val(iter->view->v6pfxs, iter->v6pfx_it)->user;
}

void bgpwatcher_view_iter_set_v4pfx_user(bgpwatcher_view_iter_t *iter, void *user)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {
      return;
    }
  kh_val(iter->view->v4pfxs, iter->v4pfx_it)->user = user;
  return;
}

void bgpwatcher_view_iter_set_v6pfx_user(bgpwatcher_view_iter_t *iter, void *user)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX))
    {
      return;
    }
  kh_val(iter->view->v6pfxs, iter->v6pfx_it)->user = user;
  return;
}

bl_peerid_t
bgpwatcher_view_iter_get_peerid(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      return 0;
    }
  return kh_key(iter->view->peerinfo, iter->peer_it);
}

bl_peer_signature_t *
bgpwatcher_view_iter_get_peersig(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      return NULL;
    }
  return bl_peersign_map_get_peersign(iter->view->peersigns,
				      bgpwatcher_view_iter_get_peerid(iter));
}

int bgpwatcher_view_iter_get_peer_v4pfx_cnt(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      return -1;
    }

  return kh_val(iter->view->peerinfo, iter->peer_it).v4_pfx_cnt;
}

int bgpwatcher_view_iter_get_peer_v6pfx_cnt(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      return -1;
    }

  return kh_val(iter->view->peerinfo, iter->peer_it).v6_pfx_cnt;
}

bl_peerid_t
bgpwatcher_view_iter_get_v4pfx_peerid(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
    {
      return 0;
    }

  return kh_key(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers,
		iter->v4pfx_peer_it);
}

bl_peerid_t
bgpwatcher_view_iter_get_v6pfx_peerid(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
    {
      return 0;
    }

  return kh_key(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers,
		iter->v6pfx_peer_it);
}

bl_peer_signature_t *
bgpwatcher_view_iter_get_v4pfx_peersig(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
    {
      return NULL;
    }

  return bl_peersign_map_get_peersign(iter->view->peersigns,
				   bgpwatcher_view_iter_get_v4pfx_peerid(iter));
}

bl_peer_signature_t *
bgpwatcher_view_iter_get_v6pfx_peersig(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
    {
      return NULL;
    }

  return bl_peersign_map_get_peersign(iter->view->peersigns,
				   bgpwatcher_view_iter_get_v6pfx_peerid(iter));
}

bgpwatcher_pfx_peer_info_t *
bgpwatcher_view_iter_get_v4pfx_pfxinfo(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
    {
      return NULL;
    }

  return &kh_val(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers,
		 iter->v4pfx_peer_it);
}

bgpwatcher_pfx_peer_info_t *
bgpwatcher_view_iter_get_v6pfx_pfxinfo(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
    {
      return NULL;
    }

  return &kh_val(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers,
		 iter->v6pfx_peer_it);
}
