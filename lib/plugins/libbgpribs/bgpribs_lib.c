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
#include "bgpribs_int.h"

#include <assert.h>
#include "utils.h"
#include <bgpwatcher_client.h>


#define METRIC_PREFIX "bgp.bgpribs"


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

/** ases_table related functions */

static ases_table_wrapper_t *ases_table_create() 
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

static void ases_table_insert(ases_table_wrapper_t *ases_table, uint32_t as) 
{
  assert(ases_table); 
  int khret;
  khiter_t k;
  if((k = kh_get(ases_table_t, ases_table->table,
			       as)) == kh_end(ases_table->table))
    {
      k = kh_put(ases_table_t, ases_table->table, 
		       as, &khret);
    }
}

static void ases_table_reset(ases_table_wrapper_t *ases_table) 
{
  assert(ases_table); 
  kh_clear(ases_table_t, ases_table->table);
}

static void ases_table_destroy(ases_table_wrapper_t *ases_table) 
{
  if(ases_table != NULL) 
    {
      kh_destroy(ases_table_t, ases_table->table);
      // free prefixes_table
      free(ases_table);
    }
}


/** prefixes_table related functions */

static prefixes_table_t *prefixes_table_create() 
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

static void prefixes_table_insert(prefixes_table_t *prefixes_table, bgpstream_prefix_t prefix)
{
  int khret;
  khiter_t k;
  if(prefix.number.type == BST_IPV4)
    {
      if((k = kh_get(ipv4_prefixes_table_t, prefixes_table->ipv4_prefixes_table,
		     prefix)) == kh_end(prefixes_table->ipv4_prefixes_table))
	{
	  k = kh_put(ipv4_prefixes_table_t, prefixes_table->ipv4_prefixes_table, 
		     prefix, &khret);
	}
    }
  else
    { // assert(prefix.number.type == BST_IPV6)
      if((k = kh_get(ipv6_prefixes_table_t, prefixes_table->ipv6_prefixes_table,
		     prefix)) == kh_end(prefixes_table->ipv6_prefixes_table))
	{
	  k = kh_put(ipv6_prefixes_table_t, prefixes_table->ipv6_prefixes_table, 
		     prefix, &khret);
	}
    }
}


static void prefixes_table_reset(prefixes_table_t *prefixes_table) 
{
  assert(prefixes_table);
  kh_clear(ipv4_prefixes_table_t, prefixes_table->ipv4_prefixes_table);
  kh_clear(ipv6_prefixes_table_t, prefixes_table->ipv6_prefixes_table);
}

static void prefixes_table_destroy(prefixes_table_t *prefixes_table) 
{
  if(prefixes_table == NULL) 
    {
      kh_destroy(ipv4_prefixes_table_t, prefixes_table->ipv4_prefixes_table);
      kh_destroy(ipv6_prefixes_table_t, prefixes_table->ipv6_prefixes_table);
      // free prefixes_table
      free(prefixes_table);
    }
}


/** ribs_table related functions */

static ribs_table_t *ribs_table_create() 
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

static void ribs_table_apply_elem(ribs_table_t *ribs_table, bgpstream_elem_t *bs_elem)
{
  khiter_t k;
  int khret;

  // prepare pd in case of insert
  prefixdata_t pd;
  pd.origin_as = 0;
  pd.is_active = 0; // if it is a withdrawal it will remain 0
  pd.ts = bs_elem->timestamp;
  prefixdata_t current_pd;

  if(bs_elem->type == BST_ANNOUNCEMENT || bs_elem->type == BST_RIB)
    {
      pd.is_active = 1;
      pd.aspath = bs_elem->aspath;
      // compute origin_as
      if(bs_elem->aspath.hop_count > 0 && 
	 bs_elem->aspath.type == BST_UINT32_ASPATH ) 
	{
	  pd.origin_as = bs_elem->aspath.numeric_aspath[(bs_elem->aspath.hop_count-1)];
	}  
    } 

  if(bs_elem->prefix.number.type == BST_IPV4) 
    { // ipv4 prefix
      k = kh_get(ipv4_rib_t, ribs_table->ipv4_rib,
		 bs_elem->prefix);
      // if it doesn't exist
      if(k == kh_end(ribs_table->ipv4_rib))
	{
	  k = kh_put(ipv4_rib_t, ribs_table->ipv4_rib, 
		     bs_elem->prefix, &khret);
	  kh_value(ribs_table->ipv4_rib, k) = pd;
	}
      else
	{
	  // updating the value only if the new timestamp is >= than the current one
	  // (if it is equal we assume data arrives in order and we apply it)
	  current_pd = kh_value(ribs_table->ipv4_rib, k);
	  if(pd.ts >= current_pd.ts)
	    {
	      kh_value(ribs_table->ipv4_rib, k) = pd;
	    }
	}
    }
  else
    { // ipv6 prefix  // assert(bs_elem->prefix.number.type == BST_IPV6) 
      k = kh_get(ipv6_rib_t, ribs_table->ipv6_rib,
		 bs_elem->prefix);
      // if it doesn't exist
      if(k == kh_end(ribs_table->ipv6_rib))
	{
	  k = kh_put(ipv6_rib_t, ribs_table->ipv6_rib, 
		     bs_elem->prefix, &khret);
	  kh_value(ribs_table->ipv6_rib, k) = pd;
	}
      else
	{
	  // updating the value only if the new timestamp is >= than the current one
	  // (if it is equal we assume data arrives in order and we apply it)
	  current_pd = kh_value(ribs_table->ipv6_rib, k);
	  if(pd.ts >= current_pd.ts)
	    {
	      kh_value(ribs_table->ipv6_rib, k) = pd;
	    }
	}
    }
  return;	
}



static void ribs_table_reset(ribs_table_t *ribs_table)
{
  ribs_table->reference_rib_start = 0;
  ribs_table->reference_rib_end = 0;
  ribs_table->reference_dump_time = 0;
  // ipv4_rib_t has static keys and values
  kh_clear(ipv4_rib_t, ribs_table->ipv4_rib);
  // ipv6_rib_t has static keys and values
  kh_clear(ipv6_rib_t,ribs_table->ipv6_rib);    
}


static void ribs_table_destroy(ribs_table_t *ribs_table) 
{
  if(ribs_table != NULL) 
    {
      kh_destroy(ipv4_rib_t, ribs_table->ipv4_rib);
      kh_destroy(ipv6_rib_t, ribs_table->ipv6_rib);
      // free ribs_table
      free(ribs_table);
    }
}


/** peerdata related functions */

static peerdata_t *peerdata_create(bgpstream_ip_address_t * peer_address)
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

  if((peer_data->unique_origin_ases = ases_table_create()) == NULL)
    {
      ribs_table_destroy(peer_data->uc_ribs_table);
      peer_data->uc_ribs_table = NULL;
      ribs_table_destroy(peer_data->active_ribs_table);
      peer_data->active_ribs_table = NULL;
      free(peer_data);
      return NULL;
    }

  if((peer_data->affected_prefixes = prefixes_table_create()) == NULL)
    {
      ases_table_destroy(peer_data->unique_origin_ases);
      peer_data->unique_origin_ases = NULL;      
      ribs_table_destroy(peer_data->uc_ribs_table);
      peer_data->uc_ribs_table = NULL;
      ribs_table_destroy(peer_data->active_ribs_table);
      peer_data->active_ribs_table = NULL;
      free(peer_data);
      return NULL;
    }

  if((peer_data->announcing_origin_ases = ases_table_create()) == NULL)
    {
      prefixes_table_destroy(peer_data->affected_prefixes);
      peer_data->affected_prefixes = NULL;      
      ases_table_destroy(peer_data->unique_origin_ases);
      peer_data->unique_origin_ases = NULL;      
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
      ases_table_destroy(peer_data->announcing_origin_ases);
      peer_data->announcing_origin_ases = NULL;      
      prefixes_table_destroy(peer_data->affected_prefixes);
      peer_data->affected_prefixes = NULL;      
      ases_table_destroy(peer_data->unique_origin_ases);
      peer_data->unique_origin_ases = NULL;      
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
      prefixes_table_insert(peer_data->affected_prefixes,bs_elem->prefix);
      if(bs_elem->aspath.hop_count > 0 && 
	 bs_elem->aspath.type == BST_UINT32_ASPATH ) 
	{
	  ases_table_insert(peer_data->announcing_origin_ases,bs_elem->aspath.numeric_aspath[(bs_elem->aspath.hop_count-1)]);
	}  
      return;
    }
  if(bs_elem->type == BST_WITHDRAWAL) 
    {
      prefixes_table_insert(peer_data->affected_prefixes,bs_elem->prefix);
      return;
    }
}


static int peerdata_apply_elem(peerdata_t *peer_data, 
			bgpstream_record_t * bs_record, bgpstream_elem_t *bs_elem)
{
  assert(peer_data);
  assert(bs_record);
  assert(bs_record->status == VALID_RECORD);
  assert(bs_elem);

  peer_data->elem_types[bs_elem->type]++;

  // NOTE: there is no need to update peer_data->most_recent_ts, apply_record does that

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

	  // in this case we just rely on the soft-rollback mechanism that is
	  // already embedded into the ribs_table_apply_elem mechanism
	  ribs_table_apply_elem(peer_data->active_ribs_table, bs_elem);
	  peerdata_update_affected_resources(peer_data,bs_elem);

	  // TODO: signal an out of order 

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
      //  (e.g UC_OFF and !DUMP_START) or it is out of order
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
  peer_data->elem_types[bs_elem->type]--;

  // logging events that are not considered meaningful for further processing
  peerdata_log_event(peer_data, bs_record, bs_elem);

  return 0;
}



/* apply the record to the peer_data provided:
 * - it returns 1 if the peer is active (UP)
 * - it returns 0 if the peer is down   (DOWN OR NULL)
 * - it return -1 if something went wrong */
static int peerdata_apply_record(peerdata_t *peer_data, bgpstream_record_t * bs_record)
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


static void peerdata_interval_end(peerdata_t *peer_data, int interval_start,
			   collectordata_t *collector_data) 
{
  khiter_t k;
  int khret;
  bgpstream_prefix_t prefix;
  prefixdata_t pd;
  uint32_t as;

 // OUTPUT METRIC: peer_status
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_status %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  // (peer_data->status-1) => { -1 NULL, 0 DOWN, 1 UP }
	  (peer_data->status - 1),
	  interval_start);

  // OUTPUT METRIC: elem_types[]
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_announcements_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_ANNOUNCEMENT],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_withdrawals_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_WITHDRAWAL],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_rib_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_RIB],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.elem_state_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  peer_data->elem_types[BST_STATE],
	  interval_start);

  // reset array
  memset(peer_data->elem_types, 0, sizeof(peer_data->elem_types));


  // OUTPUT METRIC: peer_affected_ipv4_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_affected_ipv4_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  kh_size(peer_data->affected_prefixes->ipv4_prefixes_table),
	  interval_start);

  // OUTPUT METRIC: peer_affected_ipv6_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_affected_ipv6_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  kh_size(peer_data->affected_prefixes->ipv6_prefixes_table),
	  interval_start);

  // OUTPUT METRIC: peer_announcing_origin_ases_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_announcing_origin_ases_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  kh_size(peer_data->announcing_origin_ases->table),
	  interval_start);

  // "Aggregation" of affected resources per collector

  for(k = kh_begin(peer_data->affected_prefixes->ipv4_prefixes_table);
      k != kh_end(peer_data->affected_prefixes->ipv4_prefixes_table); ++k)
    {
      if (kh_exist(peer_data->affected_prefixes->ipv4_prefixes_table, k))
	{
	  prefix = kh_key(peer_data->affected_prefixes->ipv4_prefixes_table, k);
	  prefixes_table_insert(collector_data->affected_prefixes, prefix);
	}
    }
  for(k = kh_begin(peer_data->affected_prefixes->ipv6_prefixes_table);
      k != kh_end(peer_data->affected_prefixes->ipv6_prefixes_table); ++k)
    {
      if (kh_exist(peer_data->affected_prefixes->ipv6_prefixes_table, k))
	{
	  prefix = kh_key(peer_data->affected_prefixes->ipv6_prefixes_table, k);
	  prefixes_table_insert(collector_data->affected_prefixes, prefix);
	}
    }

  // then clear data
  prefixes_table_reset(peer_data->affected_prefixes);

  for(k = kh_begin(peer_data->announcing_origin_ases->table);
      k != kh_end(peer_data->announcing_origin_ases->table); ++k)
    {
      if (kh_exist(peer_data->announcing_origin_ases->table, k))
	{
	  as = kh_key(peer_data->announcing_origin_ases->table, k);
	  ases_table_insert(collector_data->announcing_origin_ases, as);
	}
    }

  // then clear data
  ases_table_reset(peer_data->announcing_origin_ases);

  if(peer_data->status != PEER_UP)
    {
      return;
    }
  
  // the following actions require the peer to be UP
  
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
	      prefixes_table_insert(collector_data->unique_prefixes, prefix);
	      if(pd.origin_as != 0)
		{
		  ases_table_insert(peer_data->unique_origin_ases, pd.origin_as);
		  ases_table_insert(collector_data->unique_origin_ases, pd.origin_as);
		}
	      avg_aspath_len_ipv4 += pd.aspath.hop_count;
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
	      prefixes_table_insert(collector_data->unique_prefixes, prefix);
	      if(pd.origin_as != 0)
		{
		  ases_table_insert(peer_data->unique_origin_ases, pd.origin_as);
		  ases_table_insert(collector_data->unique_origin_ases, pd.origin_as);
		}
	      avg_aspath_len_ipv6 += pd.aspath.hop_count;
	    }
	}
    }

  // OUTPUT METRIC: ipv4_rib_size
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_ipv4_rib_size %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  ipv4_rib_size,
	  interval_start);

  // OUTPUT METRIC: ipv6_rib_size
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_ipv6_rib_size %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  ipv6_rib_size,
	  interval_start);

  // OUTPUT METRIC: unique_std_origin_ases_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_unique_std_origin_ases_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  kh_size(peer_data->unique_origin_ases->table),
	  interval_start);

  
  // OUTPUT METRIC: peer_avg_aspathlen_ipv4
  if(ipv4_size > 0) 
    {
      avg_aspath_len_ipv4 = avg_aspath_len_ipv4 / ipv4_size;
    }
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.peer_avg_aspathlen_ipv4 %f %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
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
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  peer_data->peer_address_str,
	  avg_aspath_len_ipv6,
	  interval_start);
  
  // reset per interval variables
  memset(peer_data->elem_types, 0, sizeof(peer_data->elem_types));
  ases_table_reset(peer_data->unique_origin_ases);
}


static void peerdata_destroy(peerdata_t *peer_data)
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
      if(peer_data->unique_origin_ases != NULL) 
	{
	  ases_table_destroy(peer_data->unique_origin_ases);
	  peer_data->unique_origin_ases = NULL;
	}
      if(peer_data->affected_prefixes != NULL) 
	{
	  prefixes_table_destroy(peer_data->affected_prefixes);
	  peer_data->affected_prefixes = NULL;
	}
      if(peer_data->announcing_origin_ases != NULL) 
	{
	  ases_table_destroy(peer_data->announcing_origin_ases);
	  peer_data->announcing_origin_ases = NULL;
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

static peers_table_t *peers_table_create() 
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
static int peers_table_process_record(peers_table_t *peers_table, 
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


static void peers_table_interval_end(peers_table_t *peers_table, int interval_start,
			       collectordata_t *collector_data)
{
  assert(peers_table);
  khiter_t k;
  peerdata_t * peer_data;

  // print stats for ipv4 peers
  for (k = kh_begin(peers_table->ipv4_peers_table);
       k != kh_end(peers_table->ipv4_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv4_peers_table, k))
	{
	  peer_data = kh_value(peers_table->ipv4_peers_table, k);
	  peerdata_interval_end(peer_data, interval_start, collector_data);
	}
    }   
  // print stats for ipv6 peers
  for (k = kh_begin(peers_table->ipv6_peers_table);
       k != kh_end(peers_table->ipv6_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv6_peers_table, k))
	{
	  peer_data = kh_value(peers_table->ipv6_peers_table, k);
	  peerdata_interval_end(peer_data, interval_start, collector_data);
	}
    }   
}


static void peers_table_destroy(peers_table_t *peers_table) 
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

static collectordata_t *collectordata_create(const char *project,
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

  if((collector_data->unique_prefixes = prefixes_table_create()) == NULL)
    {
      peers_table_destroy(collector_data->peers_table);
      collector_data->peers_table = NULL;
      free(collector_data->dump_collector);
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }

  if((collector_data->unique_origin_ases = ases_table_create()) == NULL)
    {
      prefixes_table_destroy(collector_data->unique_prefixes);
      collector_data->unique_prefixes = NULL;
      peers_table_destroy(collector_data->peers_table);
      collector_data->peers_table = NULL;
      free(collector_data->dump_collector);
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }

  if((collector_data->affected_prefixes = prefixes_table_create()) == NULL)
    {
      ases_table_destroy(collector_data->unique_origin_ases);
      collector_data->unique_origin_ases = NULL;
      prefixes_table_destroy(collector_data->unique_prefixes);
      collector_data->unique_prefixes = NULL;
      peers_table_destroy(collector_data->peers_table);
      collector_data->peers_table = NULL;
      free(collector_data->dump_collector);
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }

  if((collector_data->announcing_origin_ases = ases_table_create()) == NULL)
    {
      prefixes_table_destroy(collector_data->affected_prefixes);
      collector_data->affected_prefixes = NULL;
      ases_table_destroy(collector_data->unique_origin_ases);
      collector_data->unique_origin_ases = NULL;
      prefixes_table_destroy(collector_data->unique_prefixes);
      collector_data->unique_prefixes = NULL;
      peers_table_destroy(collector_data->peers_table);
      collector_data->peers_table = NULL;
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


static int collectordata_process_record(collectordata_t *collector_data,
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


static void collectordata_interval_end(collectordata_t *collector_data, 
				int interval_start)
{
  assert(collector_data);
  
  // OUTPUT METRIC: status
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_status %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  // (collector_data->status-1) => { -1 NULL, 0 DOWN, 1 UP }
	  (collector_data->status - 1),
	  interval_start);

  // OUTPUT METRIC: active_peers
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.active_peers_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->active_peers,
	  interval_start);


  // OUTPUT METRIC: record_types[]
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_valid_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[VALID_RECORD],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_filtered_source_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[FILTERED_SOURCE],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_empty_source_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[EMPTY_SOURCE],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_corrupted_source_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[CORRUPTED_SOURCE],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_corrupted_record_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[CORRUPTED_RECORD],
	  interval_start);
  // reset record type array
  memset(collector_data->record_types, 0, sizeof(collector_data->record_types));

  // the following function call the peer interval_end functions for 
  // each peer and populate the collector_data aggregated stats
  peers_table_interval_end(collector_data->peers_table, interval_start,
			   collector_data);
  

  // OUTPUT METRIC: collector_affected_ipv4_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_affected_ipv4_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->affected_prefixes->ipv4_prefixes_table),
	  interval_start);

  // OUTPUT METRIC: collector_affected_ipv6_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_affected_ipv6_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->affected_prefixes->ipv6_prefixes_table),
	  interval_start);

  prefixes_table_reset(collector_data->affected_prefixes);

  // OUTPUT METRIC: collector_announcing_origin_ases_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_announcing_origin_ases_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->announcing_origin_ases->table),
	  interval_start);
  
  ases_table_reset(collector_data->announcing_origin_ases);


  // OUTPUT METRIC: unique_ipv4_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_unique_ipv4_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->unique_prefixes->ipv4_prefixes_table),
	  interval_start);

  // OUTPUT METRIC: unique_ipv6_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_unique_ipv6_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->unique_prefixes->ipv6_prefixes_table),
	  interval_start);

  prefixes_table_reset(collector_data->unique_prefixes);

  // OUTPUT METRIC: unique_std_origin_ases_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_unique_std_origin_ases_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->unique_origin_ases->table),
	  interval_start);
  
  ases_table_reset(collector_data->unique_origin_ases);


  // Note: this metric has to be the last one
  // so it embeds the processing time at the end 
  // of each interval

  // OUTPUT METRIC: collector_realtime_delay
  time_t now = time(NULL); 
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_realtime_delay %ld %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  // difference between last record processed and now
	  (now - collector_data->most_recent_ts),
	  interval_start); 
}


static void collectordata_destroy(collectordata_t *collector_data)
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
      if(collector_data->unique_prefixes != NULL)
	{
	  prefixes_table_destroy(collector_data->unique_prefixes);
	  collector_data->unique_prefixes = NULL;
	}
      if(collector_data->unique_origin_ases != NULL)
	{
	  ases_table_destroy(collector_data->unique_origin_ases);
	  collector_data->unique_origin_ases = NULL;
	}
      if(collector_data->affected_prefixes != NULL)
	{
	  prefixes_table_destroy(collector_data->affected_prefixes);
	  collector_data->affected_prefixes = NULL;
	}
      if(collector_data->announcing_origin_ases != NULL)
	{
	  ases_table_destroy(collector_data->announcing_origin_ases);
	  collector_data->announcing_origin_ases = NULL;
	}
      free(collector_data);
    }
}


/* collectors_table related functions */

collectors_table_wrapper_t *collectors_table_create() 
{
  bgpwatcher_client_t *client = NULL;
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


/* dump statistics for each collector */
void collectors_table_interval_end(collectors_table_wrapper_t *collectors_table,
				   int interval_processing_start, int interval_start)
{
  assert(collectors_table != NULL); 
  khiter_t k;
  collectordata_t * collector_data;
  for (k = kh_begin(collectors_table->table);
       k != kh_end(collectors_table->table); ++k)
    {
      if (kh_exist(collectors_table->table, k))
	{
	  collector_data = kh_value(collectors_table->table, k);
	  // if the collector is in an unknown status we do not output
	  // information, this way we can merge different runs on our
	  // time series database
	  if(collector_data->status != COLLECTOR_NULL)
	    {
	      collectordata_interval_end(collector_data,interval_start);
	    }
	}
    }

  // if there is only 1 collector, then output its processing statistics
  // otherwise use the word "multiple"
  char collector_str[64];
  collector_str[0] = '\0';
  if(kh_size(collectors_table->table) == 1)
    {
      for(k = kh_begin(collectors_table->table);
	  k != kh_end(collectors_table->table); ++k)
	{
	  if (kh_exist(collectors_table->table, k))
	    {
	      collector_data = kh_value(collectors_table->table, k);
	      strcat(collector_str,collector_data->dump_project);
	      strcat(collector_str,".");
	      strcat(collector_str,collector_data->dump_collector);
	      break;
	    }
	}
    }
  else
    {
      strcat(collector_str,"multiple");
    }

  // OUTPUT METRIC: Interval processing time
  time_t now = time(NULL); 
  fprintf(stdout,
	  METRIC_PREFIX".%s.interval_processing_time %ld %d\n",
	  collector_str, (now - interval_processing_start),
	  interval_start);
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




