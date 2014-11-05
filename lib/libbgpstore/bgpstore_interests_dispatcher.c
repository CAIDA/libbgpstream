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


#include "bgpstore_interests_dispatcher.h"
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


/** Dispatcher structures and functions */

#define DISPATCH_TO_NONE      0b00000000
#define DISPATCH_TO_PARTIAL   0b00000010
#define DISPATCH_TO_FULL      0b00000100
#define DISPATCH_TO_FIRSTFULL 0b00001000


typedef struct struct_bgpstore_interests_dispatcher {

  // TODO: a structure for every possible interest
  bgpviewstatus_interest_t *bvstatus;
  perasvisibility_interest_t *peras_vis;

  // TODO: documentation
  uint8_t sendto_mask;

} bgpstore_interests_dispatcher_t;



static bgpstore_interests_dispatcher_t *bgpstore_interests_dispatcher_create()
{
  bgpstore_interests_dispatcher_t *bid = (bgpstore_interests_dispatcher_t *)malloc_zero(sizeof(bgpstore_interests_dispatcher_t));
  // interests are created on demand
  bid->bvstatus = NULL;
  bid->peras_vis = NULL;
  bid->sendto_mask = DISPATCH_TO_NONE;
  return bid;
}


static int bgpstore_interests_dispatcher_send(bgpstore_interests_dispatcher_t *bid, char *client)
{
  // TODO: send interests related to client
  return 0;
}


static void bgpstore_interests_dispatcher_destroy(bgpstore_interests_dispatcher_t *bid)
{
  if(bid != NULL)
    {
      if(bid->bvstatus != NULL)
	{
	  bgpviewstatus_interest_destroy(bid->bvstatus);
	}
      bid->bvstatus = NULL;
      if(bid->peras_vis != NULL)
	{
	  perasvisibility_interest_destroy(bid->peras_vis);
	}
      bid->peras_vis = NULL;
      free(bid);
    }
}


int bgpstore_interests_dispatcher_run(clientinfo_map_t *active_clients,
				      bgpview_t *bgp_view, uint32_t ts) {

  
  // create an empty interests dispatcher structure
  bgpstore_interests_dispatcher_t *bid = bgpstore_interests_dispatcher_create();
  if(bid == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize bgpstore interests dispatcher\n");
      return -1;
    }

  // 1. verify what macro interests can be satisfied
  
  // check if it satisfies PARTIAL VIEW consumers
  if( (bgp_view->state == BGPVIEW_PARTIAL || bgp_view->state == BGPVIEW_FULL) &&
      bgp_view->dis_status[BGPVIEW_PARTIAL].modified != 0 )
    {
      // send to partial consumers
      bid->sendto_mask = bid->sendto_mask | DISPATCH_TO_PARTIAL;
      bgp_view->dis_status[BGPVIEW_PARTIAL].modified = 0;
      bgp_view->dis_status[BGPVIEW_PARTIAL].sent = 1;
    }

  // check if it satisfies FULL VIEW consumers
  if( bgp_view->state == BGPVIEW_FULL &&
      bgp_view->dis_status[BGPVIEW_FULL].modified != 0 )
    {
      if(bgp_view->dis_status[BGPVIEW_FULL].sent == 0)
	{
	  // send to first full consumers
	  bid->sendto_mask = bid->sendto_mask | DISPATCH_TO_FIRSTFULL;
	}
      // send to full consumers
      bid->sendto_mask = bid->sendto_mask | DISPATCH_TO_FULL;
      bgp_view->dis_status[BGPVIEW_FULL].modified = 0;
      bgp_view->dis_status[BGPVIEW_FULL].sent = 1;
    }

  if(bid->sendto_mask == DISPATCH_TO_NONE)
    {
      // no client can be satisfied
      return 0;
    }


  // TODO: remove later, dispatch to FULL only
  if(! (bid->sendto_mask & DISPATCH_TO_FULL))
    {
      return 0;
    }
  

  // 2. satisfy consumer interests (specific)
   
  khiter_t k;
  clientstatus_t *cl_status;
  char *client_name;

  
  // for each active client:
  for (k = kh_begin(active_clients); k != kh_end(active_clients); ++k)
    {
      if (kh_exist(active_clients, k))
	{
	  client_name = kh_key(active_clients,k);
	  cl_status = &(kh_value(active_clients,k));

	  // one if for each interest
	  if(cl_status->consumer_interests & BGPWATCHER_CONSUMER_INTEREST_BGPVIEWSTATUS)
	    {
	      if(bid->bvstatus == NULL)
		{
		  bid->bvstatus = bgpviewstatus_interest_create(bgp_view, ts);
		  if(bid->bvstatus == NULL)
		    {
		      fprintf(stderr, "ERROR: could not create bgpstore bgpviewstatus interest\n");
		      return -1;
		    }
		}
	      bgpviewstatus_interest_send(bid->bvstatus, client_name);
	    }
	    
	  if(cl_status->consumer_interests & BGPWATCHER_CONSUMER_INTEREST_ASVISIBILITY)
	    {
	      if(bid->peras_vis == NULL)
		{
		  bid->peras_vis = perasvisibility_interest_create(bgp_view, ts);
		  if(bid->peras_vis == NULL)
		    {
		      fprintf(stderr, "ERROR: could not create bgpstore AS visibility interest\n");
		      return -1;
		    }
		}
	      perasvisibility_interest_send(bid->peras_vis, client_name);
	    }
	    
	  // TODO: satisfy other interests	  
	  // bgpstore_interests_dispatcher_send(bid, client_name);

	}
    }

  // destroy the interests dispatcher structure
  bgpstore_interests_dispatcher_destroy(bid);

  return 0;
}
