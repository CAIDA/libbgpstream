/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bgpribs_peers_table.h"


peers_table_t *peers_table_create() 
{
  peers_table_t *peers_table;
  if((peers_table = malloc_zero(sizeof(peers_table_t))) == NULL)
    {
      return NULL;
    }
  // init ipv4 and ipv6 peers khashes
  peers_table->ipv4_peers_table = kh_init(ipv4_peers_table_t);
  peers_table->ipv6_peers_table = kh_init(ipv6_peers_table_t);
  return peers_table;
}


/* process the current record and
 * returns the number of active peers  
 * it returns < 0 if some error was encountered */
int peers_table_process_record(peers_table_t *peers_table, 
			       bgpstream_record_t * bs_record)
{
  assert(peers_table);
  assert(bs_record);
  int num_active_peers = 0;
  bgpstream_elem_t * bs_elem_queue;
  bgpstream_elem_t * bs_iterator;
  khiter_t k;
  int khret;
  peerdata_t * peer_data = NULL;

  /* if we receive a VALID_RECORD we extract the 
   * bgpstream_elem_queue and we send each elem
   * to the corresponding peer in the peer table */
  if(bs_record->status == VALID_RECORD) 
    {
      bs_elem_queue = bgpstream_get_elem_queue(bs_record);
      bs_iterator = bs_elem_queue;
      while(bs_iterator != NULL)
	{
	  /* check if we need to create a peer
	   * create the peerdata structure if necessary
	   * and send the elem to the corresponding peerdata */
	  /* update peer information and check return value*/
	  if(bs_iterator->peer_address.type == BST_IPV4)
	    {
	      /* check if this peer is in the hash already */
	      if((k = kh_get(ipv4_peers_table_t, peers_table->ipv4_peers_table,
			     bs_iterator->peer_address)) ==
		 kh_end(peers_table->ipv4_peers_table))
		{
		  /* create a new peerdata structure */
		  if((peer_data = peerdata_create(&(bs_iterator->peer_address))) == NULL)
		    {
		      // TODO: output some error message
		      bgpstream_destroy_elem_queue(bs_elem_queue);
		      return -1;
		    }
		  /* add it to the hash */
		  k = kh_put(ipv4_peers_table_t, peers_table->ipv4_peers_table, 
			     bs_iterator->peer_address, &khret);
		  kh_value(peers_table->ipv4_peers_table, k) = peer_data;
		  // if we have just created a peer_data and we are reading
		  // a BGPSTREAM_RIB, we move the rt_status to UC_ON
		  if(bs_record->attributes.dump_type == BGPSTREAM_RIB)
		    {
		      peer_data->rt_status = UC_ON;
		      peer_data->uc_ribs_table->reference_dump_time = bs_record->attributes.dump_time;
		      peer_data->uc_ribs_table->reference_rib_start = 0;
		      peer_data->uc_ribs_table->reference_rib_end = 0;
		    }
		}
	      else
		{ /* already exists, just get it */	
		  peer_data = kh_value(peers_table->ipv4_peers_table, k);
		}
	    }
	  else 
	    { // assert(bs_iterator->peer_address.type == BST_IPV6) 
	      /* check if this peer is in the hash already */
	      if((k = kh_get(ipv6_peers_table_t, peers_table->ipv6_peers_table,
			     bs_iterator->peer_address)) ==
		 kh_end(peers_table->ipv6_peers_table))
		{
		  /* create a new peerdata structure */
		  if((peer_data = peerdata_create(&(bs_iterator->peer_address))) == NULL)
		    {
		      // TODO: output some error message
		      bgpstream_destroy_elem_queue(bs_elem_queue);
		      return -1;
		    }
		  /* add it to the hash */
		  k = kh_put(ipv6_peers_table_t, peers_table->ipv6_peers_table, 
			     bs_iterator->peer_address, &khret);
		  kh_value(peers_table->ipv6_peers_table, k) = peer_data;
		  // if we have just created a peer_data and we are reading
		  // a BGPSTREAM_RIB, we move the rt_status to UC_ON
		  if(bs_record->attributes.dump_type == BGPSTREAM_RIB)
		    {
		      peer_data->rt_status = UC_ON;
		      peer_data->uc_ribs_table->reference_dump_time = bs_record->attributes.dump_time;
		      peer_data->uc_ribs_table->reference_rib_start = 0;
		      peer_data->uc_ribs_table->reference_rib_end = 0;
		    }
		}
	      else
		{ /* already exists, just get it */
		  peer_data = kh_value(peers_table->ipv6_peers_table, k);
		}    
	    }	  

	  // DEBUG:
	  /* ribs_tables_status_t old_rt_status = peer_data->rt_status; */
	  /* peer_status_t old_status = peer_data->status; */

	  // apply each elem to the right peer_data
	  if((peerdata_apply_elem(peer_data, bs_record, bs_iterator)) == -1) 
	    {
	      // TODO: output some error message
	      bgpstream_destroy_elem_queue(bs_elem_queue);
	      return -1;
	    }
	  
	  // DEBUG:
	  /* if(strcmp(peer_data->peer_address_str, "202_79_197_122") == 0 && */
	  /*    (old_status != peer_data->status || old_rt_status != peer_data->rt_status)) */
	  /*   { */
	  /*     printf("\t %ld E %s - (%d - %d) - to -> (%d - %d)\n", */
	  /* 	     peer_data->most_recent_ts, */
	  /* 	     peer_data->peer_address_str, */
	  /* 	     old_status, old_rt_status, */
	  /* 	     peer_data->status, peer_data->rt_status); */
	  /*   } */
	  
	  // other information are computed at dump time
	  bs_iterator = bs_iterator->next;
	}
      bgpstream_destroy_elem_queue(bs_elem_queue);
    }


  /* now that all the peers have been created (if it was necessary)
   * we send the current record to all of them and
   * we count how many of them are active or if some
   * computation generated an unexpected error */
  int peer_status;
  for(k = kh_begin(peers_table->ipv4_peers_table);
      k != kh_end(peers_table->ipv4_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv4_peers_table, k))
	{	  
	  peer_data = kh_value(peers_table->ipv4_peers_table, k);
	  // DEBUG:
	  /* ribs_tables_status_t old_rt_status = peer_data->rt_status; */
	  /* peer_status_t old_status = peer_data->status; */
	  // apply each record to each peer_data
	  peer_status = peerdata_apply_record(peer_data, bs_record);
	  // DEBUG:
	  /* if(strcmp(peer_data->peer_address_str, "202_79_197_122") == 0 && */
	  /*    (old_status != peer_data->status || old_rt_status != peer_data->rt_status)) */
	  /*   { */
	  /*     printf("\t %ld R %s - (%d - %d) - to -> (%d - %d)\n", */
	  /* 	     peer_data->most_recent_ts, */
	  /* 	     peer_data->peer_address_str, */
	  /* 	     old_status, old_rt_status, */
	  /* 	     peer_data->status, peer_data->rt_status); */
	  /*   } */
	  if(peer_status < 0)
	    {
	      // something went wrong during peerdata_apply_record function
	      // TODO: log error
	      return -1;
	    }
	  else 
	    {
	      num_active_peers += peer_status;
	    }
	}      
    }
  for(k = kh_begin(peers_table->ipv6_peers_table);
      k != kh_end(peers_table->ipv6_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv6_peers_table, k))
	{
	  peer_data = kh_value(peers_table->ipv6_peers_table, k);
	  // DEBUG:
	  /* ribs_tables_status_t old_rt_status = peer_data->rt_status; */
	  /* peer_status_t old_status = peer_data->status; */
	  // apply each record to each peer_data
	  peer_status = peerdata_apply_record(peer_data, bs_record);
	  // DEBUG:
	  /* if( strcmp(peer_data->peer_address_str, "193_160_39_1") == 0 && */
	  /*    (old_status != peer_data->status || old_rt_status != peer_data->rt_status)) */
	  /*   {  */
	  /*     printf("\t %ld R %s - (%d - %d) - to -> (%d - %d)\n", */
	  /* 	     peer_data->most_recent_ts, */
	  /* 	     peer_data->peer_address_str,  */
	  /* 	     old_status, old_rt_status, */
	  /* 	     peer_data->status, peer_data->rt_status); */
	  /*   }  */
	  if(peer_status < 0)
	    {
	      // something went wrong during peerdata_apply_record function
	      // TODO: log error
	      return -1;
	    }
	  else 
	    {
	      num_active_peers += peer_status;
	    }
	}      
    }
  return num_active_peers;
}




#ifdef WITH_BGPWATCHER
int peers_table_interval_end(char *project_str, char *collector_str,
			     peers_table_t *peers_table,
			     aggregated_bgp_stats_t * collector_aggr_stats,
			     bw_client_t *bw_client,
			     int interval_start)
#else
int peers_table_interval_end(char *project_str, char *collector_str,
			     peers_table_t *peers_table,
			     aggregated_bgp_stats_t * collector_aggr_stats,
			     int interval_start)
#endif
{
  assert(peers_table);
  khiter_t k;
  bgpstream_ip_address_t peer_address;
  peerdata_t * peer_data;

#ifdef WITH_BGPWATCHER
  int rc;
  uint32_t peer_table_time = interval_start;
  // if the bgpwatcher client is enabled, then start to send the peer table
  if((rc = bgpwatcher_client_peer_table_begin(bw_client->peer_table,
					      collector_str,
					      peer_table_time)) < 0)
    {
      fprintf(stderr, "Could not begin peer table\n");
      return -1;
    }
#endif

  // print stats for ipv4 peers
  for (k = kh_begin(peers_table->ipv4_peers_table);
       k != kh_end(peers_table->ipv4_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv4_peers_table, k))
	{
	  peer_address = kh_key(peers_table->ipv4_peers_table, k);
	  peer_data = kh_value(peers_table->ipv4_peers_table, k);
#ifdef WITH_BGPWATCHER
	  if(bgpwatcher_client_peer_table_add(bw_client->peer_table,
					      &peer_address, 
					      peer_data->status) < 0)
	    {
	      // something went wrong with bgpwatcher
	      fprintf(stderr, "Something went wrong with bgpwatcher peer table add\n");
	      return -1;
	    }
	  if(peerdata_interval_end(project_str, collector_str, 
				   &peer_address, peer_data, 
				   collector_aggr_stats, bw_client, interval_start) < 0)
#else
	  if(peerdata_interval_end(project_str, collector_str, 
				   &peer_address, peer_data, 
				   collector_aggr_stats, interval_start) < 0)
#endif
	    {
	      return -1;
	    }
	}
    }   

  // print stats for ipv6 peers
  for (k = kh_begin(peers_table->ipv6_peers_table);
       k != kh_end(peers_table->ipv6_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv6_peers_table, k))
	{
	  peer_address = kh_key(peers_table->ipv6_peers_table, k);
	  peer_data = kh_value(peers_table->ipv6_peers_table, k);
#ifdef WITH_BGPWATCHER
	  if(bgpwatcher_client_peer_table_add(bw_client->peer_table,
					      &peer_address, 
					      peer_data->status) < 0)
	    {
	      // something went wrong with bgpwatcher
	      fprintf(stderr, "Something went wrong with bgpwatcher peer table add\n");
	      return -1;
	    }
	  if(peerdata_interval_end(project_str, collector_str, 
				   &peer_address, peer_data, 
				   collector_aggr_stats, bw_client, interval_start) < 0)
#else
	  if(peerdata_interval_end(project_str, collector_str, 
				   peer_address, peer_data, 
				   collector_aggr_stats, interval_start) < 0)
#endif
	    {
	      return -1;
	    }
	}
    }

#ifdef WITH_BGPWATCHER
  if((rc = bgpwatcher_client_peer_table_end(bw_client->peer_table)) < 0)
    {
      fprintf(stderr, "Could not end peer table\n");
      return -1;
    }
#endif
  return 0;
}


void peers_table_destroy(peers_table_t *peers_table) 
{
  khiter_t k;
  if(peers_table != NULL) 
    {
      /* free all values in the ipv4 peers_table table */
      for (k = kh_begin(peers_table->ipv4_peers_table);
	   k != kh_end(peers_table->ipv4_peers_table); ++k)
	{
	  if (kh_exist(peers_table->ipv4_peers_table, k))
	    {
	      /* free the value */
	      peerdata_destroy(kh_value(peers_table->ipv4_peers_table, k));
	    }
	}   

      /* free all values in the ipv6 peers_table table */
      for (k = kh_begin(peers_table->ipv6_peers_table);
	   k != kh_end(peers_table->ipv6_peers_table); ++k)
	{
	  if (kh_exist(peers_table->ipv6_peers_table, k))
	    {
	      /* free the value */
	      peerdata_destroy(kh_value(peers_table->ipv6_peers_table, k));
	    }
	}  
      /* destroy the ipv4 peers table */
      kh_destroy(ipv4_peers_table_t, peers_table->ipv4_peers_table);
      /* destroy the ipv6 peers table */
      kh_destroy(ipv6_peers_table_t, peers_table->ipv6_peers_table);
      // free peers_table
      free(peers_table);
    }
}
