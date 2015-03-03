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
      kh_val(view->peerinfo, k).in_use = 0;
      kh_val(view->peerinfo, k).user = NULL;
    }
  
  // activate/init peer
  if(kh_val(view->peerinfo, k).in_use == 0)
    {
      kh_val(view->peerinfo, k).v4_pfx_cnt = 0;
      kh_val(view->peerinfo, k).v6_pfx_cnt = 0;
      kh_val(view->peerinfo, k).in_use = 1;
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
          v->peers[i].in_use = 0;
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
          v->peers[i].in_use = 0;
        }

      v->peers_alloc_cnt = peerid+1;
    }

  /* if this peer was not previously used, we need to count it */
  if(v->peers[peerid].in_use == 0)
    {
      v->peers_cnt++;

      /* and we need to add this prefix to the peerinfo counter */
      peerinfo_add_pfx(view, peerid, prefix);
    }

  v->peers[peerid] = *pfx_info;
  v->peers[peerid].in_use = 1;
  v->peers[peerid].user = NULL;
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
      for(i = 0; i< v->peers_alloc_cnt; i++)
        {
          pfx_peer_info_destroy(view, &v->peers[i]);
        }
        free(v->peers);
    }
  v->peers = NULL;
  v->peers_cnt = 0;
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
                                             bgpstream_pfx_storage_t *prefix)
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
	}
    }
  view->v4pfxs_cnt = 0;

  /* mark all ipv6 prefixes as unused */
  for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); ++k)
    {
      if(kh_exist(view->v6pfxs, k))
	{
	  kh_value(view->v6pfxs, k)->peers_cnt = 0;
	}
    }
  view->v6pfxs_cnt = 0;

  /* clear out the peerinfo table */
  for(k = kh_begin(view->peerinfo); k != kh_end(view->peerinfo); ++k)
    {
      if(kh_exist(view->peerinfo, k))
	{
	  kh_value(view->peerinfo, k).in_use = 0;
	}
    }
  view->peerinfo_cnt = 0;


  view->pub_cnt = 0;
}

int bgpwatcher_view_add_prefix(bgpwatcher_view_t *view,
                               bgpstream_pfx_storage_t *prefix,
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

  if(peerid_pfxinfo_insert(view, (bgpstream_pfx_t *)prefix,
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

// TODO REMOVE!
/* void bgpwatcher_view_destroy_user(bgpwatcher_view_t *view, */
/* 				  bgpwatcher_view_destroy_user_cb *call_back) */
/* { */
/*   khiter_t k; */
/*   assert(call_back != NULL); */
/*   for(k = kh_begin(view->v4pfxs); k != kh_end(view->v4pfxs); k++) */
/*     { */
/*       if(kh_exist(view->v4pfxs,k)) */
/* 	{ */
/* 	  call_back(kh_val(view->v4pfxs,k)->user); */
/* 	  kh_val(view->v4pfxs,k)->user = NULL; */
/* 	} */
/*     } */
/*   for(k = kh_begin(view->v6pfxs); k != kh_end(view->v6pfxs); k++) */
/*     { */
/*       if(kh_exist(view->v6pfxs,k)) */
/* 	{ */
/* 	  call_back(kh_val(view->v6pfxs,k)->user); */
/* 	  kh_val(view->v6pfxs,k)->user = NULL; */
/* 	} */
/*     }  */
/* } */


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
	    (!kh_exist(iter->view->peerinfo, iter->peer_it) ||
             !kh_val(iter->view->peerinfo, iter->peer_it).in_use))
	{
	  iter->peer_it++;
	}
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER:
      assert(iter->v4pfx_it != kh_end(iter->view->v4pfxs));
      iter->v4pfx_peer_it = 0;
      iter->v4pfx_peer_it_valid = 1;

      while((iter->v4pfx_peer_it <
             kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt) &&
	    (!kh_val(iter->view->v4pfxs, iter->v4pfx_it)
             ->peers[iter->v4pfx_peer_it].in_use))
	{
	  iter->v4pfx_peer_it++;
	}
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER:
      assert(iter->v6pfx_it != kh_end(iter->view->v6pfxs));
      iter->v6pfx_peer_it = 0;
      iter->v6pfx_peer_it_valid = 1;

      while((iter->v6pfx_peer_it <
             kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt) &&
	    (!kh_val(iter->view->v6pfxs, iter->v6pfx_it)
             ->peers[iter->v6pfx_peer_it].in_use))
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
	 >= kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt)
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
	 >= kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt)
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
              (!kh_exist(iter->view->peerinfo, iter->peer_it) ||
               !kh_val(iter->view->peerinfo, iter->peer_it).in_use));
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER:
      /* increment the iterator until:
	 1. we reach the end of the hash, or
	 2. (we find a peer valid in the hash, and
	 3.  we find a peer valid in the view)
      */
      do {
	iter->v4pfx_peer_it++;
      } while(iter->v4pfx_peer_it <
	      kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers_alloc_cnt &&
	      (!kh_val(iter->view->v4pfxs, iter->v4pfx_it)
               ->peers[iter->v4pfx_peer_it].in_use));
      break;

    case BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER:
      /* increment the iterator until:
	 1. we reach the end of the hash, or
	 2. (we find a peer valid in the hash, and
	 3.  we find a peer valid in the view)
      */
      do {
	iter->v6pfx_peer_it++;
      } while(iter->v6pfx_peer_it <
	      kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers_alloc_cnt &&
	      (!kh_val(iter->view->v6pfxs, iter->v6pfx_it)
               ->peers[iter->v6pfx_peer_it].in_use));
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

bgpstream_ipv4_pfx_t *bgpwatcher_view_iter_get_v4pfx(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {
      return NULL;
    }
  return &kh_key(iter->view->v4pfxs, iter->v4pfx_it);
}

bgpstream_ipv6_pfx_t *bgpwatcher_view_iter_get_v6pfx(bgpwatcher_view_iter_t *iter)
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

int bgpwatcher_view_iter_set_v4pfx_user(bgpwatcher_view_iter_t *iter, void *user)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {
      return -1;
    }
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

int bgpwatcher_view_iter_set_v6pfx_user(bgpwatcher_view_iter_t *iter, void *user)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX))
    {
      return -1;
    }
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

void bgpwatcher_view_set_pfx_user_destructor(bgpwatcher_view_t *view,
                                             bgpwatcher_view_destroy_user_t *bwv_pfx_user_destructor)
{
  view->pfx_user_destructor = bwv_pfx_user_destructor;
}

bgpstream_peer_id_t
bgpwatcher_view_iter_get_peerid(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      return 0;
    }
  return kh_key(iter->view->peerinfo, iter->peer_it);
}

bgpstream_peer_sig_t *
bgpwatcher_view_iter_get_peersig(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      return NULL;
    }
  return bgpstream_peer_sig_map_get_sig(iter->view->peersigns,
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

void *bgpwatcher_view_iter_get_peer_user(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      return NULL;
    }

  return kh_val(iter->view->peerinfo, iter->peer_it).user;
}

int bgpwatcher_view_iter_set_peer_user(bgpwatcher_view_iter_t *iter, void *user)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      return -1;
    }

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

void bgpwatcher_view_set_peer_user_destructor(bgpwatcher_view_t *view,
                                              bgpwatcher_view_destroy_user_t *bwv_peer_user_destructor)
{
  view->peer_user_destructor = bwv_peer_user_destructor;
}


bgpstream_peer_id_t
bgpwatcher_view_iter_get_v4pfx_peerid(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
    {
      return 0;
    }

  return iter->v4pfx_peer_it;
}

bgpstream_peer_id_t
bgpwatcher_view_iter_get_v6pfx_peerid(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
    {
      return 0;
    }

  return iter->v6pfx_peer_it;
}

bgpstream_peer_sig_t *
bgpwatcher_view_iter_get_v4pfx_peersig(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
    {
      return NULL;
    }

  return bgpstream_peer_sig_map_get_sig(iter->view->peersigns,
                                        bgpwatcher_view_iter_get_v4pfx_peerid(iter));
}

bgpstream_peer_sig_t *
bgpwatcher_view_iter_get_v6pfx_peersig(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
    {
      return NULL;
    }

  return bgpstream_peer_sig_map_get_sig(iter->view->peersigns,
                                        bgpwatcher_view_iter_get_v6pfx_peerid(iter));
}

bgpwatcher_pfx_peer_info_t *
bgpwatcher_view_iter_get_v4pfx_pfxinfo(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
    {
      return NULL;
    }

  return &kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers[iter->v4pfx_peer_it];
}

bgpwatcher_pfx_peer_info_t *
bgpwatcher_view_iter_get_v6pfx_pfxinfo(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
    {
      return NULL;
    }

  return &kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers[iter->v6pfx_peer_it];
}

void *
bgpwatcher_view_iter_get_v4pfx_pfxinfo_user(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
    {
      return NULL;
    }
  return kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers[iter->v4pfx_peer_it].user;
}

void *
bgpwatcher_view_iter_get_v6pfx_pfxinfo_user(bgpwatcher_view_iter_t *iter)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
    {
      return NULL;
    }
  return kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers[iter->v6pfx_peer_it].user;
}

int
bgpwatcher_view_iter_set_v4pfx_pfxinfo_user(bgpwatcher_view_iter_t *iter, void *user)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
    {
      return -1;
    }
  void *cur_user = kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers[iter->v4pfx_peer_it].user;
  if(cur_user == user)
    {
      return 0;
    }
  if(cur_user != NULL &&
     iter->view->pfx_peer_user_destructor != NULL)
    {
      iter->view->pfx_peer_user_destructor(cur_user);
    } 
  kh_val(iter->view->v4pfxs, iter->v4pfx_it)->peers[iter->v4pfx_peer_it].user = user;
  return 1;
}

int
bgpwatcher_view_iter_set_v6pfx_pfxinfo_user(bgpwatcher_view_iter_t *iter, void *user)
{
  if(bgpwatcher_view_iter_is_end(iter, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
    {
      return -1;
    }
  void *cur_user = kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers[iter->v6pfx_peer_it].user;
  if(cur_user == user)
    {
      return 0;
    }
  if(cur_user != NULL &&
     iter->view->pfx_peer_user_destructor != NULL)
    {
      iter->view->pfx_peer_user_destructor(cur_user);
    } 
  kh_val(iter->view->v6pfxs, iter->v6pfx_it)->peers[iter->v6pfx_peer_it].user = user;
  return 1;
}

void
bgpwatcher_view_set_pfx_peer_user_destructor(bgpwatcher_view_t *view,
                                               bgpwatcher_view_destroy_user_t *bwv_pfx_peer_user_destructor)
{
  view->pfx_peer_user_destructor = bwv_pfx_peer_user_destructor;
}

