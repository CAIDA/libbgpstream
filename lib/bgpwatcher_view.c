/*
 * This file is part of bgpwatcher
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

#include <stdio.h>

#include <czmq.h>

/* we need to poke our fingers into the peersign map @TODO do we? */
#include "bgpstream_utils_peer_sig_map.h"

#include "bgpwatcher_view_int.h"

struct bgpwatcher_view_iter {

  /** Pointer to the view instance we are iterating over */
  bgpwatcher_view_t *view;

  /** The IP version that is currently iterated*/
  bgpstream_addr_version_t version_ptr;

  /** 0 if all IP versions are iterated, 
   *  BGPSTREAM_ADDR_VERSION_IPV4 if only IPv4 are iterated, 
   *  BGPSTREAM_ADDR_VERSION_IPV6 if only IPv6 are iterated */
  int version_filter;

  /** Current v4pfx */
  khiter_t v4pfx_it;

  /** Current v6pfx */
  khiter_t v6pfx_it;

  /** State mask used for prefix iteration */
  uint8_t pfx_state_mask;

  /** Current peersig
   *  @todo: I guess this is an old doc, we are actually
   *  iterating over fields of value bwv_peerinfo_t */
  khiter_t peer_it;

  /** State mask used for peer iteration */
  uint8_t peer_state_mask;
  
  /** Current v4pfx_peer */
  khiter_t v4pfx_peer_it;
  /** Is the v4pfx_peer iterator valid? */
  int v4pfx_peer_it_valid;

  /** Current v6pfx_peer */
  khiter_t v6pfx_peer_it;
  /** Is the v6pfx_peer iterator valid? */
  int v6pfx_peer_it_valid;
  
  /** State mask used for prefix-peer iteration */
  uint8_t pfx_peer_state_mask;

};


/* ========== PRIVATE FUNCTIONS ========== */

static int peerinfo_add_pfx(bgpwatcher_view_t *view, bgpstream_peer_id_t peerid,
                            bgpstream_pfx_t *prefix)
{
  khiter_t k;
  int khret;
  /* for now, just increment the prefix count */

  /* this MUST only be called when first adding a peer to a prefix, otherwise we
     will double-count prefixes that are added twice */

  if((k = kh_get(bwv_peerid_peerinfo, view->peerinfo, peerid))
     == kh_end(view->peerinfo))
    {
      /* first prefix for this peer, it creates a new peerinfo structure */
      k = kh_put(bwv_peerid_peerinfo, view->peerinfo, peerid, &khret);
      kh_val(view->peerinfo, k).id = peerid;
      kh_val(view->peerinfo, k).state = BGPWATCHER_VIEW_FIELD_INVALID;
      kh_val(view->peerinfo, k).user = NULL;
    }
  
  // activate/init peer
  if(kh_val(view->peerinfo, k).state != BGPWATCHER_VIEW_FIELD_ACTIVE)
    {
      kh_val(view->peerinfo, k).v4_pfx_cnt = 0;
      kh_val(view->peerinfo, k).v6_pfx_cnt = 0;
      kh_val(view->peerinfo, k).state = BGPWATCHER_VIEW_FIELD_ACTIVE;
      view->peerinfo_cnt++;
    }

  switch(prefix->address.version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      kh_val(view->peerinfo, k).v4_pfx_cnt++;
      break;

    case BGPSTREAM_ADDR_VERSION_IPV6:
      kh_val(view->peerinfo, k).v6_pfx_cnt++;
      break;

    default:
      return -1;
      break;
    }

  return 0;
}

static void peerinfo_destroy_user(bgpwatcher_view_t *view)
{
  khiter_t k;
  if(view->peer_user_destructor != NULL)
    {
      for(k = kh_begin(view->peerinfo); k != kh_end(view->peerinfo); ++k)
        {
          if(kh_exist(view->peerinfo, k))
            {
              if(kh_value(view->peerinfo, k).user != NULL)
                {
                  view->peer_user_destructor(kh_value(view->peerinfo, k).user);
                  kh_value(view->peerinfo, k).user = NULL;
                }
            }
        }
    }      
}
                                                            
static bwv_peerid_pfxinfo_t* peerid_pfxinfo_create()
{
  bwv_peerid_pfxinfo_t *v;

  if((v = malloc(sizeof(bwv_peerid_pfxinfo_t))) == NULL)
    {
      return NULL;
    }

  v->peers = NULL;
  v->peers_alloc_cnt = 0;
  v->peers_cnt = 0;
  v->state = BGPWATCHER_VIEW_FIELD_INVALID;

  v->user = NULL;

  return v;
}

static int peerid_pfxinfo_insert(bgpwatcher_view_t *view,
                                 bgpstream_pfx_t *prefix,
                                 bwv_peerid_pfxinfo_t *v,
                                 bgpstream_peer_id_t peerid,
                                 bgpwatcher_pfx_peer_info_t *pfx_info)
{
  int i;
  
  /* if we are the first to insert a peer for this prefix after it was cleared,
     we are also responsible for clearing all the peer info */
  if(v->peers_cnt == 0)
    {
      for(i=0; i<v->peers_alloc_cnt; i++)
	{
          v->peers[i].state = BGPWATCHER_VIEW_FIELD_INVALID;
	}
    }

  /* need to realloc the array? */
  if((peerid+1) > v->peers_alloc_cnt)
    {
      if((v->peers =
          realloc(v->peers,
                  sizeof(bgpwatcher_pfx_peer_info_t)*(peerid+1))) == NULL)
        {
          return -1;
        }

      /* now we have to zero everything between prev_last and the end */
      for(i = v->peers_alloc_cnt; i <= peerid; i++)
        {
          v->peers[i].state = BGPWATCHER_VIEW_FIELD_INVALID;
          v->peers[i].user = NULL;
        }

      v->peers_alloc_cnt = peerid+1;
    }

  /* if this peer was not previously used, we need to count it */
  if(v->peers[peerid].state != BGPWATCHER_VIEW_FIELD_ACTIVE)
    {
      v->peers_cnt++;
      v->state = BGPWATCHER_VIEW_FIELD_ACTIVE;
      /* and we need to add this prefix to the peerinfo counter */
      peerinfo_add_pfx(view, peerid, prefix);
    }

  v->peers[peerid].orig_asn = pfx_info->orig_asn;
  v->peers[peerid].state = BGPWATCHER_VIEW_FIELD_ACTIVE;
  /** v->peers[peerid].user remains untouched */

  return 0;
}

static void pfx_peer_info_destroy(bgpwatcher_view_t *view, bgpwatcher_pfx_peer_info_t *v)
{
  if(v != NULL)
    {
      if(v->user != NULL && view->pfx_peer_user_destructor != NULL)
        {
          view->pfx_peer_user_destructor(v->user);
        }
      v->user = NULL;
    }
}


static void peerid_pfxinfo_destroy(bgpwatcher_view_t *view, bwv_peerid_pfxinfo_t *v)
{
  if(v == NULL)
    {
      return;
    }
  int i = 0;
  if(v->peers!=NULL)
    {
      /* Warning: the peer id is always equal to the position
       * of the peer in the peers array. Since there is no 
       * peer with id 0, the array is always 1 elem larger, also
       * position 0 is to be considered not valid at all times.
       * We cannot destroy memory that is not valid, so we
       * destroy just the peer_infos with id >=1  */
      for(i = 1; i< v->peers_alloc_cnt; i++)
        {
          pfx_peer_info_destroy(view, &v->peers[i]);
        }
        free(v->peers);
    }
  v->peers = NULL;
  v->peers_cnt = BGPWATCHER_VIEW_FIELD_INVALID;
  v->peers_alloc_cnt = 0;
  if(view->pfx_user_destructor != NULL && v->user != NULL)
    {
      view->pfx_user_destructor(v->user);
    }
  v->user = NULL;
  free(v);
}

/** @todo consider making these macros? */
static bwv_peerid_pfxinfo_t *get_v4pfx_peerids(bgpwatcher_view_t *view,
                                               bgpstream_ipv4_pfx_t *v4pfx)
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
                                               bgpstream_ipv6_pfx_t *v6pfx)
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
                                             bgpstream_pfx_t *prefix)
{
  if(prefix->address.version == BGPSTREAM_ADDR_VERSION_IPV4)
    {
      return get_v4pfx_peerids(view, (bgpstream_ipv4_pfx_t *)(prefix));
    }
  else if(prefix->address.version == BGPSTREAM_ADDR_VERSION_IPV6)
    {
      return get_v6pfx_peerids(view, (bgpstream_ipv6_pfx_t *)(prefix));
    }

  return NULL;
}

/* ========== PROTECTED FUNCTIONS ========== */

void bgpwatcher_view_clear(bgpwatcher_view_t *view)
{
  khiter_t k;

  view->time = 0;

  gettimeofday(&view->time_created, NULL);

  /* mark all ipv4 prefixes as unused */
  for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); ++k)
    {
      if(kh_exist(view->v4pfxs, k))
	{
	  kh_value(view->v4pfxs, k)->peers_cnt = 0;
          kh_value(view->v4pfxs, k)->state = BGPWATCHER_VIEW_FIELD_INVALID;
	}
    }
  view->v4pfxs_cnt = 0;

  /* mark all ipv6 prefixes as unused */
  for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); ++k)
    {
      if(kh_exist(view->v6pfxs, k))
	{
	  kh_value(view->v6pfxs, k)->peers_cnt = 0;
          kh_value(view->v6pfxs, k)->state = BGPWATCHER_VIEW_FIELD_INVALID;
	}
    }
  view->v6pfxs_cnt = 0;

  /* clear out the peerinfo table */
  for(k = kh_begin(view->peerinfo); k != kh_end(view->peerinfo); ++k)
    {
      if(kh_exist(view->peerinfo, k))
	{
	  kh_value(view->peerinfo, k).state = BGPWATCHER_VIEW_FIELD_INVALID;
	}
    }
  view->peerinfo_cnt = 0;
  
  view->pub_cnt = 0;
}

int bgpwatcher_view_add_prefix(bgpwatcher_view_t *view,
                               bgpstream_pfx_t *prefix,
                               bgpstream_peer_id_t peerid,
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

  if(peerid_pfxinfo_insert(view, prefix,
                           peerids_pfxinfo, peerid, pfx_info) < 0)
    {
      return -1;
    }

  return 0;
}




/* ========== PUBLIC FUNCTIONS ========== */

bgpwatcher_view_t *bgpwatcher_view_create_shared(bgpstream_peer_sig_map_t *peersigns,
                                                 bgpwatcher_view_destroy_user_t *bwv_user_destructor,
                                                 bgpwatcher_view_destroy_user_t *bwv_peer_user_destructor,
                                                 bgpwatcher_view_destroy_user_t *bwv_pfx_user_destructor,
                                                 bgpwatcher_view_destroy_user_t *bwv_pfx_peer_user_destructor)
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
      if((view->peersigns = bgpstream_peer_sig_map_create()) == NULL)
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
  view->peerinfo_cnt = 0;

  gettimeofday(&view->time_created, NULL);

  view->user_destructor = bwv_user_destructor;
  view->peer_user_destructor = bwv_peer_user_destructor;
  view->pfx_user_destructor = bwv_pfx_user_destructor;  
  view->pfx_peer_user_destructor = bwv_pfx_peer_user_destructor;
  
  view->user = NULL;

  return view;

 err:
  fprintf(stderr, "Failed to create BGP Watcher View\n");
  bgpwatcher_view_destroy(view);
  return NULL;
}

bgpwatcher_view_t *bgpwatcher_view_create(bgpwatcher_view_destroy_user_t *bwv_user_destructor,
                                          bgpwatcher_view_destroy_user_t *bwv_peer_user_destructor,
                                          bgpwatcher_view_destroy_user_t *bwv_pfx_user_destructor,
                                          bgpwatcher_view_destroy_user_t *bwv_pfx_peer_user_destructor)
{
  return bgpwatcher_view_create_shared(NULL, bwv_user_destructor, bwv_peer_user_destructor,
                                       bwv_pfx_user_destructor, bwv_pfx_peer_user_destructor);
}

void bgpwatcher_view_destroy(bgpwatcher_view_t *view)
{
  if(view == NULL)
    {
      return;
    }

  khiter_t k;

  if(view->v4pfxs != NULL)
    {
      for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); ++k)
        {
          if(kh_exist(view->v4pfxs, k))
            {
              peerid_pfxinfo_destroy(view, kh_value(view->v4pfxs, k));
            }
        }
      kh_destroy(bwv_v4pfx_peerid_pfxinfo, view->v4pfxs);
      view->v4pfxs = NULL;
    }

  if(view->v6pfxs != NULL)
    {
      for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); ++k)
        {
          if(kh_exist(view->v6pfxs, k))
            {
              peerid_pfxinfo_destroy(view, kh_value(view->v6pfxs, k));
            }
        }
      kh_destroy(bwv_v6pfx_peerid_pfxinfo, view->v6pfxs);
      view->v6pfxs = NULL;
    }

  if(view->peersigns_shared == 0 && view->peersigns != NULL)
    {
      bgpstream_peer_sig_map_destroy(view->peersigns);
      view->peersigns = NULL;
    }

  if(view->peerinfo != NULL)
    {
      peerinfo_destroy_user(view);
      kh_destroy(bwv_peerid_peerinfo, view->peerinfo);
      view->peerinfo = NULL;
    }

  if(view->user != NULL)
    {
      if(view->user_destructor != NULL)
        {
          view->user_destructor(view->user);
        }
      view->user = NULL;
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
  return view->peerinfo_cnt;
}

uint32_t bgpwatcher_view_time(bgpwatcher_view_t *view)
{
  return view->time;
}

void *bgpwatcher_view_get_user(bgpwatcher_view_t *view)
{
  return view->user;
}

int bgpwatcher_view_set_user(bgpwatcher_view_t *view, void *user)
{
  if(view->user == user)
    {
      return 0;
    }
  if(view->user != NULL && view->user_destructor != NULL)
    {
      view->user_destructor(view->user);
    }
  view->user = user;
  return 1;
}

void bgpwatcher_view_set_user_destructor(bgpwatcher_view_t *view,
                                         bgpwatcher_view_destroy_user_t *bwv_user_destructor)
{
  view->user_destructor = bwv_user_destructor;
}

void
bgpwatcher_view_set_pfx_user_destructor(bgpwatcher_view_t *view,
                                        bgpwatcher_view_destroy_user_t *bwv_pfx_user_destructor)
{
   view->pfx_user_destructor = bwv_pfx_user_destructor;  
}

void
bgpwatcher_view_set_peer_user_destructor(bgpwatcher_view_t *view,
                                         bgpwatcher_view_destroy_user_t *bwv_peer_user_destructor)
{
  view->peer_user_destructor = bwv_peer_user_destructor;
}

void
bgpwatcher_view_set_pfx_peer_user_destructor(bgpwatcher_view_t *view,
                                             bgpwatcher_view_destroy_user_t *bwv_pfx_peer_user_destructor)
{
  view->pfx_peer_user_destructor = bwv_pfx_peer_user_destructor;
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

  iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV4;
  iter->version_filter = 0; // default: all prefix versions

  iter->v4pfx_it = kh_end(iter->view->v4pfxs);
  iter->v6pfx_it = kh_end(iter->view->v6pfxs);
  iter->peer_it  = kh_end(iter->view->peerinfo);

  iter->v4pfx_peer_it_valid = 0;
  iter->v6pfx_peer_it_valid = 0;

  // default: all valid fields are iterated
  iter->pfx_state_mask = BGPWATCHER_VIEW_FIELD_ALL_VALID;
  iter->peer_state_mask = BGPWATCHER_VIEW_FIELD_ALL_VALID;
  iter->pfx_peer_state_mask = BGPWATCHER_VIEW_FIELD_ALL_VALID;

  return iter;
}

void bgpwatcher_view_iter_destroy(bgpwatcher_view_iter_t *iter)
{
  free(iter);
}

int
bgpwatcher_view_iter_first_pfx(bgpwatcher_view_iter_t *iter,
                               int version,
                               uint8_t state_mask)
{
  // set the version we iterate through
  iter->version_filter = version;
  
  // set the version we start iterating through
  if(iter->version_filter == BGPSTREAM_ADDR_VERSION_IPV4 || iter->version_filter == 0)
    {
      iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV4;
    }
  else
    {
      iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV6;
    }

  // set the pfx mask
  iter->pfx_state_mask = state_mask;
     
  if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
    {
      iter->v4pfx_it = kh_begin(iter->view->v4pfxs);
      while(iter->v4pfx_it != kh_end(iter->view->v4pfxs) &&
	    (!kh_exist(iter->view->v4pfxs, iter->v4pfx_it) ||
             !(iter->pfx_state_mask & kh_val(iter->view->v4pfxs, iter->v4pfx_it)->state)))
	{
	  iter->v4pfx_it++;
	}
      // a matching first ipv4 prefix was found
      if(iter->v4pfx_it != kh_end(iter->view->v4pfxs))
        {
          iter->v4pfx_peer_it = 0;
          iter->v4pfx_peer_it_valid = 1;
          return 1;
        }
      // no ipv4 prefix was found, we don't look for other versions
      // unless version_filter is zero
      if(iter->version_filter)  
        { 
          return 0;   
        }

      // continue to the next IP version
      iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV6;
    }

  if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
    {
      iter->v6pfx_it = kh_begin(iter->view->v6pfxs);
      /* keep searching if this does not exist */
      while(iter->v6pfx_it != kh_end(iter->view->v6pfxs) &&
	    (!kh_exist(iter->view->v6pfxs, iter->v6pfx_it) ||
             !(iter->pfx_state_mask & kh_val(iter->view->v6pfxs, iter->v6pfx_it)->state)))
	{
	  iter->v6pfx_it++;
	}
      // a matching first ipv6 prefix was found
      if(iter->v6pfx_it != kh_end(iter->view->v6pfxs))
        {
          iter->v6pfx_peer_it = 0;
          iter->v6pfx_peer_it_valid = 1;      
          return 1;
        }
      // there are no more ip versions to look for
      return 0;   
    }

  return 0;
}

int
bgpwatcher_view_iter_next_pfx(bgpwatcher_view_iter_t *iter)
{
   if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
    {
      do {
	iter->v4pfx_it++;
      } while(iter->v4pfx_it != kh_end(iter->view->v4pfxs) &&
	      (!kh_exist(iter->view->v4pfxs, iter->v4pfx_it) ||
	       !(iter->pfx_state_mask & kh_val(iter->view->v4pfxs, iter->v4pfx_it)->state)));
      // a matching ipv4 prefix was found
      if(iter->v4pfx_it != kh_end(iter->view->v4pfxs))
        {
          iter->v4pfx_peer_it = 0;
          iter->v4pfx_peer_it_valid = 1;
          return 1;
        }
      // no ipv4 prefix was found, we don't look for other versions
      if(iter->version_filter) 
        { 
          return 0;   
        }


      // when we reach the end of ipv4 we continue to
      // the next IP version and we look for the first
      // ipv6 prefix
      iter->v6pfx_it = kh_begin(iter->view->v6pfxs);
      iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV6;
      
      /* keep searching if this does not exist */
      while(iter->v6pfx_it != kh_end(iter->view->v6pfxs) &&
	    (!kh_exist(iter->view->v6pfxs, iter->v6pfx_it) ||
             !(iter->pfx_state_mask & kh_val(iter->view->v6pfxs, iter->v6pfx_it)->state)))
	{
	  iter->v6pfx_it++;
	}
      // a matching first ipv6 prefix was found
      if(iter->v6pfx_it != kh_end(iter->view->v6pfxs))
        {
          iter->v6pfx_peer_it = 0;
          iter->v6pfx_peer_it_valid = 1;      
          return 1;
        }
      // if here we are also at the end of ipv6
      return 0;
    }

  if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
    {
      do {
	iter->v6pfx_it++;
      } while(iter->v6pfx_it != kh_end(iter->view->v6pfxs) &&
	      (!kh_exist(iter->view->v6pfxs, iter->v6pfx_it) ||
	       !(iter->pfx_state_mask & kh_val(iter->view->v6pfxs, iter->v6pfx_it)->state)));
      // a matching ipv6 prefix was found
      if(iter->v6pfx_it != kh_end(iter->view->v6pfxs))
        {
          iter->v6pfx_peer_it = 0;
          iter->v6pfx_peer_it_valid = 1;      
          return 1;
        }
      // there are no more ip versions to look for
      return 0;   
    }

  return 0;
}

int
bgpwatcher_view_iter_has_more_pfx(bgpwatcher_view_iter_t *iter)
{
  if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
    {
      // if there are more ipv4 prefixes 
      if(iter->v4pfx_it != kh_end(iter->view->v4pfxs))
        {
          return 1;
        }
      // if there are no more ipv4 prefixes and we filter
      if(iter->version_filter)
        {
          return 0;
        }
      // continue to the next IP version
      iter->v6pfx_it = kh_begin(iter->view->v6pfxs);
      iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV6;
      /* keep searching if this does not exist */
      while(iter->v6pfx_it != kh_end(iter->view->v6pfxs) &&
	    (!kh_exist(iter->view->v6pfxs, iter->v6pfx_it) ||
             !(iter->pfx_state_mask & kh_val(iter->view->v6pfxs, iter->v6pfx_it)->state)))
	{
	  iter->v6pfx_it++;
	}
    }

  // if the version is ipv6, return 1 if there are more ipv6 prefixes
  if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
    {
      return iter->v6pfx_it != kh_end(iter->view->v6pfxs);
    }

  return 0;
}

int
bgpwatcher_view_iter_seek_pfx(bgpwatcher_view_iter_t *iter,
                              bgpstream_pfx_t *pfx,
                              uint8_t state_mask)
{
  iter->version_filter = pfx->address.version;
  iter->version_ptr = pfx->address.version;
  iter->pfx_state_mask = state_mask;
  
  // after an unsuccessful seek, all the pfx iterators
  // have to point at the end
  iter->v4pfx_it = kh_end(iter->view->v4pfxs);
  iter->v6pfx_it = kh_end(iter->view->v6pfxs);
  
  switch(pfx->address.version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      iter->v4pfx_it = kh_get(bwv_v4pfx_peerid_pfxinfo, iter->view->v4pfxs, *((bgpstream_ipv4_pfx_t *)pfx));
      if(iter->v4pfx_it != kh_end(iter->view->v4pfxs))
        {
          if(iter->pfx_state_mask & kh_val(iter->view->v4pfxs, iter->v4pfx_it)->state)
            {
              iter->v4pfx_peer_it = 0;
              iter->v4pfx_peer_it_valid = 1;
              return 1;
            }
          // if the mask does not match, than set the iterator to the end
          iter->v4pfx_it = kh_end(iter->view->v4pfxs);
        }
      return 0;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      iter->v6pfx_it = kh_get(bwv_v6pfx_peerid_pfxinfo, iter->view->v6pfxs, *((bgpstream_ipv6_pfx_t *)pfx));
      if(iter->v6pfx_it != kh_end(iter->view->v6pfxs))
        {
          if(iter->pfx_state_mask & kh_val(iter->view->v6pfxs, iter->v6pfx_it)->state)
            {
              iter->v6pfx_peer_it = 0;
              iter->v6pfx_peer_it_valid = 1;
              return 1;
            }
          // if the mask does not match, than set the iterator to the end
          iter->v6pfx_it = kh_end(iter->view->v6pfxs);
        }
      return 0;
    default:
      /* programming error */
      assert(0);
    }
  return 0;
}

int
bgpwatcher_view_iter_first_peer(bgpwatcher_view_iter_t *iter,                               
                                uint8_t state_mask)
{
  iter->peer_it = kh_begin(iter->view->peerinfo);
  iter->peer_state_mask = state_mask;
  /* keep searching if this does not exist */
  while(iter->peer_it != kh_end(iter->view->peerinfo) &&
        (!kh_exist(iter->view->peerinfo, iter->peer_it) ||
         !(iter->peer_state_mask & kh_val(iter->view->peerinfo, iter->peer_it).state)))
    {
      iter->peer_it++;
    }
  if(iter->peer_it != kh_end(iter->view->peerinfo))
    {
      return 1;
    }
  return 0;
}

int
bgpwatcher_view_iter_next_peer(bgpwatcher_view_iter_t *iter)
{
  do {
    iter->peer_it++;
  } while(iter->peer_it != kh_end(iter->view->peerinfo) &&
          (!kh_exist(iter->view->peerinfo, iter->peer_it) ||
           !(iter->peer_state_mask & kh_val(iter->view->peerinfo, iter->peer_it).state)));  
  if(iter->peer_it != kh_end(iter->view->peerinfo))
    {
      return 1;
    }
  return 0;
}

int
bgpwatcher_view_iter_has_more_peer(bgpwatcher_view_iter_t *iter)
{
  if(iter->peer_it != kh_end(iter->view->peerinfo))
    {
      return 1;
    }
  return 0;
}

int
bgpwatcher_view_iter_seek_peer(bgpwatcher_view_iter_t *iter,
                               bgpstream_peer_id_t peerid,
                               uint8_t state_mask)
{
  iter->peer_state_mask = state_mask;
  iter->peer_it = kh_get(bwv_peerid_peerinfo, iter->view->peerinfo, peerid);
  if(iter->peer_it != kh_end(iter->view->peerinfo))
    {
      if(iter->peer_state_mask & kh_val(iter->view->peerinfo, iter->peer_it).state)
        {
          return 1;
        }
      iter->peer_it = kh_end(iter->view->peerinfo);
    }
  return 0;
}

int
bgpwatcher_view_iter_pfx_first_peer(bgpwatcher_view_iter_t *iter,
                                    uint8_t state_mask)
{
  iter->pfx_peer_state_mask = state_mask;

  switch(iter->version_ptr)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      assert(iter->v4pfx_it != kh_end(iter->view->v4pfxs));
      iter->v4pfx_peer_it = 0;
      iter->v4pfx_peer_it_valid = 1;
        while((iter->v4pfx_peer_it <
             kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt) &&
	    (!(iter->pfx_peer_state_mask & kh_val(iter->view->v4pfxs, iter->v4pfx_it)
               ->peers[iter->v4pfx_peer_it].state)))
	{
	  iter->v4pfx_peer_it++;
	}
        if(iter->v4pfx_peer_it < kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt)
          {
            bgpwatcher_view_iter_seek_peer(iter, iter->v4pfx_peer_it, state_mask);
            return 1;
          }
        iter->v4pfx_peer_it_valid = 0;
        return 0;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      assert(iter->v6pfx_it != kh_end(iter->view->v6pfxs));
      iter->v6pfx_peer_it = 0;
      iter->v6pfx_peer_it_valid = 1;
        while((iter->v6pfx_peer_it <
             kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt) &&
	    (!(iter->pfx_peer_state_mask & kh_val(iter->view->v6pfxs, iter->v6pfx_it)
               ->peers[iter->v6pfx_peer_it].state)))
	{
	  iter->v6pfx_peer_it++;
	}
        if(iter->v6pfx_peer_it < kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt)
          {
            bgpwatcher_view_iter_seek_peer(iter, iter->v6pfx_peer_it, state_mask);
            return 1;
          }
        iter->v6pfx_peer_it_valid = 0;
        return 0;
    default:
      /* programming error */
      assert(0);
    }
  return 0;
}

int
bgpwatcher_view_iter_pfx_next_peer(bgpwatcher_view_iter_t *iter)
{
  switch(iter->version_ptr)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      assert(iter->v4pfx_it != kh_end(iter->view->v4pfxs));
      do {
	iter->v4pfx_peer_it++;
      } while(iter->v4pfx_peer_it <
	      kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt &&
	      (!(iter->pfx_peer_state_mask & kh_val(iter->view->v4pfxs, iter->v4pfx_it)
               ->peers[iter->v4pfx_peer_it].state)));
      if(iter->v4pfx_peer_it < kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt)
        {
          bgpwatcher_view_iter_seek_peer(iter, iter->v4pfx_peer_it, iter->pfx_peer_state_mask);
          return 1;
        }
      iter->v4pfx_peer_it_valid = 0;      
      return 0;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      assert(iter->v6pfx_it != kh_end(iter->view->v6pfxs));
      do {
	iter->v6pfx_peer_it++;
      } while(iter->v6pfx_peer_it <
	      kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt &&
	      (!(iter->pfx_peer_state_mask & kh_val(iter->view->v6pfxs, iter->v6pfx_it)
               ->peers[iter->v6pfx_peer_it].state)));
      if(iter->v6pfx_peer_it < kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt)
        {
          bgpwatcher_view_iter_seek_peer(iter, iter->v6pfx_peer_it, iter->pfx_peer_state_mask);
          return 1;
        }
      iter->v6pfx_peer_it_valid = 0;
      return 0;
    default:
      /* programming error */
      assert(0);
    }
  return 0;
}

int
bgpwatcher_view_iter_pfx_has_more_peer(bgpwatcher_view_iter_t *iter)
{
  switch(iter->version_ptr)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      assert(iter->v4pfx_it != kh_end(iter->view->v4pfxs));
      if(iter->v4pfx_peer_it < kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt)
        {
          return 1;
        }
      iter->v4pfx_peer_it_valid = 0;
      return 0;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      assert(iter->v6pfx_it != kh_end(iter->view->v6pfxs));
      if(iter->v6pfx_peer_it < kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt)
        {
          return 1;
        }
      iter->v6pfx_peer_it_valid = 0;
      return 0;
    default:
      /* programming error */
      assert(0);
    }
  return 0;  
}

int
bgpwatcher_view_iter_pfx_seek_peer(bgpwatcher_view_iter_t *iter,
                                   bgpstream_peer_id_t peerid,
                                   uint8_t state_mask)
{
  iter->pfx_peer_state_mask = state_mask;
  switch(iter->version_ptr)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      assert(iter->v4pfx_it != kh_end(iter->view->v4pfxs));
      if(peerid < kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt)
        {
          if(iter->pfx_peer_state_mask & kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers[peerid].state)
            {
              iter->v4pfx_peer_it_valid = 1;
              iter->v4pfx_peer_it = peerid;
              bgpwatcher_view_iter_seek_peer(iter, iter->v4pfx_peer_it, state_mask);
              return 1;
            }
        }
      iter->v4pfx_peer_it = kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt;
      iter->v4pfx_peer_it_valid = 0;
      return 0;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      assert(iter->v6pfx_it != kh_end(iter->view->v6pfxs));
      if(peerid < kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt)
        {
          if(iter->pfx_peer_state_mask & kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers[peerid].state)
            {
              iter->v6pfx_peer_it_valid = 1;
              iter->v6pfx_peer_it = peerid;
              bgpwatcher_view_iter_seek_peer(iter, iter->v6pfx_peer_it, state_mask);
              return 1;
            }
        }
      iter->v6pfx_peer_it = kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt;
      iter->v6pfx_peer_it_valid = 0;
      return 0;
    default:
      /* programming error */
      assert(0);
    }
  return 0;
}

int
bgpwatcher_view_iter_first_pfx_peer(bgpwatcher_view_iter_t *iter,
                                    int version,
                                    uint8_t pfx_mask,
                                    uint8_t peer_mask)
{
    // set the version(s) we iterate through
  iter->version_filter = version;
  
  // set the version we start iterating through
  if(iter->version_filter == BGPSTREAM_ADDR_VERSION_IPV4 || iter->version_filter == 0)
    {
      iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV4;
    }
  else
    {
      iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV6;
    }

  // masks are going to be set by each first function
  iter->pfx_state_mask = 0;
  iter->pfx_peer_state_mask = 0;
  
  // start from the first matching prefix
  bgpwatcher_view_iter_first_pfx(iter, version, pfx_mask);  
  while(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      // look for the first matching peer within the prefix
      if(bgpwatcher_view_iter_pfx_first_peer(iter, peer_mask))
        {
          return 1;
        }
      bgpwatcher_view_iter_next_pfx(iter);
    }
  return 0;       
}

int 
bgpwatcher_view_iter_next_pfx_peer(bgpwatcher_view_iter_t *iter)
{  
  while(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      // look for the next matching peer within the prefix
      if(bgpwatcher_view_iter_pfx_next_peer(iter))
        {
          return 1;
        }
      // if there are no more peers for the given prefix
      // go to the next prefix
      if(bgpwatcher_view_iter_next_pfx(iter))
        {
          // and check if the first peer is available
          bgpwatcher_view_iter_pfx_first_peer(iter, iter->pfx_peer_state_mask);
          if(bgpwatcher_view_iter_pfx_has_more_peer(iter))
            {
              return 1;
            }            
        }
    }
    return 0;
}

int
bgpwatcher_view_iter_has_more_pfx_peer(bgpwatcher_view_iter_t *iter)
{
    while(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      // look for the next matching peer within the prefix
      if(bgpwatcher_view_iter_pfx_has_more_peer(iter))
        {
          return 1;
        }
      if(bgpwatcher_view_iter_next_pfx(iter))
        {
          bgpwatcher_view_iter_pfx_first_peer(iter, iter->pfx_peer_state_mask);
        }
    }
    return 0;
}

int
bgpwatcher_view_iter_seek_pfx_peer(bgpwatcher_view_iter_t *iter,
                                   bgpstream_pfx_t *pfx,
                                   bgpstream_peer_id_t peerid,                                   
                                   uint8_t pfx_mask,
                                   uint8_t peer_mask)
{
  // all these filters are reset to default, and then
  // set by the single seek fuctions 
  iter->version_filter = 0;
  iter->version_ptr = BGPSTREAM_ADDR_VERSION_IPV4;
  iter->pfx_state_mask = 0;
  iter->pfx_peer_state_mask = 0;
  
  if(bgpwatcher_view_iter_seek_pfx(iter, pfx, pfx_mask))
    {
      if(bgpwatcher_view_iter_pfx_seek_peer(iter, peerid, peer_mask))
        {
          return 1;          
        }
      // if the peer is not found we also reset the prefix iterator
      iter->v4pfx_it = kh_end(iter->view->v4pfxs);
      iter->v6pfx_it = kh_end(iter->view->v6pfxs);
    }  
  iter->v4pfx_peer_it_valid = 0;
  iter->v6pfx_peer_it_valid = 0;
  
  return 0;
}

bgpwatcher_view_t *
bgpwatcher_view_iter_get_view(bgpwatcher_view_iter_t *iter)
{
  if(iter != NULL)
    {
      return iter->view;
    }
  return NULL;
}

int
bgpwatcher_view_iter_activate_pfx(bgpwatcher_view_iter_t *iter)
{
  bgpwatcher_view_field_state_t *st;
  if(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          st = &kh_val(iter->view->v4pfxs, iter->v4pfx_it)->state;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          st = &kh_val(iter->view->v6pfxs, iter->v6pfx_it)->state;
        }
      if(*st == BGPWATCHER_VIEW_FIELD_INACTIVE)
        {
          *st = BGPWATCHER_VIEW_FIELD_ACTIVE;
          return 1;
        }
      // if already active
      return 0;
    }
  return -1;
}

int
bgpwatcher_view_iter_deactivate_pfx(bgpwatcher_view_iter_t *iter)
{
  bgpwatcher_view_field_state_t *st;
  if(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          st = &kh_val(iter->view->v4pfxs, iter->v4pfx_it)->state;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          st = &kh_val(iter->view->v6pfxs, iter->v6pfx_it)->state;
        }
      if(*st == BGPWATCHER_VIEW_FIELD_ACTIVE)
        {
          // deactivate all the pfx-peers for this prefix
          for(bgpwatcher_view_iter_pfx_first_peer(iter, BGPWATCHER_VIEW_FIELD_ACTIVE);
              bgpwatcher_view_iter_pfx_has_more_peer(iter);
              bgpwatcher_view_iter_pfx_next_peer(iter))
            {
              bgpwatcher_view_iter_pfx_deactivate_peer(iter);
            }
          *st = BGPWATCHER_VIEW_FIELD_INACTIVE;          
        }
      // if already inactive
      return 0;
    }
  return -1;
}

bgpstream_pfx_t *
bgpwatcher_view_iter_pfx_get_pfx(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          return (bgpstream_pfx_t *)&kh_key(iter->view->v4pfxs, iter->v4pfx_it);
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          return (bgpstream_pfx_t *)&kh_key(iter->view->v6pfxs, iter->v6pfx_it);
        }
    }
  return NULL;
}

int
bgpwatcher_view_iter_pfx_get_peers_cnt(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          return kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_cnt;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          return kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_cnt;
        }
    }
  return -1;
}

bgpwatcher_view_field_state_t
bgpwatcher_view_iter_pfx_get_state(bgpwatcher_view_iter_t *iter)
  {
  if(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          return kh_val(iter->view->v4pfxs, iter->v4pfx_it)->state;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          return kh_val(iter->view->v6pfxs, iter->v6pfx_it)->state;
        }
    }
  return BGPWATCHER_VIEW_FIELD_INVALID;
}

void *
bgpwatcher_view_iter_pfx_get_user(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          return kh_val(iter->view->v4pfxs, iter->v4pfx_it)->user;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          return kh_val(iter->view->v6pfxs, iter->v6pfx_it)->user;
        }
    }
  return NULL;
}

int
bgpwatcher_view_iter_pfx_set_user(bgpwatcher_view_iter_t *iter, void *user)
{
 if(bgpwatcher_view_iter_has_more_pfx(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          if(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->user == user)
            {
              return 0;
            }          
          if(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->user != NULL &&
             iter->view->pfx_user_destructor != NULL)
            {
              iter->view->pfx_user_destructor(kh_val(iter->view->v4pfxs, iter->v4pfx_it)->user);              
            }
          kh_val(iter->view->v4pfxs, iter->v4pfx_it)->user = user;              
          return 1;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          if(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->user == user)
            {
              return 0;
            }          
          if(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->user != NULL &&
             iter->view->pfx_user_destructor != NULL)
            {
              iter->view->pfx_user_destructor(kh_val(iter->view->v6pfxs, iter->v6pfx_it)->user);              
            }
          kh_val(iter->view->v6pfxs, iter->v6pfx_it)->user = user;              
          return 1;
        }
    }
  return -1;
}

int
bgpwatcher_view_iter_activate_peer(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_has_more_peer(iter))
    {
      if(kh_val(iter->view->peerinfo, iter->peer_it).state == BGPWATCHER_VIEW_FIELD_INACTIVE)
        {
          kh_val(iter->view->peerinfo, iter->peer_it).state = BGPWATCHER_VIEW_FIELD_ACTIVE;
          return 1;
        }
      // if already active
      return 0;
    }
  return -1;
}

int
bgpwatcher_view_iter_deactivate_peer(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_has_more_peer(iter))
    {
      if(kh_val(iter->view->peerinfo, iter->peer_it).state == BGPWATCHER_VIEW_FIELD_ACTIVE)
        {
          bgpwatcher_view_iter_t *inn_it;
          inn_it = bgpwatcher_view_iter_create(iter->view);
          for(bgpwatcher_view_iter_first_pfx_peer(inn_it, BGPSTREAM_ADDR_VERSION_IPV4,
                                                  BGPWATCHER_VIEW_FIELD_ACTIVE,
                                                  BGPWATCHER_VIEW_FIELD_ACTIVE);
              bgpwatcher_view_iter_has_more_pfx_peer(inn_it);
              bgpwatcher_view_iter_next_pfx_peer(inn_it))
            {
              // deactivate all the peer-pfx associated with the peer
              if(bgpwatcher_view_iter_peer_get_peer(inn_it) == bgpwatcher_view_iter_peer_get_peer(iter))
                {
                  bgpwatcher_view_iter_pfx_deactivate_peer(inn_it);
                }
            }
          // reduce the number of active peers
          iter->view->peerinfo_cnt--;
          kh_val(iter->view->peerinfo, iter->peer_it).state = BGPWATCHER_VIEW_FIELD_INACTIVE;
          return 1;
        }
      // if already inactive
      return 0;
    }
  return -1;
}

bgpstream_peer_id_t 
bgpwatcher_view_iter_peer_get_peer(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_has_more_peer(iter))
    {
      return kh_key(iter->view->peerinfo, iter->peer_it);
    }
  return 0;
}

bgpstream_peer_sig_t * 
bgpwatcher_view_iter_peer_get_sign(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_has_more_peer(iter))
    {
      return bgpstream_peer_sig_map_get_sig(iter->view->peersigns,
                                            bgpwatcher_view_iter_peer_get_peer(iter));
    }
  return NULL;
}


int
bgpwatcher_view_iter_peer_get_pfx_count(bgpwatcher_view_iter_t *iter,
                                        int version)
{
  if(bgpwatcher_view_iter_has_more_peer(iter))
    {  
      if(version == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          return kh_value(iter->view->peerinfo, iter->peer_it).v4_pfx_cnt;
        }
      if(version == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          return kh_value(iter->view->peerinfo, iter->peer_it).v6_pfx_cnt;
        }
      if(version == 0)
        {
          return kh_value(iter->view->peerinfo, iter->peer_it).v4_pfx_cnt +
            kh_value(iter->view->peerinfo, iter->peer_it).v6_pfx_cnt;
        }
    }
  return -1;
}

bgpwatcher_view_field_state_t
bgpwatcher_view_iter_peer_get_state(bgpwatcher_view_iter_t *iter)
{
 if(bgpwatcher_view_iter_has_more_peer(iter))
    {
      return kh_val(iter->view->peerinfo, iter->peer_it).state;
    }
  return BGPWATCHER_VIEW_FIELD_INVALID;
}

void *
bgpwatcher_view_iter_peer_get_user(bgpwatcher_view_iter_t *iter)
{
   if(bgpwatcher_view_iter_has_more_peer(iter))
    {
      return kh_val(iter->view->peerinfo, iter->peer_it).user;
    }
  return NULL;
}

int
bgpwatcher_view_iter_peer_set_user(bgpwatcher_view_iter_t *iter, void *user)
{
 if(bgpwatcher_view_iter_has_more_peer(iter))
    {
      if(kh_val(iter->view->peerinfo, iter->peer_it).user == user)
        {
          return 0;
        }          
      if(kh_val(iter->view->peerinfo, iter->peer_it).user != NULL &&
         iter->view->peer_user_destructor != NULL)
        {
          iter->view->peer_user_destructor(kh_val(iter->view->peerinfo, iter->peer_it).user);              
        }
      kh_val(iter->view->peerinfo, iter->peer_it).user = user;              
      return 1;
    }
  return -1;
}

int
bgpwatcher_view_iter_pfx_activate_peer(bgpwatcher_view_iter_t *iter)
{
  bgpwatcher_view_field_state_t *st;
  uint32_t *num_pfxs_ptr;
  uint16_t *num_peers_ptr;

  if(bgpwatcher_view_iter_pfx_has_more_peer(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          st = &kh_val(iter->view->v4pfxs, iter->v4pfx_it)
               ->peers[iter->v4pfx_peer_it].state;
          num_peers_ptr = &kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_cnt;
          num_pfxs_ptr = &kh_value(iter->view->peerinfo, iter->peer_it).v4_pfx_cnt;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          st = &kh_val(iter->view->v6pfxs, iter->v6pfx_it)
               ->peers[iter->v6pfx_peer_it].state;
          num_peers_ptr = &kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_cnt;
          num_pfxs_ptr = &kh_value(iter->view->peerinfo, iter->peer_it).v6_pfx_cnt;
        }
      if(*st == BGPWATCHER_VIEW_FIELD_INACTIVE)
        {
          *st = BGPWATCHER_VIEW_FIELD_ACTIVE;
          // increment the number of peers that observe the prefix
          *num_peers_ptr = *num_peers_ptr + 1;
          bgpwatcher_view_iter_activate_pfx(iter);
          // increment the number of prefixes observed by the peer
          *num_pfxs_ptr = *num_pfxs_ptr + 1;
          bgpwatcher_view_iter_activate_peer(iter);
          return 1;
        }
      // if already active
      return 0;
    }
 return -1;
}

int
bgpwatcher_view_iter_pfx_deactivate_peer(bgpwatcher_view_iter_t *iter)
{
  bgpwatcher_view_field_state_t *st;
  uint32_t *num_pfxs_ptr;
  uint16_t *num_peers_ptr;
  if(bgpwatcher_view_iter_pfx_has_more_peer(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          st = &kh_val(iter->view->v4pfxs, iter->v4pfx_it)
               ->peers[iter->v4pfx_peer_it].state;
          num_peers_ptr = &kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_cnt;
          num_pfxs_ptr = &kh_value(iter->view->peerinfo, iter->peer_it).v4_pfx_cnt;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          st = &kh_val(iter->view->v6pfxs, iter->v6pfx_it)
               ->peers[iter->v6pfx_peer_it].state;
          num_peers_ptr = &kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_cnt;
          num_pfxs_ptr = &kh_value(iter->view->peerinfo, iter->peer_it).v6_pfx_cnt;
        }
      if(*st == BGPWATCHER_VIEW_FIELD_ACTIVE)
        {
          *st = BGPWATCHER_VIEW_FIELD_INACTIVE;
          // decrease prefix counters (for the current peer)
          *num_pfxs_ptr = *num_pfxs_ptr - 1;
          // decrease peers counter (for the current prefix)
          *num_peers_ptr = *num_peers_ptr - 1;
          if(*num_peers_ptr == 0)
            {
              bgpwatcher_view_iter_deactivate_pfx(iter);
            }
          return 1;
        }
      // if already active
      return 0;
    }
 return -1;

}


int
bgpwatcher_view_iter_pfx_peer_get_orig_asn(bgpwatcher_view_iter_t *iter)
{
 if(bgpwatcher_view_iter_pfx_has_more_peer(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          return kh_val(iter->view->v4pfxs, iter->v4pfx_it)
               ->peers[iter->v4pfx_peer_it].orig_asn;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          return kh_val(iter->view->v6pfxs, iter->v6pfx_it)
               ->peers[iter->v6pfx_peer_it].orig_asn;
        }
    }
  return -1;
}

bgpwatcher_view_field_state_t
bgpwatcher_view_iter_pfx_peer_get_state(bgpwatcher_view_iter_t *iter)
{
 if(bgpwatcher_view_iter_pfx_has_more_peer(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          return kh_val(iter->view->v4pfxs, iter->v4pfx_it)
               ->peers[iter->v4pfx_peer_it].state;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          return kh_val(iter->view->v6pfxs, iter->v6pfx_it)
               ->peers[iter->v6pfx_peer_it].state;
        }
    }
  return BGPWATCHER_VIEW_FIELD_INVALID;
}

void * 
bgpwatcher_view_iter_pfx_peer_get_user(bgpwatcher_view_iter_t *iter)
{
 if(bgpwatcher_view_iter_pfx_has_more_peer(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {
          return kh_val(iter->view->v4pfxs, iter->v4pfx_it)
               ->peers[iter->v4pfx_peer_it].user;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {
          return kh_val(iter->view->v6pfxs, iter->v6pfx_it)
               ->peers[iter->v6pfx_peer_it].user;
        }
    }
  return NULL;
}

int
bgpwatcher_view_iter_pfx_peer_set_user(bgpwatcher_view_iter_t *iter, void *user)
{
 if(bgpwatcher_view_iter_pfx_has_more_peer(iter))
    {
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV4)
        {      
          if(kh_val(iter->view->v4pfxs, iter->v4pfx_it)
             ->peers[iter->v4pfx_peer_it].user == user)
            {
              return 0;
            }
            
          if(kh_val(iter->view->v4pfxs, iter->v4pfx_it)
             ->peers[iter->v4pfx_peer_it].user != NULL &&
             iter->view->pfx_peer_user_destructor != NULL)
            {
              iter->view->pfx_peer_user_destructor(kh_val(iter->view->v4pfxs, iter->v4pfx_it)
                                                   ->peers[iter->v4pfx_peer_it].user);              
            }
          kh_val(iter->view->v4pfxs, iter->v4pfx_it)
            ->peers[iter->v4pfx_peer_it].user = user;              
          return 1;
        }
      if(iter->version_ptr == BGPSTREAM_ADDR_VERSION_IPV6)
        {      
          if(kh_val(iter->view->v6pfxs, iter->v6pfx_it)
             ->peers[iter->v6pfx_peer_it].user == user)
            {
              return 0;
            }
            
          if(kh_val(iter->view->v6pfxs, iter->v6pfx_it)
             ->peers[iter->v6pfx_peer_it].user != NULL &&
             iter->view->pfx_peer_user_destructor != NULL)
            {
              iter->view->pfx_peer_user_destructor(kh_val(iter->view->v6pfxs, iter->v6pfx_it)
                                                   ->peers[iter->v6pfx_peer_it].user);              
            }
          kh_val(iter->view->v6pfxs, iter->v6pfx_it)
            ->peers[iter->v6pfx_peer_it].user = user;              
          return 1;
        }
    }
 return -1;
}


