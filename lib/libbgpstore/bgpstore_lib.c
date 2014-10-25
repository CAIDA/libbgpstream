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


#include "bgpstore_int.h"


bgpstore_t *bgpstore_create()
{
  bgpstore_t *bgp_store;
  // allocate memory for the structure
  if((bgp_store = malloc_zero(sizeof(bgpstore_t))) == NULL)
    {
      return NULL;
    }

  if((bgp_store->bgp_timeseries = kh_init(timebgpview)) == NULL)
    {
      fprintf(stderr, "Failed to create bgp_timeseries\n");
      goto err;
    }

  if((bgp_store->active_clients = kh_init(strclientstatus)) == NULL)
    {
      fprintf(stderr, "Failed to create active_clients\n");
      goto err;
    }

  if((bgp_store->collectorpeer_bsid = kh_init(collectorpeeridtable)) == NULL)
    {
      fprintf(stderr, "Failed to create (collectorpeeridtable)\n");
      goto err;
    }

  if((bgp_store->bsid_collectorpeer = kh_init(bsidtable)) == NULL)
    {
      fprintf(stderr, "Failed to create (collectorsidtable)\n");
      goto err;
    }

  bgp_store->next_bs_id = 0;

#ifdef DEBUG
  fprintf(stderr, "DEBUG: bgpstore created\n");
#endif
  return bgp_store;

 err:
  if(bgp_store != NULL)
    {
      bgpstore_destroy(bgp_store);
    }
  return NULL;    
}


void bgpstore_destroy(bgpstore_t *bgp_store)
{
  if(bgp_store != NULL)
    {
      if(bgp_store->bgp_timeseries != NULL)
	{
	  kh_free_vals(timebgpview, bgp_store->bgp_timeseries, bgpview_destroy);
	  kh_destroy(timebgpview, bgp_store->bgp_timeseries);
	  bgp_store->bgp_timeseries = NULL;
	}
      if(bgp_store->active_clients != NULL)
	{
	  kh_destroy(strclientstatus, bgp_store->active_clients);
	  bgp_store->active_clients = NULL;
	}
      
      free(bgp_store);
#ifdef DEBUG
      fprintf(stderr, "DEBUG: bgpstore destroyed\n");
#endif
    }
}

