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

#include "bgpribs_peerdata.h"


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
   *  elem_types = [0,0,0,0]
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

  // aggregatable data
  if((peer_data->aggr_stats = malloc_zero(sizeof(aggregated_bgp_stats_t))) == NULL)
    {
      ribs_table_destroy(peer_data->uc_ribs_table);
      peer_data->uc_ribs_table = NULL;
      ribs_table_destroy(peer_data->active_ribs_table);
      peer_data->active_ribs_table = NULL;
      free(peer_data);
      return NULL;
    }

  peer_data->aggr_stats->unique_prefixes = NULL; // it is not used at peer level

  if((peer_data->aggr_stats->unique_origin_ases = ases_table_create()) == NULL)
    {
      free(peer_data->aggr_stats);
      peer_data->aggr_stats = NULL;
      ribs_table_destroy(peer_data->uc_ribs_table);
      peer_data->uc_ribs_table = NULL;
      ribs_table_destroy(peer_data->active_ribs_table);
      peer_data->active_ribs_table = NULL;
      free(peer_data);
      return NULL;
    }

  if((peer_data->aggr_stats->affected_prefixes = prefixes_table_create()) == NULL)
    {
      ases_table_destroy(peer_data->aggr_stats->unique_origin_ases);
      peer_data->aggr_stats->unique_origin_ases = NULL;      
      free(peer_data->aggr_stats);
      peer_data->aggr_stats = NULL;
      ribs_table_destroy(peer_data->uc_ribs_table);
      peer_data->uc_ribs_table = NULL;
      ribs_table_destroy(peer_data->active_ribs_table);
      peer_data->active_ribs_table = NULL;
      free(peer_data);
      return NULL;
    }

  if((peer_data->aggr_stats->announcing_origin_ases = ases_table_create()) == NULL)
    {
      prefixes_table_destroy(peer_data->aggr_stats->affected_prefixes);
      peer_data->aggr_stats->affected_prefixes = NULL;      
      ases_table_destroy(peer_data->aggr_stats->unique_origin_ases);
      peer_data->aggr_stats->unique_origin_ases = NULL;      
      free(peer_data->aggr_stats);
      peer_data->aggr_stats = NULL;
      ribs_table_destroy(peer_data->uc_ribs_table);
      peer_data->uc_ribs_table = NULL;
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
      ases_table_destroy(peer_data->aggr_stats->announcing_origin_ases);
      peer_data->aggr_stats->announcing_origin_ases = NULL;      
      prefixes_table_destroy(peer_data->aggr_stats->affected_prefixes);
      peer_data->aggr_stats->affected_prefixes = NULL;      
      ases_table_destroy(peer_data->aggr_stats->unique_origin_ases);
      peer_data->aggr_stats->unique_origin_ases = NULL;      
      free(peer_data->aggr_stats);
      peer_data->aggr_stats = NULL;
      ribs_table_destroy(peer_data->uc_ribs_table);
      peer_data->uc_ribs_table = NULL;
      ribs_table_destroy(peer_data->active_ribs_table);
      peer_data->active_ribs_table = NULL;
      free(peer_data);
      return NULL;
    }
  graphite_safe(peer_data->peer_address_str);

  return peer_data;
}


static void peerdata_log_event(peerdata_t *peer_data, 
			bgpstream_record_t * bs_record, bgpstream_elem_t *bs_elem)
{
  // TODO: print in log
  return;
  if(bs_elem != NULL)
    {
      // print bs_elem variables
      printf("Something weird in apply_elem\n");
      printf("\t %ld E %s - (%d - %d)\n", 
	     peer_data->most_recent_ts,
	     peer_data->peer_address_str,
	     peer_data->status, peer_data->rt_status);
      printf("\t %ld \t %d - dt: %ld dp: %d \n", 
	     bs_elem->timestamp,
	     bs_elem->type,
	     bs_record->attributes.dump_time,
	     bs_record->dump_pos);
    }
  else
    {
      printf("Something weird in apply_record\n");
      printf("\t %ld E %s - (%d - %d)\n", 
	     peer_data->most_recent_ts,
	     peer_data->peer_address_str,
	     peer_data->status, peer_data->rt_status);
      printf("\t %ld \t status: %d - dt: %d dp: %d\n", 
	     bs_record->attributes.dump_time,
	     bs_record->status,
	     bs_record->attributes.dump_type,
	     bs_record->dump_pos);
    }
}


static void peerdata_update_affected_resources(peerdata_t *peer_data, bgpstream_elem_t *bs_elem)
{
  if(bs_elem->type == BST_ANNOUNCEMENT)
    {
      prefixes_table_insert(peer_data->aggr_stats->affected_prefixes,bs_elem->prefix);
      if(bs_elem->aspath.hop_count > 0 && 
	 bs_elem->aspath.type == BST_UINT32_ASPATH ) 
	{
	  ases_table_insert(peer_data->aggr_stats->announcing_origin_ases,
			    bs_elem->aspath.numeric_aspath[(bs_elem->aspath.hop_count-1)]);
	}  
      return;
    }
  if(bs_elem->type == BST_WITHDRAWAL) 
    {
      prefixes_table_insert(peer_data->aggr_stats->affected_prefixes,bs_elem->prefix);
      return;
    }
}


int peerdata_apply_elem(peerdata_t *peer_data, bgpstream_record_t * bs_record, bgpstream_elem_t *bs_elem)
{
  assert(peer_data);
  assert(bs_record);
  assert(bs_record->status == VALID_RECORD);
  assert(bs_elem);

  peer_data->elem_types[bs_elem->type]++;

  // NOTE: there is no need to update peer_data->most_recent_ts, apply_record does that

  if(bs_elem->timestamp < peer_data->most_recent_ts)
    {
      peer_data->out_of_order++;
    }

  if(bs_elem->type == BST_STATE && bs_elem->new_state == BST_ESTABLISHED)
    {
      peer_data->state_up_elems++;
    }

  // type is update
  if(bs_elem->type == BST_ANNOUNCEMENT || bs_elem->type == BST_WITHDRAWAL)
    {
      // case 1
      if(peer_data->status == PEER_UP &&
	 ((bs_elem->timestamp >= peer_data->most_recent_ts) ||
	  (peer_data->most_recent_ts == peer_data->active_ribs_table->reference_rib_end &&
	   bs_elem->timestamp >= peer_data->active_ribs_table->reference_rib_start &&
	   bs_elem->timestamp <= peer_data->active_ribs_table->reference_rib_end) ||
	  (peer_data->rt_status == UC_ON && bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start) 
	  ))
	{
	  // apply update to the current active ribs (do not change status)
	  ribs_table_apply_elem(peer_data->active_ribs_table, bs_elem);
	  peerdata_update_affected_resources(peer_data,bs_elem);

	  // if we are not constructing a new RIB, we exit
	  if(!(peer_data->rt_status == UC_ON &&
	       bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start))
	    {
	      return 0;	      
	    }
	  // WARNING:
	  // if the previous condition is false, we need to update the current ribs which are
	  // under construction, to do so we use the next if (see below)
	}
      // case 2
      if(peer_data->rt_status == UC_ON &&
	 bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start)
	{
	  // apply update to the current uc ribs (do not change status)
	  ribs_table_apply_elem(peer_data->uc_ribs_table, bs_elem);
	  peerdata_update_affected_resources(peer_data,bs_elem);
	  return 0;
	}
      // case 3
      if(peer_data->status == PEER_UP &&
	 bs_elem->timestamp >= peer_data->active_ribs_table->reference_rib_start)
	{
	  // if none of the previous options have been triggered, it means that we
	  // just received an out of order that invalidates the current status
	  // i.e. the active rib, however, it does not affect the uc_ribs

	  // in this case we just rely on the soft-merge mechanism that is
	  // already embedded into the ribs_table_apply_elem mechanism
	  ribs_table_apply_elem(peer_data->active_ribs_table, bs_elem);
	  peerdata_update_affected_resources(peer_data,bs_elem);

	  peer_data->soft_merge_cnt++;

	  return 0;
	}
      // case 4
      if(peer_data->status == PEER_DOWN &&
	 bs_elem->timestamp >= peer_data->most_recent_ts)
	{	  
	  // go to PEER_UP and apply update to active ribs
	  peer_data->status = PEER_UP;
	  ribs_table_apply_elem(peer_data->active_ribs_table, bs_elem);
	  // this is an artifact, there is no "concrete" rib, just 
	  // updates that repopulate the active rib
	  peer_data->active_ribs_table->reference_dump_time = bs_record->attributes.dump_time;
	  peer_data->active_ribs_table->reference_rib_start = bs_elem->timestamp;
	  peer_data->active_ribs_table->reference_rib_end = bs_elem->timestamp;
	  return 0;
	}      
      // if here, we are ignoring this update as it is not useful
      //  (e.g UC_OFF and PEER_NULL) or it is out of order
      // we log at the end of the function
    }

  // type is RIB
  if(bs_elem->type == BST_RIB)
    {
      // case 5
      if(bs_record->dump_pos == DUMP_START && 
	 bs_elem->timestamp >= peer_data->most_recent_ts)
	{				     
	  peer_data->rt_status = UC_ON;
	  // check if it is the start of a newer rib or not
	  // in case reset the current uc_tables 
	  if(peer_data->uc_ribs_table->reference_dump_time < bs_record->attributes.dump_time)
	    {
	      // reset uc_table
	      ribs_table_reset(peer_data->uc_ribs_table);
	      peer_data->uc_ribs_table->reference_dump_time = bs_record->attributes.dump_time;
	      peer_data->uc_ribs_table->reference_rib_start = bs_elem->timestamp;
	      peer_data->uc_ribs_table->reference_rib_end = bs_elem->timestamp;
	    }
	  if(peer_data->status == PEER_DOWN)
	    {
	      peer_data->status = PEER_NULL;
	    }
	  // apply rib to uc_ribs_table
	  ribs_table_apply_elem(peer_data->uc_ribs_table, bs_elem);
	  return 0;
	}
      // case 6
      if((bs_record->dump_pos == DUMP_MIDDLE ||  
	  bs_record->dump_pos == DUMP_END) &&
	 bs_elem->timestamp >= peer_data->most_recent_ts &&
	 peer_data->rt_status == UC_ON &&
	 peer_data->uc_ribs_table->reference_dump_time == bs_record->attributes.dump_time)
	{
	  // if the PEER is DOWN and we finally receive something,
	  // then it goes to NULL status
	  if(peer_data->status == PEER_DOWN)
	    {
	      peer_data->status = PEER_NULL;
	    }
	  // CASE: if we are currently processing a rib (and the message we received 
	  // belong to that rib)
	  // if it is the first message that we receive for this rib
	  // then update the reference_rib_start
	  if(peer_data->uc_ribs_table->reference_rib_start == 0) 
	    {
	      peer_data->uc_ribs_table->reference_rib_start = bs_elem->timestamp;
	    }
	  peer_data->uc_ribs_table->reference_rib_end = bs_elem->timestamp;
	  // apply rib to uc_ribs_table	  
	  ribs_table_apply_elem(peer_data->uc_ribs_table, bs_elem);
	  return 0;
	}				     
      // if here, we are ignoring this rib as it is not useful
      // (e.g UC_OFF and !DUMP_START) or it is out of order
      // we log at the end of the function
    }

  // type is STATE
  if(bs_elem->type == BST_STATE)
    {
      // case 7
      if((bs_elem->new_state != BST_ESTABLISHED && 
	  bs_elem->timestamp >= peer_data->most_recent_ts) ||
	 (bs_elem->new_state != BST_ESTABLISHED && 
	  peer_data->rt_status == UC_ON &&
	  bs_elem->timestamp >= peer_data->uc_ribs_table->reference_rib_start) ||
	 (bs_elem->new_state != BST_ESTABLISHED && 
	  peer_data->rt_status == UC_OFF &&
	  peer_data->status == PEER_UP &&
	  bs_elem->timestamp >= peer_data->active_ribs_table->reference_rib_start))
	{				     
	  // this state message is invalidating the current status:
	  // the active tables, the uc_tables, or both of them

	  // reset everything and move to peer DOWN
	  peer_data->status = PEER_DOWN;
	  peer_data->rt_status = UC_OFF;

	  ribs_table_reset(peer_data->active_ribs_table);
	  // peer_data->active_ribs_table->reference_rib_start = 0;
	  // peer_data->active_ribs_table->reference_rib_end = 0;
	  // peer_data->active_ribs_table->reference_dump_time = 0;

	  ribs_table_reset(peer_data->uc_ribs_table);
	  // peer_data->uc_ribs_table->reference_rib_start = 0;
	  // peer_data->uc_ribs_table->reference_rib_end = 0;
	  // peer_data->uc_ribs_table->reference_dump_time = 0;

	  return 0;
	}
      // case 8
      if(bs_elem->new_state != BST_ESTABLISHED && 
	 peer_data->status == PEER_UP &&
	 peer_data->rt_status == UC_ON &&
	 bs_elem->timestamp >= peer_data->active_ribs_table->reference_rib_start &&
	 bs_elem->timestamp < peer_data->uc_ribs_table->reference_rib_start)
	{
	  // this message is invalidating the active ribs but not those
	  // that are under construction

	  // go to PEER_NULL, reset active tables,
	  // maintain UC_ON and uc_ribs tables
	  peer_data->status = PEER_NULL;

	  ribs_table_reset(peer_data->active_ribs_table);
	  // peer_data->active_ribs_table->reference_rib_start = 0;
	  // peer_data->active_ribs_table->reference_rib_end = 0;
	  // peer_data->active_ribs_table->reference_dump_time = 0;

	  return 0;
	}
      // case 9
      if(bs_elem->new_state == BST_ESTABLISHED && 
	 peer_data->status == PEER_DOWN &&
	 bs_elem->timestamp >= peer_data->most_recent_ts)
	{
	  // move to PEER_UP (with both active ribs empty, maintain uc ribs in whatever
	  // status they already were)
	  peer_data->status = PEER_UP;
	  peer_data->active_ribs_table->reference_rib_start = bs_elem->timestamp;
	  peer_data->active_ribs_table->reference_rib_end = bs_elem->timestamp;
	  peer_data->active_ribs_table->reference_dump_time = bs_elem->timestamp;
	  return 0;
	}

      // if here, we are ignoring this state message as it is not
      // meaningful for our purposes
      // we log at the end of the function
    }

  // if the program is here, we ignored the elem
  if(bs_elem->timestamp < peer_data->most_recent_ts)
    {
      peer_data->ignored_out_of_order++;
    }

  peer_data->ignored_elems++;

  return 0;
}


/* apply the record to the peer_data provided:
 * - it returns 1 if the peer is active (UP)
 * - it returns 0 if the peer is down   (DOWN OR NULL)
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
  // if necessary we update the most_recent_ts (all cases)
  if(peer_data->most_recent_ts < bs_record->attributes.record_time)
    {
      peer_data->most_recent_ts = bs_record->attributes.record_time;
    }
  
  /* if we receive a VALID_RECORD we need to check
   * whether it provides data on-time or data out of
   * of order. Also if type is RIB and the record
   * dump_pos is either DUMP_START or DUMP_END then
   * we have to update the ribs_table structures
   * appropriately. 
   */
  if(bs_record->status == VALID_RECORD) 
    {      
      // if we receive an "updated" RIB message and it is also
      // a dump start, we turn set UC_ON on all the peers
      if(bs_record->attributes.dump_type == BGPSTREAM_RIB && 
	 bs_record->dump_pos == DUMP_START && 
	 bs_record->attributes.record_time >= peer_data->most_recent_ts)
	{
	  // DEBUG:
	  /* if(strcmp(peer_data->peer_address_str, "202_79_197_122") == 0) */
	  /*   { */
	  /*     printf("Here: %ld - %ld\n", bs_record->attributes.dump_time, bs_record->attributes.record_time); */
	  /*   } */
	  peer_data->rt_status = UC_ON;
	  // Note: we turn UC_ON even if peer is DOWN, if no elem
	  // turns the rib into a NULL status, then the peer will 
	  // remain DOWN, and UC will be reset to OFF at the end 
	  // of the rib

	  // if no element has already set the reference_dump time, or if
	  // a newer dump has arrived
	  if(bs_record->attributes.dump_time > peer_data->uc_ribs_table->reference_dump_time)
	    {
	      // make sure that uc_ribs_table is empty
	      ribs_table_reset(peer_data->uc_ribs_table);
	      peer_data->uc_ribs_table->reference_dump_time = bs_record->attributes.dump_time;
	      // peer_data->uc_ribs_table->reference_rib_start = 0;
	      // peer_data->uc_ribs_table->reference_rib_end = 0;
	    }
	  return (peer_data->status == PEER_UP) ? 1 : 0;
	}

      // if we received an "updated" RIB message and it is a dump end
      // we set UC_OFF on all peers whose state was UC_ON
      if(bs_record->attributes.dump_type == BGPSTREAM_RIB && 
	 bs_record->dump_pos == DUMP_END && 
	 bs_record->attributes.record_time >= peer_data->most_recent_ts)
	{

	  // if this exact rib was under construction
	  if(peer_data->rt_status == UC_ON &&
	     bs_record->attributes.dump_time == peer_data->uc_ribs_table->reference_dump_time)
	    {
	      if(peer_data->status != PEER_DOWN)
		{
		  // uc_ribs_table is the new active, so:
		  // 1) reset(active)
		  ribs_table_reset(peer_data->active_ribs_table);
		  peer_data->active_ribs_table->reference_rib_start = 0;
		  peer_data->active_ribs_table->reference_rib_end = 0;
		  peer_data->active_ribs_table->reference_dump_time = 0;
		  // 2) tmp = active
		  ribs_table_t *tmp = peer_data->active_ribs_table;
		  // 3) active = uc_ribs
		  peer_data->active_ribs_table = peer_data->uc_ribs_table;
		  // 4) uc_ribs = tmp (i.e. already reset table)
		  peer_data->uc_ribs_table = tmp;
		  // the UC is OFF
		  peer_data->rt_status = UC_OFF;
		  if(peer_data->status == PEER_NULL) 
		    {
		      peer_data->status = PEER_UP;
		    }

		  // A new active rib is now in place
		  peer_data->new_rib = 1;
		  peer_data->new_rib_length = peer_data->active_ribs_table->reference_rib_end - 
		    peer_data->active_ribs_table->reference_rib_start;

		}
	      else
		{
		  // if the peer is still down, it will remain DOWN
		  peer_data->rt_status = UC_OFF;
		  peer_data->active_ribs_table->reference_rib_start = 0;
		  peer_data->active_ribs_table->reference_rib_end = 0;
		  peer_data->active_ribs_table->reference_dump_time = 0;
		}
	    }
	  return (peer_data->status == PEER_UP) ? 1 : 0;
	}     
      // no need to signal event if it is a valid record
      return (peer_data->status == PEER_UP) ? 1 : 0;
    }

  /* if we receive a record signaling a FILTERED_SOURCE
   * or an EMPTY_SOURCE of UPDATES
   * we do not need to check if it is in time-order or 
   * not (in any way it would have affected
   * our current dataset). However we update the
   * most_recent_ts (if necessary) */
  if(bs_record->status == FILTERED_SOURCE ||
     (bs_record->status == EMPTY_SOURCE && bs_record->attributes.dump_type == BGPSTREAM_UPDATE)) 
    {
      /* signal event in bgpribs log (see end of function) */      
    }


  /* if we receive an EMPTY_SOURCE of RIBS we may update
   *  the mr_ts, however we do not do anything */
  if(bs_record->status == EMPTY_SOURCE && bs_record->attributes.dump_type == BGPSTREAM_RIB) 
    {
      /* Comments: 
       * an empty rib is a pretty strange case. It means that the 
       * collector is most likely down. It could be that a peer just
       * went down and it is in the process of repopulating the rib
       * through updates. Imagine a small collector, all the peers
       * suffer an outage... :-O, ok that's maybe too much of imagination! 
       */

      /* signal event in bgpribs log (see end of function) */
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
      // if the peer was up, and the active table is affected
      if(peer_data->status == PEER_UP && 
	 bs_record->attributes.record_time >= peer_data->active_ribs_table->reference_rib_start)
	{
	  peer_data-> most_recent_ts = bs_record->attributes.record_time;
	  peer_data->status = PEER_NULL;
	  // reset active ribs
	  ribs_table_reset(peer_data->active_ribs_table);
	  peer_data->active_ribs_table->reference_rib_start = 0;
	  peer_data->active_ribs_table->reference_rib_end = 0;
	  peer_data->active_ribs_table->reference_dump_time = 0;
	  // check also if the rt_status was ON (see below)
	  if(!peer_data->rt_status == UC_ON)
	    {
	      return (peer_data->status == PEER_UP) ? 1 : 0;
	    }
	  // else the next if is triggered
	}
      // if the peer was building a UC_table and that rib is affected
      if(peer_data->rt_status == UC_ON && 
	 bs_record->attributes.record_time >= peer_data->uc_ribs_table->reference_rib_start)
	{
	  peer_data-> most_recent_ts = bs_record->attributes.record_time;
	  peer_data->status = PEER_NULL;
	  peer_data->rt_status = UC_OFF;
	  // reset uc ribs
	  ribs_table_reset(peer_data->uc_ribs_table);
	  peer_data->uc_ribs_table->reference_rib_start = 0;
	  peer_data->uc_ribs_table->reference_rib_end = 0;
	  peer_data->uc_ribs_table->reference_dump_time = 0;
	  return (peer_data->status == PEER_UP) ? 1 : 0;
	}

      // if peer was and uc is off, and the record is "on time"
      // (to check that we verify if it has the most_recent_ts 
      if(peer_data->status == PEER_DOWN && 
	 peer_data->most_recent_ts == bs_record->attributes.record_time)
	{
	  peer_data->status = PEER_NULL;
	  peer_data->rt_status = UC_OFF;
	  return (peer_data->status == PEER_UP) ? 1 : 0;
	}
      /* signal event in bgpribs log (see end of function) */
    }

  // logging events that are not considered meaningful for further processing
  peerdata_log_event(peer_data, bs_record, NULL);

  return (peer_data->status == PEER_UP) ? 1 : 0;
}


#ifdef WITH_BGPWATCHER
int peerdata_interval_end(char *project_str, char *collector_str,
			  bgpstream_ip_address_t * peer_address, peerdata_t *peer_data,
			  aggregated_bgp_stats_t * collector_aggr_stats,
			  bw_client_t *bw_client,
			  int interval_start)
#else
int peerdata_interval_end(char *project_str, char *collector_str,
			  bgpstream_ip_address_t * peer_address, peerdata_t *peer_data,
			  aggregated_bgp_stats_t * collector_aggr_stats,
			  int interval_start)
#endif
{
  khiter_t k;
  int khret;
  bgpstream_prefix_t prefix;
  prefixdata_t pd;
  uint32_t as;

 // OUTPUT METRIC: peer_status
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_status %d %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  // (peer_data->status-1) => { -1 NULL, 0 DOWN, 1 UP }
	  (peer_data->status - 1),
	  interval_start);

  // OUTPUT METRIC: elem_types[]
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_announcements_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_ANNOUNCEMENT],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_withdrawals_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_WITHDRAWAL],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_rib_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_RIB],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_state_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_STATE],
	  interval_start);

  // OUTPUT METRIC: state elem detail
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_state_established_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->state_up_elems,
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_state_down_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_STATE]-peer_data->state_up_elems,
	  interval_start);

  // OUTPUT METRIC: ignored elem
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_ignored_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->ignored_elems,
	  interval_start);

  // OUTPUT METRIC: out of order details
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_out_of_order_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->out_of_order,
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_soft_merge_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->soft_merge_cnt,
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_out_of_order_ignored_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->ignored_out_of_order,
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_out_of_order_in_rib_cnt %"PRIu64" %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  peer_data->out_of_order - (peer_data->soft_merge_cnt + peer_data->ignored_out_of_order),
	  interval_start);


  // OUTPUT METRIC:  new active rib related metrics
  if(peer_data->new_rib == 1)
    {
      fprintf(stdout,
	      METRIC_PREFIX".%s.%s.%s.new_rib_flag %"PRIu8" %d\n",
	      project_str,
	      collector_str,
	      peer_data->peer_address_str,
	      peer_data->new_rib,
	      interval_start);
      fprintf(stdout,
	      METRIC_PREFIX".%s.%s.%s.new_rib_length %"PRIu8" %d\n",
	      project_str,
	      collector_str,
	      peer_data->peer_address_str,
	      peer_data->new_rib_length,
	      interval_start);
    }

  // reset array and metrics
  memset(peer_data->elem_types, 0, sizeof(peer_data->elem_types));
  peer_data->state_up_elems = 0;
  peer_data->ignored_elems = 0;
  peer_data->out_of_order = 0;
  peer_data->soft_merge_cnt = 0;
  peer_data->ignored_out_of_order = 0;
  peer_data->new_rib = 0;
  peer_data->new_rib_length = 0;


  // OUTPUT METRIC: peer_affected_ipv4_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_affected_ipv4_prefixes_cnt %d %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  kh_size(peer_data->aggr_stats->affected_prefixes->ipv4_prefixes_table),
	  interval_start);

  // OUTPUT METRIC: peer_affected_ipv6_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_affected_ipv6_prefixes_cnt %d %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  kh_size(peer_data->aggr_stats->affected_prefixes->ipv6_prefixes_table),
	  interval_start);

  // OUTPUT METRIC: peer_announcing_origin_ases_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_announcing_origin_ases_cnt %d %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  kh_size(peer_data->aggr_stats->announcing_origin_ases->table),
	  interval_start);

  // "Aggregation" of affected resources per collector

  for(k = kh_begin(peer_data->aggr_stats->affected_prefixes->ipv4_prefixes_table);
      k != kh_end(peer_data->aggr_stats->affected_prefixes->ipv4_prefixes_table); ++k)
    {
      if (kh_exist(peer_data->aggr_stats->affected_prefixes->ipv4_prefixes_table, k))
	{
	  prefix = kh_key(peer_data->aggr_stats->affected_prefixes->ipv4_prefixes_table, k);
	  prefixes_table_insert(collector_aggr_stats->affected_prefixes, prefix);
	}
    }
  for(k = kh_begin(peer_data->aggr_stats->affected_prefixes->ipv6_prefixes_table);
      k != kh_end(peer_data->aggr_stats->affected_prefixes->ipv6_prefixes_table); ++k)
    {
      if (kh_exist(peer_data->aggr_stats->affected_prefixes->ipv6_prefixes_table, k))
	{
	  prefix = kh_key(peer_data->aggr_stats->affected_prefixes->ipv6_prefixes_table, k);
	  prefixes_table_insert(collector_aggr_stats->affected_prefixes, prefix);
	}
    }

  // then clear data
  prefixes_table_reset(peer_data->aggr_stats->affected_prefixes);

  for(k = kh_begin(peer_data->aggr_stats->announcing_origin_ases->table);
      k != kh_end(peer_data->aggr_stats->announcing_origin_ases->table); ++k)
    {
      if (kh_exist(peer_data->aggr_stats->announcing_origin_ases->table, k))
	{
	  as = kh_key(peer_data->aggr_stats->announcing_origin_ases->table, k);
	  ases_table_insert(collector_aggr_stats->announcing_origin_ases, as);
	}
    }

  // then clear data
  ases_table_reset(peer_data->aggr_stats->announcing_origin_ases);

  if(peer_data->status != PEER_UP)
    {
      return 0;
    }
  
  // the following actions require the peer to be UP

#ifdef WITH_BGPWATCHER
  int rc;
  uint32_t pfx_table_time = interval_start;
  // if the bgpwatcher client is enabled, then start to send the prefix table
  if((rc = bgpwatcher_client_pfx_table_begin(bw_client->pfx_table,
					     collector_str,
					     peer_address,
					     pfx_table_time)) < 0)
    {
      fprintf(stderr, "Could not begin prefix table\n");
      return -1;
    }    
#endif
  
  uint32_t ipv4_rib_size = 0;
  uint32_t ipv6_rib_size = 0;

  // go through ipv4 an ipv6 ribs and get the standard origin
  // ases, plus integrate the data into collector_data structs
  double avg_aspath_len_ipv4 = 0;
  double ipv4_size = kh_size(peer_data->active_ribs_table->ipv4_rib);
  double avg_aspath_len_ipv6 = 0;
  double ipv6_size = kh_size(peer_data->active_ribs_table->ipv6_rib);

  for(k = kh_begin(peer_data->active_ribs_table->ipv4_rib);
      k != kh_end(peer_data->active_ribs_table->ipv4_rib); ++k)
    {
      if (kh_exist(peer_data->active_ribs_table->ipv4_rib, k))
	{
	  // get prefix
	  prefix = kh_key(peer_data->active_ribs_table->ipv4_rib, k);
	  // get prefix_data
	  pd = kh_value(peer_data->active_ribs_table->ipv4_rib, k);
	  if(pd.is_active == 1) 
	    {
	      ipv4_rib_size++;
	      prefixes_table_insert(collector_aggr_stats->unique_prefixes, prefix);
	      if(pd.origin_as != 0)
		{
		  ases_table_insert(peer_data->aggr_stats->unique_origin_ases, pd.origin_as);
		  ases_table_insert(collector_aggr_stats->unique_origin_ases, pd.origin_as);
		}
	      avg_aspath_len_ipv4 += pd.aspath.hop_count;
#ifdef WITH_BGPWATCHER
	      if((rc = bgpwatcher_client_pfx_table_add(bw_client->pfx_table,
	      					       &prefix,
	      					       pd.origin_as)) < 0)
	      	{
	      	  bgpwatcher_client_perr(bw_client->client);
	      	  fprintf(stderr, "Could not add to pfx table\n");
	      	  return -1;
	      	}
#endif
	    }
	}
    }
  
  for(k = kh_begin(peer_data->active_ribs_table->ipv6_rib);
      k != kh_end(peer_data->active_ribs_table->ipv6_rib); ++k)
    {
      if (kh_exist(peer_data->active_ribs_table->ipv6_rib, k))
	{
	  // get prefix
	  prefix = kh_key(peer_data->active_ribs_table->ipv6_rib, k);
	  // get prefix_data
	  pd = kh_value(peer_data->active_ribs_table->ipv6_rib, k);
	  if(pd.is_active == 1) 
	    {
	      ipv6_rib_size++;
	      prefixes_table_insert(collector_aggr_stats->unique_prefixes, prefix);
	      if(pd.origin_as != 0)
		{
		  ases_table_insert(peer_data->aggr_stats->unique_origin_ases, pd.origin_as);
		  ases_table_insert(collector_aggr_stats->unique_origin_ases, pd.origin_as);
		}
	      avg_aspath_len_ipv6 += pd.aspath.hop_count;
#ifdef WITH_BGPWATCHER
	      if((rc = bgpwatcher_client_pfx_table_add(bw_client->pfx_table,
	      					       &prefix,
	      					       pd.origin_as)) < 0)
	      	{
	      	  bgpwatcher_client_perr(bw_client->client);
	      	  fprintf(stderr, "Could not add to pfx table\n");
	      	  return -1;
	      	}
#endif
	    }
	}
    }

#ifdef WITH_BGPWATCHER
  if((rc = bgpwatcher_client_pfx_table_end(bw_client->pfx_table)) < 0)
    {
      fprintf(stderr, "Could not end prefix table\n");
      return -1;
    }    
#endif

  // OUTPUT METRIC: ipv4_rib_size
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_ipv4_rib_size %d %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  ipv4_rib_size,
	  interval_start);

  // OUTPUT METRIC: ipv6_rib_size
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_ipv6_rib_size %d %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  ipv6_rib_size,
	  interval_start);

  // OUTPUT METRIC: unique_std_origin_ases_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_unique_std_origin_ases_cnt %d %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  kh_size(peer_data->aggr_stats->unique_origin_ases->table),
	  interval_start);

  
  // OUTPUT METRIC: peer_avg_aspathlen_ipv4
  if(ipv4_size > 0) 
    {
      avg_aspath_len_ipv4 = avg_aspath_len_ipv4 / ipv4_size;
    }
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_avg_aspathlen_ipv4 %f %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  avg_aspath_len_ipv4,
	  interval_start);

  // OUTPUT METRIC: peer_avg_aspathlen_ipv6
  if(ipv6_size > 0) 
    {
      avg_aspath_len_ipv6 = avg_aspath_len_ipv6 / ipv6_size;
    }
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_avg_aspathlen_ipv6 %f %d\n",
	  project_str,
	  collector_str,
	  peer_data->peer_address_str,
	  avg_aspath_len_ipv6,
	  interval_start);
  
  // reset per interval variables
  memset(peer_data->elem_types, 0, sizeof(peer_data->elem_types));
  ases_table_reset(peer_data->aggr_stats->unique_origin_ases);
  return 0;
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
      if(peer_data->aggr_stats != NULL)
	{
	  if(peer_data->aggr_stats->unique_origin_ases != NULL) 
	    {
	      ases_table_destroy(peer_data->aggr_stats->unique_origin_ases);
	      peer_data->aggr_stats->unique_origin_ases = NULL;
	    }
	  if(peer_data->aggr_stats->affected_prefixes != NULL) 
	    {
	      prefixes_table_destroy(peer_data->aggr_stats->affected_prefixes);
	      peer_data->aggr_stats->affected_prefixes = NULL;
	    }
	  if(peer_data->aggr_stats->announcing_origin_ases != NULL) 
	    {
	      ases_table_destroy(peer_data->aggr_stats->announcing_origin_ases);
	      peer_data->aggr_stats->announcing_origin_ases = NULL;
	    }
	  free(peer_data->aggr_stats);
	  peer_data->aggr_stats = NULL;
	}
      if(peer_data->peer_address_str != NULL) 
	{
	  free(peer_data->peer_address_str);
	  peer_data->peer_address_str = NULL;
	}
      free(peer_data);
    }
}

