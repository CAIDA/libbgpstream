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
      // allocate new memory for the string
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
  // check if it exists
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



int bgpstore_prefix_table_begin(bgpstore_t *bgp_store, 
				bgpwatcher_pfx_table_t *table)
{
  
  bgpview_t *bgp_view = NULL;
  khiter_t k;
  int khret;

  // insert new bgp_view if time does not exist yet
  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries,
		 table->time)) == kh_end(bgp_store->bgp_timeseries))
    {
      // first time we receive this table time -> create bgp_view
      if((bgp_view = bgpview_create()) == NULL)
	{
	  // TODO: comment
	  return -1;
	}
      k = kh_put(timebgpview, bgp_store->bgp_timeseries, table->time, &khret);
      kh_value(bgp_store->bgp_timeseries,k) = bgp_view;
    }

  // retrieve pointer to the correct bgpview
  bgp_view = kh_value(bgp_store->bgp_timeseries,k);

  // get the list of peers associated with current pfx table
  
  int remote_peer_id; // id assigned to (collector,peer) by remote process

  bgpwatcher_peer_t* peer_info;
  for(remote_peer_id = 0; remote_peer_id < table->peers_cnt; remote_peer_id++)
    {
      // get address to peer_info structure in current table
      peer_info = &(table->peers[remote_peer_id]);      
      // set "static" (server) id assigned to (collector,peer) by current process
      peer_info->server_id = bl_peersign_map_set_and_get(bgp_store->peer_signature_id,
							 table->collector, &(peer_info->ip));
      // send peer info to the appropriate bgp view
      if(bgpview_add_peer(bgp_view, peer_info) < 0)
	{
	  // TODO: comment
	  return -1;
	}
    }  
  return 0;
}


int bgpstore_prefix_table_row(bgpstore_t *bgp_store, bgpwatcher_pfx_table_t *table, bgpwatcher_pfx_row_t *row)
{
  khiter_t k;

  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries,
		 table->time)) == kh_end(bgp_store->bgp_timeseries))
    {
      // view for this time must exist
      return -1;
    }
  
  bgpview_t * bgp_view = kh_value(bgp_store->bgp_timeseries,k);
  
  return bgpview_add_row(bgp_view, table, row);
}


int bgpstore_prefix_table_end(bgpstore_t *bgp_store, char *client_name,
			      bgpwatcher_pfx_table_t *table)
{
  khiter_t k;

  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries,
		 table->time)) == kh_end(bgp_store->bgp_timeseries))
    {
      // view for this time must exist
      return -1;
    }
  
  bgpview_t * bgp_view = kh_value(bgp_store->bgp_timeseries,k);
  
  return bgpview_table_end(bgp_view, client_name, table);
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

