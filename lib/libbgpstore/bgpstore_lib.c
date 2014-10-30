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

  if((bgp_store->peer_signature_id = bl_peersign_map_create()) == NULL)
    {
      fprintf(stderr, "Failed to create (peer_signature_id)\n");
      goto err;
    }

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



int bgpstore_client_connect(bgpstore_t *bgp_store, char *client_name,
			    uint8_t client_interests, uint8_t client_intents)
{  
  khiter_t k;
  int khret;

  char *client_name_cpy;
  clientstatus_t client_info;
  client_info.consumer_interests = client_interests;
  client_info.producer_intents = client_intents;

  // check if it does not exist
  if((k = kh_get(strclientstatus, bgp_store->active_clients,
		client_name)) == kh_end(bgp_store->active_clients))
    {  
      // in case it doesn't allocate new memory for the string
      if((client_name_cpy = strdup(client_name)) == NULL)
	{
	  return -1;
	}
      // put key in table
      k = kh_put(strclientstatus, bgp_store->active_clients,
		 client_name_cpy, &khret);
    }

  // update or insert new client info
  kh_value(bgp_store->active_clients, k) = client_info;

  return 0;
}



int bgpstore_client_disconnect(bgpstore_t *bgp_store, char *client_name)
{
  khiter_t k;
  // check if it does not exist
  if((k = kh_get(strclientstatus, bgp_store->active_clients,
		client_name)) != kh_end(bgp_store->active_clients))
    {
      // free memory allocated for the key (string)
      free(kh_key(bgp_store->active_clients,k));
      // delete entry
      kh_del(strclientstatus,bgp_store->active_clients,k);
    }
  return 0;
}


int bgpstore_some_table_start(bgpstore_t *bgp_store, char *client_name,
			      uint32_t table_time, char *collector_str,
			      bl_addr_storage_t *peer_ip)
{
  bl_peersign_map_set_and_get(bgp_store->peer_signature_id, collector_str, peer_ip);
  // TODO!
  khiter_t k;
  bgpview_t *bgp_view = NULL;
  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries, table_time)) == kh_end(bgp_store->bgp_timeseries))
    {
      // first time we receive this table time -> create bgp_view
      if((bgp_view = bgpview_create()) == NULL)
	{
	  return -1;
	}
      // k = kh_put(timebgpview, bgp_store->bgp_timeseries, table_time,&khret);

      // HERE
      
    }

  return 0;
}

int bgpstore_some_table_end(bgpstore_t *bgp_store, char *client_name,
			    uint32_t table_time, char *collector_str,
			    bl_addr_storage_t *peer_ip)
{
  khiter_t k;
  bgpview_t *bgp_view = NULL;
  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries, table_time)) != kh_end(bgp_store->bgp_timeseries))
    {
      if (kh_exist(bgp_store->bgp_timeseries, k))
	{
	  bgp_view = kh_value(bgp_store->bgp_timeseries,k);
	  
	  return 0;

	}
    }

  // TODO: error, receiving a table end for a time that has never been
  // processed
  return -1;    
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
      if(bgp_store->peer_signature_id != NULL)
	{
	  bl_peersign_map_destroy(bgp_store->peer_signature_id);
	  bgp_store->peer_signature_id = NULL;
	}
      free(bgp_store);
#ifdef DEBUG
      fprintf(stderr, "DEBUG: bgpstore destroyed\n");
#endif
    }
}

