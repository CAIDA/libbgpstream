/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#include "bgpribs_lib.h"
#include <assert.h>
#include "utils.h"

#define METRIC_PREFIX "bgp"


static void graphite_safe(char *p)
{
  if(p == NULL)
    {
      return;
    }

  while(*p != '\0')
    {
      if(*p == '.')
	{
	  *p = '_';
	}
      if(*p == '*')
	{
	  *p = '-';
	}
      p++;
    }
}


/** ribs_table related functions */

ribs_table_t *ribs_table_create() 
{
  ribs_table_t *ribs_table;
  if((ribs_table = malloc_zero(sizeof(ribs_table_t))) == NULL)
    {
      return NULL;
    }
  // init ipv4 and ipv6 khashes
  ribs_table->ipv4_rib = kh_init(ipv4_rib_t);
  ribs_table->ipv6_rib = kh_init(ipv6_rib_t);
  return ribs_table;
}

void ribs_table_destroy(ribs_table_t *ribs_table) 
{
  if(ribs_table != NULL) 
    {
      kh_destroy(ipv4_rib_t, ribs_table->ipv4_rib);
      kh_destroy(ipv6_rib_t, ribs_table->ipv6_rib);
      // free ribs_table
      free(ribs_table);
    }
}


/** ases_table related functions */

ases_table_wrapper_t *ases_table_create() 
{
  ases_table_wrapper_t *ases_table;
  if((ases_table = malloc_zero(sizeof(ases_table_wrapper_t))) == NULL)
    {
      return NULL;
    }
  // init khash
  ases_table->table = kh_init(ases_table_t);
  return ases_table;
}

void ases_table_destroy(ases_table_wrapper_t *ases_table) 
{
  if(ases_table != NULL) 
    {
      kh_destroy(ases_table_t, ases_table->table);
      // free prefixes_table
      free(ases_table);
    }
}


/** prefixes_table related functions */

prefixes_table_t *prefixes_table_create() 
{
  prefixes_table_t *prefixes_table;
  if((prefixes_table = malloc_zero(sizeof(prefixes_table_t))) == NULL)
    {
      return NULL;
    }
  // init ipv4 and ipv6 khashes
  prefixes_table->ipv4_prefixes_table = kh_init(ipv4_prefixes_table_t);
  prefixes_table->ipv6_prefixes_table = kh_init(ipv6_prefixes_table_t);
  return prefixes_table;
}

void prefixes_table_destroy(prefixes_table_t *prefixes_table) 
{
  if(prefixes_table == NULL) 
    {
      kh_destroy(ipv4_prefixes_table_t, prefixes_table->ipv4_prefixes_table);
      kh_destroy(ipv6_prefixes_table_t, prefixes_table->ipv6_prefixes_table);
      // free prefixes_table
      free(prefixes_table);
    }
}


/** peerdata related functions */

peerdata_t *peerdata_create(bgpstream_ip_address_t * peer_address)
{
  peerdata_t *peer_data;
  if((peer_data = malloc_zero(sizeof(peerdata_t))) == NULL)
    {
      return NULL;
    }

  if((peer_data->active_ribs_table = ribs_table_create()) == NULL)
    {
      free(peer_data);
      return NULL;
    }

  if((peer_data->uc_ribs_table = ribs_table_create()) == NULL)
    {
      ribs_table_destroy(peer_data->active_ribs_table);
      peer_data->active_ribs_table = NULL;
      free(peer_data);
      return NULL;
    }
  
  char ip_str[INET6_ADDRSTRLEN];
  ip_str[0] = '\0';
  if(peer_address->type == BST_IPV4)
    {
      inet_ntop(AF_INET, &(peer_address->address.v4_addr), ip_str, INET6_ADDRSTRLEN);
    }
  else // assert(peer_address->type == BST_IPV6)
    {
      inet_ntop(AF_INET6, &(peer_address->address.v6_addr), ip_str, INET6_ADDRSTRLEN);
    }
  
  if( (peer_data->peer_address_str = strdup(ip_str)) == NULL ) 
    {
      ribs_table_destroy(peer_data->active_ribs_table);
      peer_data->active_ribs_table = NULL;
      ribs_table_destroy(peer_data->uc_ribs_table);
      peer_data->uc_ribs_table = NULL;
      free(peer_data);
      return NULL;
    }
  graphite_safe(peer_data->peer_address_str);

  return peer_data;
}


int peerdata_apply_elem(peerdata_t *peer_data, 
			bgpstream_record_t * bs_record, bgpstream_elem_t *bs_elem)
{
  assert(peer_data);
  assert(bs_record);
  assert(bs_elem);
  // TODO: here
  return 0;
}


int peerdata_apply_record(peerdata_t *peer_data, bgpstream_record_t * bs_record)
{
  assert(peer_data);
  assert(bs_record);
  // TODO: here
  return 1;
}


void peerdata_destroy(peerdata_t *peer_data)
{
  if(peer_data != NULL) 
    {
      if(peer_data->active_ribs_table != NULL) 
	{
	  ribs_table_destroy(peer_data->active_ribs_table);
	  peer_data->active_ribs_table = NULL;
	}
      if(peer_data->uc_ribs_table != NULL) 
	{
	  ribs_table_destroy(peer_data->uc_ribs_table);
	  peer_data->uc_ribs_table = NULL;
	}
      if(peer_data->peer_address_str != NULL) 
	{
	  free(peer_data->peer_address_str);
	  peer_data->peer_address_str = NULL;
	}
      free(peer_data);
    }
}


/* peers_table functions */

peers_table_t *peers_table_create() 
{
  peers_table_t *peers_table;
  if((peers_table = malloc_zero(sizeof(peers_table_t))) == NULL)
    {
      return NULL;
    }
  /* all data are set to zero by malloc_zero, thereby:
   *  - elem_types = [0,0,0,0]
   */  
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
	  peers_table->elem_types[bs_iterator->type]++;

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
		}
	      else
		{ /* already exists, just get it */
		  peer_data = kh_value(peers_table->ipv6_peers_table, k);
		}    
	    }
	  
	  // apply each elem to the right peer_data
	  if((peerdata_apply_elem(peer_data, bs_record, bs_iterator)) == -1) 
	    {
	      // TODO: output some error message
	      bgpstream_destroy_elem_queue(bs_elem_queue);
	      return -1;
	    }

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
	  // apply each record to each peer_data
	  peer_status = peerdata_apply_record(peer_data, bs_record);
	  if(peer_status < 0)
	    {
	      // TODO something went wrong during "TODO" function
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
	  // apply each record to each peer_data
	  peer_status = peerdata_apply_record(peer_data, bs_record);
	  if(peer_status < 0)
	    {
	      // TODO something went wrong during "TODO" function
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


/* collectordata related functions */

collectordata_t *collectordata_create(const char *project,
				      const char *collector)
{
  collectordata_t *collector_data;
  assert(project != NULL);
  assert(collector != NULL);

  if((collector_data = malloc_zero(sizeof(collectordata_t))) == NULL)
    {
      return NULL;
    }
  /* all data are set to zero by malloc_zero, thereby:
   *  - most_recent_ts = 0
   *  - status = 0 = COLLECTOR_NULL
   *  - record_types = [0,0,0,0,0]
   */

  if((collector_data->dump_project = strdup(project)) == NULL)
    {
      free(collector_data);
      return NULL;
    }

  if((collector_data->dump_collector = strdup(collector)) == NULL)
    {
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }

  if((collector_data->peers_table = peers_table_create()) == NULL)
    {
      free(collector_data->dump_collector);
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }

  /* make the project name graphite-safe */
  graphite_safe(collector_data->dump_project);

  /* make the collector name graphite-safe */
  graphite_safe(collector_data->dump_collector);
  
  return collector_data;
}


int collectordata_process_record(collectordata_t *collector_data,
				 bgpstream_record_t * bs_record)
{
  assert(collector_data);
  assert(bs_record);
  
  // register what kind of record types we receive
  collector_data->record_types[bs_record->status]++;

  // update the most recent timestamp
  if(bs_record->attributes.record_time > collector_data->most_recent_ts) 
    {
      collector_data->most_recent_ts = bs_record->attributes.record_time;
    }

  /* send the record to the peers_table and get the number
   * of active peers */
  collector_data->active_peers = 
    peers_table_process_record(collector_data->peers_table, bs_record);

  // some error occurred during the computation
  if(collector_data->active_peers < 0) 
    {
      collector_data->status = COLLECTOR_DOWN;    
      return -1;
    }
   
  // some peers are active => the collector is active too 
  if(collector_data->active_peers > 0)
    {
      collector_data->status = COLLECTOR_UP;    
    }
  else
    { // assert(collector_data->active_peers == 0) 
      // collector was in unknown state => it remains there
      if(collector_data->status == COLLECTOR_NULL)
	{
	  collector_data->status = COLLECTOR_NULL;
	}
      else // collector was in known state
	{  // now, no peer is active => the collector must be DOWN      
	  collector_data->status = COLLECTOR_DOWN;    
	}
    }
  return 0;
}


void collectordata_destroy(collectordata_t *collector_data)
{
  if(collector_data != NULL)
    {
      if(collector_data->dump_project != NULL)
	{
	  free(collector_data->dump_project);
	  collector_data->dump_project = NULL;
	}

      if(collector_data->dump_collector != NULL)
	{
	  free(collector_data->dump_collector);
	  collector_data->dump_collector = NULL;
	}
      if(collector_data->peers_table != NULL)
	{
	  peers_table_destroy(collector_data->peers_table);
	  collector_data->peers_table = NULL;
	}
      free(collector_data);
    }
}


/* collectors_table related functions */

collectors_table_wrapper_t *collectors_table_create() 
{
  collectors_table_wrapper_t *collectors_table;
  if((collectors_table = malloc_zero(sizeof(collectors_table_wrapper_t))) == NULL)
    {
      return NULL;
    }
  collectors_table->table = kh_init(collectors_table_t);
  return collectors_table;
}


/* returns 0 if everything is fine */
int collectors_table_process_record(collectors_table_wrapper_t *collectors_table,
				    bgpstream_record_t * bs_record)
{
  khiter_t k;
  char *collector_name_cpy = NULL;
  collectordata_t * collector_data;
  int khret;

  /* check if the collector associated with the bs_record
   * already exists, if not create a collectordata object, 
   * then pass the bs_record to the collectordata object */
  if((k = kh_get(collectors_table_t, collectors_table->table,
		 bs_record->attributes.dump_collector)) ==
     kh_end(collectors_table->table))
    {
      /* create a new collectordata structure */
      if((collector_data =
	collectordata_create(bs_record->attributes.dump_project,
			     bs_record->attributes.dump_collector)) == NULL) 
	{
	  // TODO: cerr the error -> can't create collectordata
	  return -1;
	}
      /* copy the name of the collector */
      if((collector_name_cpy = strdup(bs_record->attributes.dump_collector)) == NULL)
	{
	  // TODO: cerr the error -> can't malloc memory for collector name
	  return -1;
	}
      /* add it (key/name) to the hash */
      k = kh_put(collectors_table_t, collectors_table->table,
		 collector_name_cpy, &khret);
      /* add collectordata (value) to the hash */
      kh_value(collectors_table->table, k) = collector_data;
    }
  else
    {
      /* already exists, just get it */
      collector_data = kh_value(collectors_table->table, k);
    }
  // provide the bs_record to the right collectordata structure
  return collectordata_process_record(collector_data, bs_record);
} 



void collectors_table_destroy(collectors_table_wrapper_t *collectors_table) 
{
  khiter_t k;
  if(collectors_table != NULL) 
    {
      /* free all values in the  collectors_table hash */
      for (k = kh_begin(collectors_table->table);
	   k != kh_end(collectors_table->table); ++k)
	{
	  if (kh_exist(collectors_table->table, k))
	    {
	      /* free the value */
	      collectordata_destroy(kh_value(collectors_table->table, k));
	    }
	}   
      kh_destroy(collectors_table_t, collectors_table->table);
      collectors_table->table = NULL;
      free(collectors_table);
    }
}




