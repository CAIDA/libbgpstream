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
#include "bgpstore_interests_dispatcher.h"


bgpstore_t *bgpstore_create()
{
  bgpstore_t *bgp_store;
  // allocate memory for the structure
  if((bgp_store = malloc_zero(sizeof(bgpstore_t))) == NULL)
    {
      return NULL;
    }
  
  bgp_store->min_ts = 0;
  
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

  bgpview_t *bgp_view;
  uint32_t ts;
  for (k = kh_begin(bgp_store->bgp_timeseries); k != kh_end(bgp_store->bgp_timeseries); ++k)
    {
      if (kh_exist(bgp_store->bgp_timeseries, k))
	{
	  ts = kh_key(bgp_store->bgp_timeseries, k);
	  bgp_view = kh_value(bgp_store->bgp_timeseries, k);
	  bgpstore_completion_check(bgp_store, bgp_view, ts, BGPSTORE_CLIENT_DISCONNECT);
	}
    }
  return 0;
}



int bgpstore_prefix_table_begin(bgpstore_t *bgp_store, 
				bgpwatcher_pfx_table_t *table)
{

  bgpview_t *bgp_view = NULL;
  uint32_t ts;
  khiter_t k;
  int khret;
  int ret;

  // sliding window checks

  if(bgp_store->min_ts > 0)
    {
      if(table->time >= (bgp_store->min_ts + BGPSTORE_TS_WDW_SIZE) )
	{
	  bgp_store->min_ts = 0;
	  /** trigger expiration of window on bgpviews whose ts
	   *  is older than  table->time - BGPSTORE_TS_WDW_SIZE:
	   *  expire ts if ts <= (table->time- BGPSTORE_TS_WDW_SIZE) */
	  for (k = kh_begin(bgp_store->bgp_timeseries); k != kh_end(bgp_store->bgp_timeseries); ++k)
	    {
	      if (kh_exist(bgp_store->bgp_timeseries, k))
		{
		  ts = kh_key(bgp_store->bgp_timeseries, k);
		  bgp_view = kh_value(bgp_store->bgp_timeseries, k);
		  
		  if(ts <= (table->time - BGPSTORE_TS_WDW_SIZE))
		    {
		      // ts is out of the sliding window
		      ret = bgpstore_completion_check(bgp_store, bgp_view, ts, BGPSTORE_WDW_EXCEEDED);

		    }
		  else
		    {
		      // get the next min_ts
		      if(bgp_store->min_ts == 0 || ts < bgp_store->min_ts)
			{
			  bgp_store->min_ts = ts;
			}
		    }
		}
	    }      
	}
      else
	{
	  if(table->time < bgp_store->min_ts)
	    {
	      fprintf(stderr, "bgpviews for time %"PRIu32" have been already processed!\n", ts);
	    }
	}
    }
  else
    {  // first insertion
      bgp_store->min_ts = table->time;
    }

  // insert new bgp_view if time does not exist yet
  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries,
		 table->time)) == kh_end(bgp_store->bgp_timeseries))
    {
      // first time we receive this table time -> create bgp_view
      if((bgp_view = bgpview_create()) == NULL)
	{
	  fprintf(stderr, "error could not create bgpview for time %"PRIu32", quitting.\n", table->time);
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
	{ // the function signals is something went wrong
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
      fprintf(stderr, "A bgpview for time %"PRIu32" must exist!\n", table->time);
      return -1;
    }
  
  bgpview_t * bgp_view = kh_value(bgp_store->bgp_timeseries,k);
  
  return bgpview_add_row(bgp_view, table, row);
}


int bgpstore_prefix_table_end(bgpstore_t *bgp_store, char *client_name,
			      bgpwatcher_pfx_table_t *table)
{
  khiter_t k;
  int ret;
  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries,
		 table->time)) == kh_end(bgp_store->bgp_timeseries))
    {
      // view for this time must exist
      fprintf(stderr, "A bgpview for time %"PRIu32" must exist!\n", table->time);
      return -1;
    }
  
  bgpview_t * bgp_view = kh_value(bgp_store->bgp_timeseries,k);
  if((ret = bgpview_table_end(bgp_view, client_name, table)) == 0)
    {
      bgpstore_completion_check(bgp_store, bgp_view, table->time, BGPSTORE_TABLE_END);
    }
  return ret;
}


int bgpstore_completion_check(bgpstore_t *bgp_store, bgpview_t *bgp_view, uint32_t ts, bgpstore_completion_trigger_t trigger)
{
  int ret;
  if((ret =  bgpview_completion_check(bgp_view, bgp_store->active_clients)) < 0)
    { // something went wrong with the completion check
      return ret;
    }
  
  uint8_t remove_view;

  // TODO: documentation here ##################

  if((trigger == BGPSTORE_WDW_EXCEEDED || trigger == BGPSTORE_TIMEOUT_EXPIRED) ||
     ((trigger == BGPSTORE_TABLE_END || trigger == BGPSTORE_CLIENT_DISCONNECT) && ret == 1) 
    )
    {
      remove_view = 1;
    }
  if((trigger == BGPSTORE_TABLE_END || trigger == BGPSTORE_CLIENT_DISCONNECT) && ret == 0)
    {
      remove_view = 0;
    }

  // TODO: documentation
  ret = bgpstore_interests_dispatcher_run(bgp_store->active_clients, bgp_view, ts);

  // TODO: documentation
  if(ret && remove_view == 1)
    {
      return bgpstore_remove_view(bgp_store, ts);
    }
  
  return ret;
}



int bgpstore_remove_view(bgpstore_t *bgp_store, uint32_t ts)
{
  khiter_t k;
  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries,
		 ts)) == kh_end(bgp_store->bgp_timeseries))
    {
      // view for this time must exist ? TODO: check policies!
      fprintf(stderr, "A bgpview for time %"PRIu32" must exist!\n", ts);
      return -1;
    }
  
  bgpview_t *bgp_view = kh_value(bgp_store->bgp_timeseries,k);

  // destroy view
  bgpview_destroy(bgp_view);

  // destroy time entry
  kh_del(timebgpview,bgp_store->bgp_timeseries,k);

  // update min_ts
  uint32_t time;
  bgp_store->min_ts = 0;  
  for (k = kh_begin(bgp_store->bgp_timeseries); k != kh_end(bgp_store->bgp_timeseries); ++k)
    {
      if (kh_exist(bgp_store->bgp_timeseries, k))
	{
	  time = kh_key(bgp_store->bgp_timeseries, k);
	  if(bgp_store->min_ts == 0 || bgp_store->min_ts > time)
	    {
	      bgp_store->min_ts = time;
	    }
	}
    }
  return 0;
}



  
int bgpstore_ts_completed_handler(bgpstore_t *bgp_store, uint32_t ts)
{
    
  // get current completed bgpview
  khiter_t k;
  if((k = kh_get(timebgpview, bgp_store->bgp_timeseries,
		 ts)) == kh_end(bgp_store->bgp_timeseries))
    {
      // view for this time must exist ? TODO: check policies!
      fprintf(stderr, "A bgpview for time %"PRIu32" must exist!\n", ts);
      return -1;
    }
  bgpview_t *bgp_view = kh_value(bgp_store->bgp_timeseries,k);

  int ret = bgpstore_interests_dispatcher_run(bgp_store->active_clients, bgp_view, ts);

  // TODO: decide whether to destroy the bgp_view or not

  // destroy view
  bgpview_destroy(bgp_view);

  // destroy time entry
  kh_del(timebgpview,bgp_store->bgp_timeseries,k);

  return ret;
}


void bpgstore_check_timeouts(bgpstore_t *bgp_store)
{
  bgpview_t *bgp_view = NULL;
  uint32_t ts;
  khiter_t k;
  struct timeval time_now;
  gettimeofday(&time_now, NULL);
  for (k = kh_begin(bgp_store->bgp_timeseries); k != kh_end(bgp_store->bgp_timeseries); ++k)
    {
      if (kh_exist(bgp_store->bgp_timeseries, k))
	{
	  ts = kh_key(bgp_store->bgp_timeseries, k);
	  bgp_view = kh_value(bgp_store->bgp_timeseries, k);
	  if( (time_now.tv_sec - bgp_view->bv_created_time.tv_sec) > BGPSTORE_BGPVIEW_TIMEOUT)
	    {
	      bgpstore_completion_check(bgp_store, bgp_view, ts, BGPSTORE_TIMEOUT_EXPIRED);
	    }
	}
    }
  return;
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

