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
#include "bgpstore_common.h"

#include "bgpwatcher_common.h" // interests masks

#include "utils.h"

#include "bl_bgp_utils.h"
#include "bl_str_set.h"
#include "bl_id_set.h"

#include "khash.h"
#include <assert.h>


/** Interests structures and functions */


typedef struct struct_bgpviewstatus_interest {
  // TODO
} bgpviewstatus_interest_t;

static bgpviewstatus_interest_t* bgpviewstatus_interest_create(bgpview_t *bgp_view, uint32_t ts)
{
  bgpviewstatus_interest_t *bvstatus = (bgpviewstatus_interest_t *)malloc_zero(sizeof(bgpviewstatus_interest_t));
  return bvstatus;
}

static int bgpviewstatus_interest_send(char *client_name, bgpviewstatus_interest_t* bvstatus)
{
  // TODO
  fprintf(stderr, "Sending BGPVIEWSTATUS to client: %s\n", client_name);
  return 0;
}

static void bgpviewstatus_interest_destroy(bgpviewstatus_interest_t* bvstatus)
{
  if(bvstatus !=NULL)
    {
      free(bvstatus);
    }
}



typedef struct struct_bgpstore_interests_dispatcher {

  // TODO: a structure for every possible interest
  bgpviewstatus_interest_t *bvstatus;

} bgpstore_interests_dispatcher_t;



static bgpstore_interests_dispatcher_t *bgpstore_interests_dispatcher_create()
{
  bgpstore_interests_dispatcher_t *bid = (bgpstore_interests_dispatcher_t *)malloc_zero(sizeof(bgpstore_interests_dispatcher_t));
  // interests are created on demand
  bid->bvstatus = NULL;
  return bid;
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
      free(bid);
    }
}



int bgpstore_interests_dispatcher_run(clientinfo_map_t *active_clients, bgpview_t *bgp_view, uint32_t ts) {

  // create an empty interests dispatcher structure
  bgpstore_interests_dispatcher_t *bid = bgpstore_interests_dispatcher_create();
  if(bid == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize bgpstore interests dispatcher\n");
      return -1;
    }
  
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
	      bgpviewstatus_interest_send(client_name, bid->bvstatus);
	    }
	}
    }

  // destroy the interests dispatcher structure
  bgpstore_interests_dispatcher_destroy(bid);

  return 0;
}
