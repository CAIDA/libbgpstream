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


#include "bgpstore_interests.h"
#include "bgpstore_common.h"

#include "bgpwatcher_common.h" // interests masks

#include "utils.h"

#include "bl_bgp_utils.h"
#include "bl_str_set.h"
#include "bl_id_set.h"
#include "bl_pfx_set.h"

#include "khash.h"
#include <assert.h>


#define METRIC_PREFIX "bgp.visibility"

// AS -> prefix set

KHASH_INIT(as_visibility /* name */, 
	   uint32_t /* khkey_t */, 
	   bl_ipv4_pfx_set_t* /* khval_t */, 
	   1  /* kh_is_set */,
	   kh_int_hash_func /*__hash_func */,  
	   kh_int_hash_equal /* __hash_equal */);

typedef khash_t(as_visibility) as_visibility_t;


struct perasvisibility_interest {

  /** timestamp */
  uint32_t ts;
  
  /** Set of peers that are complete (i.e. no more
   *  pfx tables expected) and satisfy the full feed
   *  requirements (i.e. #ipv4 prefixes > 500K, 
   *  #ipv6 prefixes > 10K ~ TODO check! )
   */
  bl_id_set_t *eligible_peers;
  /** for each AS we store the unique set of prefixes
   *  that are originated by this AS */
  as_visibility_t *as_vis_map;
  
    // TODO: add other structures??
};



static void as_visibility_insert(as_visibility_t *as_vis_map, uint32_t asn, bl_ipv4_pfx_t *pfx)
{
  int khret;
  khiter_t k;
  bl_ipv4_pfx_set_t* pfx_set;
  if((k = kh_get(as_visibility, as_vis_map, asn)) == kh_end(as_vis_map))
    {
      k = kh_put(as_visibility, as_vis_map, asn, &khret);
      kh_value(as_vis_map,k) = bl_ipv4_pfx_set_create();
    }
  pfx_set = kh_value(as_vis_map,k);
  if(pfx_set == NULL){
    // TODO: error
  }
  bl_ipv4_pfx_set_insert(pfx_set, *pfx);
}




perasvisibility_interest_t* perasvisibility_interest_create(bgpview_t *bgp_view,
							    uint32_t ts)
{
  perasvisibility_interest_t *peras_vis = (perasvisibility_interest_t *) malloc_zero(sizeof(perasvisibility_interest_t));

  // create internal structures
  if((peras_vis->eligible_peers = bl_id_set_create()) == NULL)
    {
      fprintf(stderr, "Error: unable to create the eligible_peers set\n");
      goto err;
    }
  
  if((peras_vis->as_vis_map = kh_init(as_visibility)) == NULL)
    {
      fprintf(stderr, "Error: unable to create the as visibility map\n");
      goto err;
    }

  peras_vis->ts = ts;
  
  khiter_t k;
  uint16_t i;
  active_peer_status_t *aps;
  
  // 1. we select full feed peers that are complete (i.e. no more pfx table expected)
  for (k = kh_begin(bgp_view->active_peers_info);
       k != kh_end(bgp_view->active_peers_info); ++k)
    {
      if (kh_exist(bgp_view->active_peers_info, k))	
	{
	  i = kh_key(bgp_view->active_peers_info, k);
	  aps = &(kh_value(bgp_view->active_peers_info, k));
	  // check completeness
	  if(aps->expected_pfx_tables_cnt == aps->received_pfx_tables_cnt)
	    {
	      // check full feed
	      if(aps->recived_ipv4_pfx_cnt > 500000 ||
		 aps->recived_ipv6_pfx_cnt > 10000 )
		{
		  bl_id_set_insert(peras_vis->eligible_peers, i);
		}
	    }	  
	}
    }

  
  // 2. go through all ipv4 prefixes, get their origin ASes
  //    and populate the as visibility map
  
  bl_ipv4_pfx_t *pfx;
  peerview_t *pv;
  pfxinfo_t *pi;
  khiter_t k_inn;

  for (k = kh_begin(bgp_view->aggregated_pfxview_ipv4);
       k != kh_end(bgp_view->aggregated_pfxview_ipv4); ++k)
    {
      if (kh_exist(bgp_view->aggregated_pfxview_ipv4, k))	
	{
	  pfx = &(kh_key(bgp_view->aggregated_pfxview_ipv4,k));
	  pv = kh_value(bgp_view->aggregated_pfxview_ipv4,k);
	  // for each id check if it is eligible
	  for (k_inn = kh_begin(pv); k_inn != kh_end(pv); ++k_inn)
	    {
	      if (kh_exist(pv, k_inn))	
		{
		  i = kh_key(pv, k_inn);
		  if(bl_id_set_exists(peras_vis->eligible_peers,i) == 1)
		    {
		      pi = &(kh_value(pv,k_inn));
		      // get origin AS
		      if(pi->orig_asn != 0) // not a special case
			{
			  // insert (AS, ipv4_pfx)
			  as_visibility_insert(peras_vis->as_vis_map, pi->orig_asn, pfx);
			}
		    }
		}
	    }
	}
    }


  return peras_vis;

 err:
  perasvisibility_interest_destroy(peras_vis);
  return NULL;
}



int perasvisibility_interest_send(perasvisibility_interest_t* peras_vis, char *client)
{

  // OUTPUT: number of ipv4 prefixes seen by each AS
  fprintf(stdout,
	  METRIC_PREFIX".full_feed_peers_cnt  %d %"PRIu32"\n",
	  kh_size(peras_vis->eligible_peers),
	  peras_vis->ts);

  // print per AS visibility
  uint32_t asn;
  uint64_t ipv4_pfx_cnt;
  khiter_t k;

  for (k = kh_begin(peras_vis->as_vis_map);
       k != kh_end(peras_vis->as_vis_map); ++k)
    {
      if (kh_exist(peras_vis->as_vis_map, k))	
	{
	  asn = kh_key(peras_vis->as_vis_map,k);
	  ipv4_pfx_cnt = kh_size(kh_value(peras_vis->as_vis_map,k));
	  // OUTPUT: number of ipv4 prefixes seen by each AS
	  fprintf(stdout,
		  METRIC_PREFIX".ipv4.%"PRIu32" %"PRIu64" %"PRIu32"\n",
		  asn,
		  ipv4_pfx_cnt,
		  peras_vis->ts);
	}
    }
  
  return 0;
}


void perasvisibility_interest_destroy(perasvisibility_interest_t* peras_vis)
{
  if(peras_vis !=NULL)
    {
      if(peras_vis->eligible_peers != NULL)
	{
	  bl_id_set_destroy(peras_vis->eligible_peers);
	  peras_vis->eligible_peers = NULL;
	}
      if(peras_vis->as_vis_map != NULL)
	{
	  kh_free_vals(as_visibility, peras_vis->as_vis_map, bl_ipv4_pfx_set_destroy);
	  kh_destroy(as_visibility, peras_vis->as_vis_map);
	  peras_vis->as_vis_map = NULL;
	}      
      free(peras_vis);
    }
}


