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


#include "bgpstore_bgpview.h"
#include <sys/time.h>

static peerview_t* peerview_create()
{
  return kh_init(bsid_pfxview);    
}

static int peerview_insert(peerview_t *peer_view, uint16_t server_id, pfxinfo_t *pfx_info)
{
  khiter_t k;
  int khret; 
  if((k = kh_get(bsid_pfxview, peer_view, server_id)) == kh_end(peer_view))
    {
      k = kh_put(bsid_pfxview, peer_view, server_id, &khret);
    }
  kh_value(peer_view,k) = *pfx_info;
  return 0;
}


static void peerview_destroy(peerview_t *pv)
{
  if(pv != NULL)
    {
      kh_destroy(bsid_pfxview, pv);	
    }
}


static active_peer_status_t * peer_status_map_get_status(peer_status_map_t *ps_map, uint16_t server_id)
{
  khiter_t k;

  if((k = kh_get(peer_status_map, ps_map, server_id)) == kh_end(ps_map))
    {
      fprintf(stderr, "Processing a server id which has not been registered before!\n");
      return NULL;
    }
  // return active_peer_status_t *
  return &(kh_value(ps_map,k));	  
}
     


bgpview_t *bgpview_create()
{
  bgpview_t *bgp_view;

  if((bgp_view = malloc_zero(sizeof(bgpview_t))) == NULL)
    {
      return NULL;
    }

  if((bgp_view->aggregated_pfxview_ipv4 = kh_init(aggr_pfxview_ipv4)) == NULL)
    {
      goto err;
    }

  if((bgp_view->aggregated_pfxview_ipv6 = kh_init(aggr_pfxview_ipv6)) == NULL)
    {
      goto err;
    }

  if((bgp_view->done_clients = bl_string_set_create()) == NULL)
    {
      goto err;
    }

  if((bgp_view->inactive_peers = bl_id_set_create()) == NULL)
    {
      goto err;
    }

  if((bgp_view->active_peers_info = kh_init(peer_status_map)) == NULL)
    {
      goto err;
    }

  bgp_view->state = BGPVIEW_STATE_UNKNOWN;

  // dis_status -> everything is set to zero
  
  gettimeofday (&bgp_view->bv_created_time, NULL);

  return bgp_view;
    
 err:
  fprintf(stderr, "Failed to create bgpstore bgpview\n");
  if(bgp_view != NULL)
    {
      bgpview_destroy(bgp_view);
    }
  return NULL;
}


int bgpview_add_peer(bgpview_t *bgp_view, bgpwatcher_peer_t* peer_info)
{
  khiter_t k;
  int khret;

  bgp_view->state = BGPVIEW_STATE_UNKNOWN;

  active_peer_status_t *ap_status = NULL;

  // if peer is up
  if(peer_info->status == 2) // ####################### TODO: use bl_bgp_utils!!!!!!!!!!!!!!!! #######################
    {
      // update active peers info
      if((k = kh_get(peer_status_map, bgp_view->active_peers_info,
		     peer_info->server_id)) == kh_end(bgp_view->active_peers_info))
	{
	  k = kh_put(peer_status_map, bgp_view->active_peers_info,
		     peer_info->server_id, &khret);
	  ap_status = &(kh_value(bgp_view->active_peers_info,k));
	  // initialize internal values (all zeros)
	  memset(ap_status, 0, sizeof(active_peer_status_t));
	}
      ap_status = &(kh_value(bgp_view->active_peers_info,k));
      // a new pfx table segment is expected from this peer
      ap_status->expected_pfx_tables_cnt++;
    }
  else
    {
      // add a new inactive peer to the list
      bl_id_set_insert(bgp_view->inactive_peers, peer_info->server_id);
    }
  return 0;
}


static peerview_t *get_ipv4_peerview(aggr_pfxview_ipv4_t *ipv4_table, bl_ipv4_pfx_t *pfx_ipv4)
{
  peerview_t *peer_view;
  khiter_t k;
  int khret;

  if((k = kh_get(aggr_pfxview_ipv4, ipv4_table, *pfx_ipv4)) == kh_end(ipv4_table))
    {
      if((peer_view = peerview_create()) == NULL)
	{
	  return NULL;
	}
      k = kh_put(aggr_pfxview_ipv4, ipv4_table, *pfx_ipv4, &khret);
      kh_value(ipv4_table,k) = peer_view;     
    }
    return kh_value(ipv4_table,k);
}

static peerview_t *get_ipv6_peerview(aggr_pfxview_ipv6_t *ipv6_table, bl_ipv6_pfx_t *pfx_ipv6)
{
  peerview_t *peer_view;
  khiter_t k;
  int khret;

  if((k = kh_get(aggr_pfxview_ipv6, ipv6_table, *pfx_ipv6)) == kh_end(ipv6_table))
    {
      if((peer_view = peerview_create()) == NULL)
	{
	  return NULL;
	}
      k = kh_put(aggr_pfxview_ipv6, ipv6_table, *pfx_ipv6, &khret);
      kh_value(ipv6_table,k) = peer_view;     
    }
    return kh_value(ipv6_table,k);
}


int bgpview_add_row(bgpview_t *bgp_view, bgpwatcher_pfx_table_t *table,
		    bgpwatcher_pfx_row_t *row)
{    
  int remote_peer_id;
  peerview_t *peer_view;
  pfxinfo_t pfx_info;
  uint16_t server_id;
  active_peer_status_t *ap_status;
  bgp_view->state = BGPVIEW_STATE_UNKNOWN;
  // convert pfx_storage in a ip version specific pfx
  // and get the appropriate peer_view
  bl_ipv4_pfx_t pfx_ipv4;
  bl_ipv6_pfx_t pfx_ipv6;  
  if(row->prefix.address.version == BL_ADDR_IPV4)
    {
      pfx_ipv4 = bl_pfx_storage2ipv4(&row->prefix);
      peer_view = get_ipv4_peerview(bgp_view->aggregated_pfxview_ipv4, &pfx_ipv4);
    }
  else 
    {
      if(row->prefix.address.version == BL_ADDR_IPV6)
	{
	  pfx_ipv6 = bl_pfx_storage2ipv6(&row->prefix);
	  peer_view = get_ipv6_peerview(bgp_view->aggregated_pfxview_ipv6, &pfx_ipv6);
	}
      else {	
	fprintf(stderr, "Unknown ip version prefix provided!\n");
	return -1;
      }
    }

  for(remote_peer_id = 0; remote_peer_id < table->peers_cnt; remote_peer_id++)
    {
      // for each peer in use
      if(row->info[remote_peer_id].in_use == 1)
	{
	  // get pfx_info 
	  pfx_info.orig_asn = row->info[remote_peer_id].orig_asn;
	  // get server id
	  server_id = table->peers[remote_peer_id].server_id;	  
	  // insert in pfx_view
	  if(peerview_insert(peer_view, server_id, &pfx_info) < 0)
	    {
	      return -1;
	    }
	  // get the active peer status ptr for the current id
	  if((ap_status = peer_status_map_get_status(bgp_view->active_peers_info, server_id)) == NULL)
	    {
	      return -1;
	    }
	  // update counters
	  if(row->prefix.address.version == BL_ADDR_IPV4)
	    {
	      ap_status->recived_ipv4_pfx_cnt++;
	    }
	  else
	    {
	      ap_status->recived_ipv6_pfx_cnt++;				
	    }
	}
    }

  return 0;
}


int bgpview_table_end(bgpview_t *bgp_view, char *client_name,
		      bgpwatcher_pfx_table_t *table)
{
  int remote_peer_id;
  uint16_t server_id;
  active_peer_status_t *ap_status;
  bgp_view->state = BGPVIEW_STATE_UNKNOWN;
  for(remote_peer_id = 0; remote_peer_id < table->peers_cnt; remote_peer_id++)
    {
      if(table->peers[remote_peer_id].status == 2)  // ####### TODO: use bl_bgp_utils!!!!!!!!!!!!!!!! #######################
	{
	  server_id = table->peers[remote_peer_id].server_id;
	  // get the active peer status ptr for the current id
	  if((ap_status = peer_status_map_get_status(bgp_view->active_peers_info, server_id)) == NULL)
	    {
	      // TODO: documentation
	      return -1;
	    }
	  // update active peers counters (i.e. signal table received)
	  ap_status->received_pfx_tables_cnt++;
	}
    }
  
  // add this client to the list of clients done
  bl_string_set_insert(bgp_view->done_clients, client_name);

  int i;
  for(i = 0; i < BGPVIEW_STATE_MAX; i++)
    {
      bgp_view->dis_status[i].modified = 1;
    }
  
  return 0;
}


int bgpview_completion_check(bgpview_t *bgp_view, clientinfo_map_t *active_clients)
{
  khiter_t k;
  clientstatus_t *cl_status;
  char *client_name;

  for (k = kh_begin(active_clients); k != kh_end(active_clients); ++k)
    {
      if (kh_exist(active_clients, k))
	{
	  client_name = kh_key(active_clients,k);
	  cl_status = &(kh_value(active_clients,k));
	  if(cl_status->producer_intents & BGPWATCHER_PRODUCER_INTENT_PREFIX)
	    {
	      // check if all the producers are done with sending pfx tables
	      if(bl_string_set_exists(bgp_view->done_clients, client_name) == 0)
		{
		  bgp_view->state = BGPVIEW_PARTIAL;
		  return 0;
		}
	    }
	}
    } 
  // bgp_view complete
  bgp_view->state = BGPVIEW_FULL;
  return 1;
}


void bgpview_destroy(bgpview_t *bgp_view)
{
  if(bgp_view != NULL)
    {

      if(bgp_view->aggregated_pfxview_ipv4 != NULL)
	{
	  kh_free_vals(aggr_pfxview_ipv4, bgp_view->aggregated_pfxview_ipv4,
		       peerview_destroy);	  
	  kh_destroy(aggr_pfxview_ipv4, bgp_view->aggregated_pfxview_ipv4);
	  bgp_view->aggregated_pfxview_ipv4 = NULL;
	}

      if(bgp_view->aggregated_pfxview_ipv6 != NULL)
	{
	  kh_free_vals(aggr_pfxview_ipv6, bgp_view->aggregated_pfxview_ipv6,
		       peerview_destroy);	  
	  kh_destroy(aggr_pfxview_ipv6, bgp_view->aggregated_pfxview_ipv6);
	  bgp_view->aggregated_pfxview_ipv6 = NULL;
	}

      if(bgp_view->done_clients != NULL)
	{
	  bl_string_set_destroy(bgp_view->done_clients);
	  bgp_view->done_clients = NULL;
	}

      if(bgp_view->inactive_peers != NULL)
	{
	  bl_id_set_destroy(bgp_view->inactive_peers);
	  bgp_view->inactive_peers = NULL;
	}

      if(bgp_view->active_peers_info != NULL)
	{
	  kh_destroy(peer_status_map, bgp_view->active_peers_info);
	  bgp_view->active_peers_info = NULL;
	}

      free(bgp_view);
    }
}

