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
  /* default:
   *  status = 0 = PEER_NULL
   *  rt_status = 0 = UC_OFF
   */

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
  assert(bs_record->status == VALID_RECORD);
  assert(bs_elem);

  if(peer_data->status == PEER_UP)
    {
      if(peer_data->rt_status == UC_OFF)
	{
	  // type is update
	  if(bs_elem->type == BST_ANNOUNCEMENT || bs_elem->type == BST_WITHDRAWAL)
	    {
	      // information arrived on time
	      if(bs_elem->timestamp >= peer_data->most_recent_ts)
		{
		  // TODO: apply update to active_ribs and update most_recent_ts
		  return 0;
		}
	      else 
		{
		  // update arrived just right after rib (forgive this delay)
		  if(peer_data->most_recent_ts == peer_data->active_ribs_table->reference_rib_end &&
		     bs_elem->timestamp >= peer_data->active_ribs_table->reference_rib_start &&
		     bs_elem->timestamp <= peer_data->active_ribs_table->reference_rib_end)
		    {
		      // TODO: apply update
		      return 0;
		    }
		  else 
		    {
		      // out of order that does not affect the current status
		      if(bs_elem->timestamp < peer_data->active_ribs_table->reference_rib_start)
			{
			  // TODO: signal out of order and ignore
			  return 0;
			}
		      else
			{
			  // TODO: think about possible rollback options
			  // TODO: for the moment, signal the out of order and
			  // go to PEER_NULL status 
			  return 0;
			}
		    }
		}
	    }
	  // type is rib
	  if(bs_elem->type == BST_RIB)
	    {
	      // information arrived on time TODO: decide if we want to maintain the = or not
	      if(bs_elem->timestamp >= peer_data->most_recent_ts)
		{
		  // a new rib arrived, we change to UC_ON status
		  if(bs_record->dump_pos == DUMP_START &&
		     bs_record->attributes.dump_time > peer_data->active_ribs_table->reference_dump_time)
		    {
		      // TODO: turn UC_ON, apply rib to uc_ribs and update most_recent_ts
		      return 0;
		    }
		  else 
		    {
		      // TODO: signal these strange cases and ignore
		      return 0;
		    }
		}	  
	    }
	  // type is state
	  if(bs_elem->type == BST_STATE)
	    {
	      // if the state is UP we basically ignore if it is on time or not
	      if(bs_elem->new_state == BST_ESTABLISHED)
		{
		  // TODO: update most_recent_ts
		  return 0;
		}
	      else
		{
		  // out of order that does not affect the current status
		  if(bs_elem->timestamp < peer_data->active_ribs_table->reference_rib_start)
		    {
		      // TODO: signal out of order and ignore
		      return 0;
		    }		  
		  else
		    {
		      // the state is !BST_ESTABLISHED and it affects the current status
		      // if it invalidates the current active ribs table
		      if(bs_elem->timestamp < peer_data->active_ribs_table->reference_rib_end)
			{
			  // TODO: go to PEER NULL
			  return 0;
			}
		      else 
			{
			  // TODO: go to PEER_DOWN
			  return 0;
			}
		    }
		  
		}
	    }
	}
      else 
	{ // assert(peer_data->rt_status == UC_ON)
	  // type is rib
	  if(bs_elem->type == BST_RIB)
	    {
	      // information arrived on time
	      if(bs_elem->timestamp >= peer_data->most_recent_ts) 
		{
		  // information relates to the rib under construction and it is a dump_middle
		  if(bs_record->attributes.dump_time == peer_data->uc_ribs_table->reference_dump_time &&
		     bs_record->dump_pos == DUMP_MIDDLE)
		    {
			  // TODO: apply rib to uc_rt, update time
			  return 0;
		    }
		  // information relates to the rib under construction and it is a dump_end
		  if(bs_record->attributes.dump_time == peer_data->uc_ribs_table->reference_dump_time &&
		     bs_record->dump_pos == DUMP_END)
		    {
		      // TODO: apply rib to uc_rt, update time
		      //       uc_rt is the new active (compare them)
		      //       go to UC_OFF
		      return 0;
		    }		  
		  // information relates to a newer rib
		  if(bs_record->attributes.dump_time > peer_data->uc_ribs_table->reference_dump_time &&
		      bs_record->dump_pos == DUMP_START)
		    {
		      // TODO: reset uc_rt, apply data to uc_rt still UC_ON
		      return 0;
		    }
		}
	      
	      // TODO: any other case is out of order or weird,
	      // we signal this, but we also ignore it (no change for any structure
	      return 0;
	    }
	  // type is update
	  if(bs_elem->type == BST_ANNOUNCEMENT || bs_elem->type == BST_WITHDRAWAL)
	    {
	      // information arrived on time
	      if(bs_elem->timestamp >= peer_data->most_recent_ts) 
		{
		  // TODO: we apply the update to active_rt and uc_rt
		  return 0;
		}
	      // information is out of order, but it does not affect uc_rt
	      if(bs_elem->timestamp < peer_data->uc_ribs_table->reference_rib_start &&
		 bs_elem->timestamp >= peer_data->active_ribs_table->reference_rib_start) 
		{
		  // TODO: reset active_rt, peer is now NULL, but UC continues to be ON		  
		  return 0;
		}
	      // information is out of order, it cannot be ignored by either ribs_tables
	      if(bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start)
		{
		  // TODO: reset active_rt, peer is now NULL, but UC continues to be ON	
		  // TODO: I think there is no chance to rollback here
		  return 0;
		}
	      // information is out of order, but it can be ignored
	      if(bs_elem->timestamp < peer_data->active_ribs_table->reference_rib_start) 
		{
		  // TODO: signal and ignore this case
		  return 0;
		}
	      // there is no other case
	    }
	  // type is STATE
	  if(bs_elem->type == BST_STATE)
	    {
	      // if the state is UP we basically ignore if it is on time or not
	      if(bs_elem->new_state == BST_ESTABLISHED)
		{
		  // TODO: update most_recent_ts (in case signal out of order)
		  return 0;
		}
	      else
		{
		  // the state is !BST_ESTABLISHED - i.e. down
		  // if active and uc are affected
		  if(bs_elem->timestamp >= peer_data->most_recent_ts)
		    {
		      // TODO: clear both active and uc and go to PEER_DOWN
		      return 0;
		    }
		  // of both of them are affected and data did not arrive on time
		  if(bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start)
		    {
		      // TODO: clear both active and uc and go to PEER_NULL
		      return 0;
		    }
		  // if active is affected, but uc is not
		  if(bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start &&
		     bs_elem->timestamp < peer_data->uc_ribs_table->reference_rib_start)
		    {
		      // TODO: go to PEER_NULL (active is cleared, UC is still on)
		      return 0;
		    }

		  // otherwise
		  // report this out of order and do nothing
		}
	    }
	}
    } // end of PEER_UP

  if(peer_data->status == PEER_DOWN)
    {
      // when a peer is down rt_status is always UC_OFF 
      // also both ribs_tables are empty
      // type is STATE
      if(bs_elem->type == BST_STATE)
	{
	  // if the state is UP
	  if(bs_elem->new_state == BST_ESTABLISHED &&
	     bs_elem->timestamp >= peer_data->most_recent_ts)
		{
		  // TODO: update most_recent_ts, now PEER is UP (empty ribs)
		  return 0;
		}
	  else
	    {
	      // TODO: update most_recent_ts (or signal out of order)
	      return 0;
	    }
	} // end of type STATE
      // type is UPDATE
      if(bs_elem->type == BST_ANNOUNCEMENT || bs_elem->type == BST_WITHDRAWAL)
	{
	  // if it is on time
	  if(bs_elem->timestamp >= peer_data->most_recent_ts)
	    {
	      // TODO: apply update to active ribs (that were empty), update ts, PEER is UP
	      return 0;
	    }
	  else 
	    {
	      // TODO: signal out of order and ignore
	      return 0;
	    }
	} // end of type UPDATE
      // type is RIB
      if(bs_elem->type == BST_RIB)
	{
	  // if it is on time and it is the first rib
	  if(bs_elem->timestamp >= peer_data->most_recent_ts &&
	     bs_record->dump_pos == DUMP_START) 
	    {
	      // TODO: UC is now ON, go to PEER is NULL
	      return 0;
	    }
	  else
	    {
	      // TODO: signal if out of order 
	      return 0;
	    }
	} // end of type RIB
    } // end of PEER_DOWN


  if(peer_data->status == PEER_NULL)
    {
      // in this case the active ribs are empty at all times
      // as they are not consistent
      if(peer_data->rt_status == UC_ON)
	{
	  // type is RIB
	  if(bs_elem->type == BST_RIB)
	    {
	      // if it is on time and we are already building the rib
	      if(bs_elem->timestamp >= peer_data->most_recent_ts &&
		 bs_record->attributes.dump_time == peer_data->uc_ribs_table->reference_dump_time) 
		{
		  // TODO: apply the rib elem and move on
		  // if it is the last set of elems, when the record
		  // signal the DUMP_END (using peerdata_apply_record) then
		  // we can move to another state
		  return 0;
		}	      
	      // if it is on time and we are starting with building a newer rib
	      if(bs_elem->timestamp >= peer_data->most_recent_ts &&
		 bs_record->dump_pos == DUMP_START &&
		 bs_record->attributes.dump_time > peer_data->uc_ribs_table->reference_dump_time) 
		{
		  // TODO: reset uc (if it is not empty) and continue to UC_ON
		  return 0;
		}
	      if(bs_elem->timestamp < peer_data->uc_ribs_table->reference_rib_start)
		{
		  // signal out of order
		  return 0;
		}
	      // if we are here we need to reset uc and move to uc_off
	      // TODO: go to UC_OFF
	      return 0;
	    } // end of type RIB
      	  // type is UPDATE
	  if(bs_elem->type == BST_ANNOUNCEMENT || bs_elem->type == BST_WITHDRAWAL)
	    {
	      if(bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start)
		{
		  // TODO: apply update and continue
		  return 0;
		}
	      // TODO: if here just ignore and signal out of order
	      return 0;
	    } // end of type UPDATE
	  // type is STATE
	  if(bs_elem->type == BST_STATE)
	    {
	      if(bs_elem->new_state != BST_ESTABLISHED &&
		 bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start)
		{
		  // TODO: reset everything, now the peer is down
		  return 0;
		}
	      // TODO: if here signal out of order and ignore
	      return 0;	      
	    } // end of type STATE
	} // end of UC_ON
      else // assert(peer_data->rt_status == UC_OFF)
	{
	  // type is RIB and we are just starting with a new rib
	  if(bs_elem->type == BST_RIB &&
	     bs_elem->timestamp >= peer_data->most_recent_ts &&
	     bs_record->dump_pos == DUMP_START &&
	     bs_record->attributes.dump_time > peer_data->uc_ribs_table->reference_dump_time) 
	    {
	      // TODO: reset uc (if it is not empty) and move to UC_ON
	      return 0;
	    }
	  // type is STATE and we received a peer DOWN
	  if(bs_elem->type == BST_STATE &&
	     bs_elem->timestamp >= peer_data->most_recent_ts &&
	     bs_elem->new_state != BST_ESTABLISHED) 
	    {
	      // TODO: go to peer down and continue to UC_OFF
	    }
	  // TODO: if here, just signal out of order and continue
	  return 0;
	} // end of UC_OFF
    } // end of PEER_NULL
  return 0;
}


/* apply the record to the peer_data provided:
 * - it returns 1 if the peer is active
 * - it returns 0 if the peer is down
 * - it return -1 if something went wrong */
int peerdata_apply_record(peerdata_t *peer_data, bgpstream_record_t * bs_record)
{
  assert(peer_data);
  assert(bs_record);
    
  /* Note:
   * bs_record statuses are put in a specific order
   * (most frequent on top) so we avoid to perform 
   * very unusual checks.
   */

  /* if we receive a VALID_RECORD we need to check
   * whether it provides data on-time or data out of
   * of order. Also if type is RIB and the record
   * dump_pos is either DUMP_START or DUMP_END then
   * we have to update the ribs_table structures
   * appropriately. 
   */
  if(bs_record->status == VALID_RECORD) 
    {      
      // TODO: see below
      return 0; // 1, -1
    }
      



      // TODO: in order?
      //       apply new time to most_recent_ts (if needed)
      
      // TODO: in time and DUMP_START !?
      //       if uc_rt already has the reference_dump_time
      //       set with the same dump, ok 
      //       else clear table and  set reference_dump_time 
      //       (the reference_rib_start will be set later)

      // TODO: in time and DUMP_END !?
      //       check uc_rt  reference_dump_time is the same
      //       as the current processed dump and in case
      //       swap active and under_construction tables

      // TODO: out of order ?
      //       yes: decide what to do!



  /* if we receive a record signaling a FILTERED_SOURCE
   * or an EMPTY_SOURCE of UPDATES
   * we do not need to check if it is in time-order or 
   * not (in any way it would have affected
   * our current dataset). However we update the
   * most_recent_ts (if necessary) */
  if(bs_record->status == FILTERED_SOURCE ||
     (bs_record->status == EMPTY_SOURCE && bs_record->attributes.dump_type == BGPSTREAM_UPDATE)) 
    {
      if(bs_record->attributes.record_time > peer_data->most_recent_ts) 
	{
	  peer_data->most_recent_ts = bs_record->attributes.record_time;
	}
      /* TODO: 
       *      => signal event in bgpribs log 
       */      
      return 0; // 1, -1
    }


  /* if we receive an EMPTY_SOURCE of RIBS we may update
   *  the mrt, however we do not do anything */
  if(bs_record->status == EMPTY_SOURCE && bs_record->attributes.dump_type == BGPSTREAM_RIB) 
    {
      if(bs_record->attributes.record_time > peer_data->most_recent_ts) 
	{
	  peer_data->most_recent_ts = bs_record->attributes.record_time;
	}
      /* TODO: 
       *      => signal event in bgpribs log */
      /* Comments: 
       * an empty rib is a pretty strange case. It means that the 
       * collector is most likely down. It could be that a peer just
       * went down and it is in the process of repopulating the rib
       * through updates. Imagine a small collector, all the peers
       * suffer an outage... :-O, ok that's maybe too much of imagination! 
       */
      return 0; // 1, -1
    }
  

  /* if we receive an CORRUPTED_SOURCE or a CORRUPTED_RECORD
   * we need to check if this information is affecting the current
   * status or not. In the first case, we need to invalidate ALL the
   * peers as we do not have any idea of which peers could have 
   * been affected by this corrupted data. In the latter case, we
   * just signal event in the bgpribs log*/  
  if(bs_record->status == CORRUPTED_SOURCE ||
     bs_record->status == CORRUPTED_RECORD) 
    {
      if(bs_record->attributes.record_time > peer_data->most_recent_ts) 
	{ 
	 peer_data-> most_recent_ts = bs_record->attributes.record_time;
	}      
      /* TODO: 
       *      => send this message to all the peers
       *         and decide if we turn the collector down or not
       *      => signal event in bgpribs log */
      return 0; // 1, -1
    }

  return 0; // 1, -1
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




