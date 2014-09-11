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

#include "config.h"
#include "bgpcorsaro_int.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bgpstream_lib.h"
#include "bgpdump_util.h"

#include "utils.h"
#include "wandio_utils.h"

#include "bgpcorsaro_io.h"
#include "bgpcorsaro_log.h"
#include "bgpcorsaro_plugin.h"

#include "khash.h"
#include "bgpcorsaro_bgpstats.h"

/** @file
 *
 * @brief Bgpcorsaro BgpStats plugin implementation
 *
 * @author Chiara Orsini
 *
 */

/** The number of output file pointers to support non-blocking close at the end
    of an interval. If the wandio buffers are large enough that it takes more
    than 1 interval to drain the buffers, consider increasing this number */
#define OUTFILE_POINTERS 2

/** The name of this plugin */
#define PLUGIN_NAME "bgpstats"

/** The version of this plugin */
#define PLUGIN_VERSION "0.1"

/** Common plugin information across all instances */
static bgpcorsaro_plugin_t bgpcorsaro_bgpstats_plugin = {
  PLUGIN_NAME,                                          /* name */
  PLUGIN_VERSION,                                       /* version */
  BGPCORSARO_PLUGIN_ID_BGPSTATS,                        /* id */
  BGPCORSARO_PLUGIN_GENERATE_PTRS(bgpcorsaro_bgpstats), /* func ptrs */
  BGPCORSARO_PLUGIN_GENERATE_TAIL,
};

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



/** bgpstream prefixes hashing and comparison */

static khint32_t bgpstream_prefix_ipv4_hash_func(bgpstream_prefix_t prefix)
{
  assert(prefix.number.type == BST_IPV4);
  assert(prefix.len >= 0);
  assert(prefix.len <= 32);
  khint32_t h = 0;
  // convert network byte order to host byte order
  // ipv4 32 bits number (in host order)
  uint32_t address = ntohl(prefix.number.address.v4_addr.s_addr);  
  // embed the network mask length in the 32 bits
  h = address | (uint32_t) prefix.len;
  return __ac_Wang_hash(h); // decreases the chances of collisions
}

static khint64_t bgpstream_prefix_ipv6_hash_func(bgpstream_prefix_t prefix) 
{
  assert(prefix.number.type == BST_IPV6);
  assert(prefix.len >= 0);
  if(prefix.len > 64) { return 0; }
  // check type is ipv6 and prefix length is correct* - we discard mask > 64
  khint64_t h = 0;
  // ipv6 number - we take most significative 64 bits only (in host order)
  uint64_t address = *((uint64_t *) &(prefix.number.address.v6_addr.s6_addr[0]));
  address = ntohll(address);
  // embed the network mask length in the 64 bits
  h = address | (uint64_t) prefix.len;
  return __ac_Wang_hash(h);  // decreases the chances of collisions
}


int bgpstream_prefix_ipv4_hash_equal(bgpstream_prefix_t prefix1,
				     bgpstream_prefix_t prefix2)
{
  assert(prefix1.number.type == BST_IPV4); // check type is ipv4
  assert(prefix2.number.type == BST_IPV4); // check type is ipv4
  return (prefix1.number.address.v4_addr.s_addr == prefix2.number.address.v4_addr.s_addr) &&
    (prefix1.len == prefix2.len);
}

int bgpstream_prefix_ipv6_hash_equal(bgpstream_prefix_t prefix1,
				     bgpstream_prefix_t prefix2) 
{
  assert(ip1.type == BST_IPV6); // check type is ipv6
  assert(ip2.type == BST_IPV6); // check type is ipv6
  return memcmp(&prefix1,&prefix2, sizeof(bgpstream_ip_address_t));
}



/** ribs table (khash) related funcions */

typedef struct struct_prefixdata_t {
  uint32_t origin_as;  
} prefixdata_t;

KHASH_INIT(ipv4_rib_t /* name */, 
	   bgpstream_prefix_t /* khkey_t */, 
	   prefixdata_t /* khval_t */, 
	   1  /* kh_is_map */, 
	   bgpstream_prefix_ipv4_hash_func /*__hash_func */,  
	   bgpstream_prefix_ipv4_hash_equal /* __hash_equal */);


KHASH_INIT(ipv6_rib_t /* name */, 
	   bgpstream_prefix_t /* khkey_t */, 
	   prefixdata_t /* khval_t */, 
	   1  /* kh_is_map */, 
	   bgpstream_prefix_ipv6_hash_func /*__hash_func */,  
	   bgpstream_prefix_ipv6_hash_equal /* __hash_equal */);


typedef struct struct_ribs_table_t {
  khash_t(ipv4_rib_t) * ipv4_rib;
  khash_t(ipv6_rib_t) * ipv6_rib;
} ribs_table_t;


/** prefixes_table related functions */

static ribs_table_t *ribs_table_create() 
{
  ribs_table_t *ribs_table;
  if((ribs_table = malloc_zero(sizeof(ribs_table_t))) == NULL)
    {
      return NULL;
    }
  // init ipv4 and ipv6 ribs khashes
  ribs_table->ipv4_rib = kh_init(ipv4_rib_t);
  ribs_table->ipv6_rib = kh_init(ipv6_rib_t);
  return ribs_table;
}


static void ribs_table_reset(ribs_table_t *ribs_table) 
{  
  assert(ribs_table != NULL);
  /** we remove prefixes from each set (ipv4 and ipv6) 
   *  without deallocating memory  */
  kh_clear(ipv4_rib_t, ribs_table->ipv4_rib);
  kh_clear(ipv6_rib_t, ribs_table->ipv6_rib);
}

static void ribs_table_destroy(ribs_table_t *ribs_table) 
{
  if(ribs_table == NULL) 
    {
      return;
    }
  kh_destroy(ipv4_rib_t, ribs_table->ipv4_rib);
  kh_destroy(ipv6_rib_t, ribs_table->ipv6_rib);
  // free ribs_table
  free(ribs_table);
}



/** ases_table (khash) related functions */

KHASH_INIT(ases_table_t /* name */, 
	   uint32_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   kh_int_hash_func /*__hash_func */,  
	   kh_int_hash_equal /* __hash_equal */);

typedef struct struct_ases_table_wrapper_t {
  khash_t(ases_table_t) * table;
} ases_table_wrapper_t;

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

static void ases_table_reset(ases_table_wrapper_t *ases_table) 
{  
  assert(ases_table != NULL);
  /** we remove ases without deallocating memory  */
  kh_clear(ases_table_t, ases_table->table);
}


static void ases_table_destroy(ases_table_wrapper_t *ases_table) 
{
  if(ases_table == NULL) 
    {
      return;
    }
  kh_destroy(ases_table_t, ases_table->table);
  // free prefixes_table
  free(ases_table);
}



/** prefixes_table (khash) related functions */

KHASH_INIT(ipv4_prefixes_table_t /* name */, 
	   bgpstream_prefix_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   bgpstream_prefix_ipv4_hash_func /*__hash_func */,  
	   bgpstream_prefix_ipv4_hash_equal /* __hash_equal */);


KHASH_INIT(ipv6_prefixes_table_t /* name */, 
	   bgpstream_prefix_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_map */, 
	   bgpstream_prefix_ipv6_hash_func /*__hash_func */,  
	   bgpstream_prefix_ipv6_hash_equal /* __hash_equal */);



typedef struct struct_prefixes_table_t {
  khash_t(ipv4_prefixes_table_t) * ipv4_prefixes_table;
  khash_t(ipv6_prefixes_table_t) * ipv6_prefixes_table;
} prefixes_table_t;


/** prefixes_table related functions */

static prefixes_table_t *prefixes_table_create() 
{
  prefixes_table_t *prefixes_table;
  if((prefixes_table = malloc_zero(sizeof(prefixes_table_t))) == NULL)
    {
      return NULL;
    }
  // init ipv4 and ipv6 peers khashes
  prefixes_table->ipv4_prefixes_table = kh_init(ipv4_prefixes_table_t);
  prefixes_table->ipv6_prefixes_table = kh_init(ipv6_prefixes_table_t);
  return prefixes_table;
}


static void prefixes_table_reset(prefixes_table_t *prefixes_table) 
{  
  assert(prefixes_table != NULL);
  /** we remove prefixes from each set (ipv4 and ipv6) 
   *  without deallocating memory  */
  kh_clear(ipv4_prefixes_table_t, prefixes_table->ipv4_prefixes_table);
  kh_clear(ipv6_prefixes_table_t, prefixes_table->ipv6_prefixes_table);
}


static void prefixes_table_destroy(prefixes_table_t *prefixes_table) 
{
  if(prefixes_table == NULL) 
    {
      return;
    }
  kh_destroy(ipv4_prefixes_table_t, prefixes_table->ipv4_prefixes_table);
  kh_destroy(ipv6_prefixes_table_t, prefixes_table->ipv6_prefixes_table);
  // free prefixes_table
  free(prefixes_table);
}


/** peerdata related functions */

typedef struct struct_peerdata_t {
  // number of elems of a given type processed in the current interval
  uint64_t num_elem[BGPSTREAM_ELEM_TYPE_MAX];
  // ribs table
  ribs_table_t * ribs_table;
  // prefixes "modified" in the current interval
  prefixes_table_t * affected_prefixes_table;
  // occurrencies of non_standard origin ases announced current interval (updates only)
  int non_std_origin_as_cnt;
  // occurrencies of internal updates (i.e. aspath is empty)
  int empty_origin_as_cnt;
  // unique ases that appear as origin in the ribs
  ases_table_wrapper_t * ases_table;
  // origin ases active in the current interval (i.e. announcing)
  ases_table_wrapper_t * announcing_ases_table;
} peerdata_t;

static peerdata_t *peerdata_create()
{
  peerdata_t *peer_data;
  if((peer_data = malloc_zero(sizeof(peerdata_t))) == NULL)
    {
      return NULL;
    }
  if((peer_data->ribs_table = ribs_table_create()) == NULL)
    {
      free(peer_data);
      return NULL;
    }
  if((peer_data->affected_prefixes_table = prefixes_table_create()) == NULL)
    {
      ribs_table_destroy(peer_data->ribs_table);
      free(peer_data);
      return NULL;
    }
  if((peer_data->ases_table = ases_table_create()) == NULL)
    {
      prefixes_table_destroy(peer_data->affected_prefixes_table);
      ribs_table_destroy(peer_data->ribs_table);
      free(peer_data);
      return NULL;
    }
  if((peer_data->announcing_ases_table = ases_table_create()) == NULL)
    {
      ases_table_destroy(peer_data->ases_table);
      prefixes_table_destroy(peer_data->affected_prefixes_table);
      ribs_table_destroy(peer_data->ribs_table);
      free(peer_data);
      return NULL;
    }
  return peer_data;
}


static void peerdata_update(bgpstream_elem_t *elem,
			    peerdata_t *peer_data)
{
  assert(elem);
  assert(peer_data);
  assert(peer_data->ribs_table);
  assert(peer_data->prefixes_table->ipv4_prefixes_table);
  assert(peer_data->prefixes_table->ipv6_prefixes_table);
  khiter_t k;
  int khret;

  peer_data->num_elem[elem->type]++;

  /* check if it is a state message, and if it is 
   * a peer down message, then clear the tables */
  if(elem->type == BST_STATE) {
    if(elem->new_state != BST_ESTABLISHED) {
      /* we remove all the prefixes from the ribs */
      ribs_table_reset(peer_data->ribs_table);
    }
    return;
  }

  // creating prefixdata and populating the structure
  prefixdata_t pd;
  pd.origin_as = 0;
  if( (elem->type == BST_RIB || elem->type == BST_ANNOUNCEMENT) &&
      elem->aspath.hop_count > 0 && 
      elem->aspath.type == BST_UINT32_ASPATH ) {
      pd.origin_as = elem->aspath.numeric_aspath[(elem->aspath.hop_count-1)];
  }

  /* Updating the ribs  */
  if(elem->prefix.number.type == BST_IPV4) { // ipv4 prefix
    k = kh_get(ipv4_rib_t, peer_data->ribs_table->ipv4_rib,
	       elem->prefix);
    // update prefix into ipv4 prefixes table
    if(elem->type == BST_RIB || elem->type == BST_ANNOUNCEMENT)
      {
	// insert key if it doesn't exist
	if(k == kh_end(peer_data->ribs_table->ipv4_rib))
	  {
	    k = kh_put(ipv4_rib_t, peer_data->ribs_table->ipv4_rib, 
		       elem->prefix, &khret);
	  }
	// updating the prefixdata structure
	kh_value(peer_data->ribs_table->ipv4_rib, k) = pd;
      }
    else
      {
	if(elem->type == BST_WITHDRAWAL) 
	  {
	    // remove if it exists
	    if(k != kh_end(peer_data->ribs_table->ipv4_rib))
	      {
		kh_del(ipv4_rib_t, peer_data->ribs_table->ipv4_rib, k);
	      }  
	  }
      }
  }
  else { // ipv6 prefix
    // update prefix into ipv6 prefixes table
    k = kh_get(ipv6_rib_t, peer_data->ribs_table->ipv6_rib,
	       elem->prefix);
    // update prefix into ipv4 prefixes table
    if(elem->type == BST_RIB || elem->type == BST_ANNOUNCEMENT)
      {
	// insert if it doesn't exist
	if(k == kh_end(peer_data->ribs_table->ipv6_rib))
	  {
	    k = kh_put(ipv6_rib_t, peer_data->ribs_table->ipv6_rib, 
		       elem->prefix, &khret);
	  }
	// updating the prefixdata structure
	kh_value(peer_data->ribs_table->ipv6_rib, k) = pd;
      }
    else
      {
	if(elem->type == BST_WITHDRAWAL) 
	  {
	    // remove if it exists
	    if(k != kh_end(peer_data->ribs_table->ipv6_rib))
	      {
		kh_del(ipv6_rib_t, peer_data->ribs_table->ipv6_rib, k);
	      }  
	  }
      }
  }

  /* Updating affected_prefixes_table */
  if(elem->prefix.number.type == BST_IPV4) { // ipv4 prefix
    k = kh_get(ipv4_prefixes_table_t, peer_data->affected_prefixes_table->ipv4_prefixes_table,
	       elem->prefix);
    // update prefix into ipv4 affected prefixes table
    if(elem->type == BST_ANNOUNCEMENT || elem->type == BST_WITHDRAWAL)
      {
	// insert if it doesn't exist
	if(k == kh_end(peer_data->affected_prefixes_table->ipv4_prefixes_table))
	  {
	    k = kh_put(ipv4_prefixes_table_t, peer_data->affected_prefixes_table->ipv4_prefixes_table, 
		       elem->prefix, &khret);
	  }
      }
  }
  else { // ipv6 prefix
    // update prefix into ipv6 affected prefixes table
    k = kh_get(ipv6_prefixes_table_t, peer_data->affected_prefixes_table->ipv6_prefixes_table,
	       elem->prefix);
    // update prefix into ipv4 prefixes table
    if(elem->type == BST_ANNOUNCEMENT || elem->type == BST_WITHDRAWAL)
      {
	// insert if it doesn't exist
	if(k == kh_end(peer_data->affected_prefixes_table->ipv6_prefixes_table))
	  {
	    k = kh_put(ipv6_prefixes_table_t, peer_data->affected_prefixes_table->ipv6_prefixes_table, 
		       elem->prefix, &khret);
	  }
      }
  }

  // updating affected ases and "internal/non_std" updates count
  uint32_t origin_as;    
  if(elem->type == BST_ANNOUNCEMENT) 
    {
      if(elem->aspath.hop_count == 0)
	{ // received an internal update 
	  peer_data->empty_origin_as_cnt++;
	}
      else
	{
	  // update origin ASes announcing a change in this interval
	  if(elem->aspath.type == BST_UINT32_ASPATH && elem->aspath.hop_count > 0) 
	    { 
	      origin_as = elem->aspath.numeric_aspath[(elem->aspath.hop_count-1)];
	      k = kh_get(ases_table_t, peer_data->announcing_ases_table->table, origin_as);
	      if(k == kh_end(peer_data->announcing_ases_table->table))
		{
		  k = kh_put(ases_table_t, peer_data->announcing_ases_table->table, 
			     origin_as, &khret);
		}
	    }
	  else
	    { 
	      // if the path is a string and the hop count is zero, then it is non_std
	      // aspath
	      if(elem->aspath.type == BST_STRING_ASPATH && elem->aspath.hop_count > 0) 
		{
		  peer_data->non_std_origin_as_cnt++;
		}
	    }
	}
    }

}

// get prefixes and ases all in one round - global and local
static void ribs_table_parser(prefixes_table_t *global_prefixes, 
			      ases_table_wrapper_t *local_ases, ases_table_wrapper_t *global_ases, 
			      ribs_table_t * to_read)
{
  khiter_t k;
  khiter_t k_check;
  int khret;
  bgpstream_prefix_t prefix;
  prefixdata_t pd; 
  // update ipv4 prefixes table
  for(k = kh_begin(to_read->ipv4_rib);
      k != kh_end(to_read->ipv4_rib); ++k)
    {
      if (kh_exist(to_read->ipv4_rib, k))
	{
	  // get prefix from "to_read" table
	  prefix = kh_key(to_read->ipv4_rib, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ipv4_prefixes_table_t, global_prefixes->ipv4_prefixes_table,
			       prefix)) == kh_end(global_prefixes->ipv4_prefixes_table))
	    {
	      k_check = kh_put(ipv4_prefixes_table_t, global_prefixes->ipv4_prefixes_table, 
			       prefix, &khret);
	    }
	  // get prefixdata from "to read" table
	  pd = kh_value(to_read->ipv4_rib, k);
	  if( pd.origin_as != 0)  // 0 means no "standard" as origin
	    {
	      // insert if it does not exist in local and global ases
	      if((k_check = kh_get(ases_table_t, local_ases->table,
				   pd.origin_as)) == kh_end(local_ases->table))
		{
		  k_check = kh_put(ases_table_t, local_ases->table, pd.origin_as, &khret);
		}
	      if((k_check = kh_get(ases_table_t, global_ases->table,
				   pd.origin_as)) == kh_end(global_ases->table))
		{
		  k_check = kh_put(ases_table_t, global_ases->table, pd.origin_as, &khret);
		}	  
	    }
	}
    }
  // update ipv6 prefixes table
  for(k = kh_begin(to_read->ipv6_rib);
      k != kh_end(to_read->ipv6_rib); ++k)
    {
      if (kh_exist(to_read->ipv6_rib, k))
	{
	  // get prefix from "to_read" table
	  prefix = kh_key(to_read->ipv6_rib, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ipv6_prefixes_table_t, global_prefixes->ipv6_prefixes_table,
			       prefix)) == kh_end(global_prefixes->ipv6_prefixes_table))
	    {
	      k_check = kh_put(ipv6_prefixes_table_t, global_prefixes->ipv6_prefixes_table, 
			       prefix, &khret);
	    }
	  // get prefixdata from "to read" table
	  pd = kh_value(to_read->ipv6_rib, k);
	  if( pd.origin_as != 0)  // 0 means no "standard" as origin
	    {
	      // insert if it does not exist in local and global ases
	      if((k_check = kh_get(ases_table_t, local_ases->table,
				   pd.origin_as)) == kh_end(local_ases->table))
		{
		  k_check = kh_put(ases_table_t, local_ases->table, pd.origin_as, &khret);
		}
	      if((k_check = kh_get(ases_table_t, global_ases->table,
				   pd.origin_as)) == kh_end(global_ases->table))
		{
		  k_check = kh_put(ases_table_t, global_ases->table, pd.origin_as, &khret);
		}
	    }
	}
    }
}


static void prefixes_table_union(prefixes_table_t * to_update, prefixes_table_t * to_read)
{
  khiter_t k;
  khiter_t k_check;
  int khret;
  bgpstream_prefix_t prefix; 
  // update ipv4 prefixes table
  for(k = kh_begin(to_read->ipv4_prefixes_table);
      k != kh_end(to_read->ipv4_prefixes_table); ++k)
    {
      if (kh_exist(to_read->ipv4_prefixes_table, k))
	{
	  // get prefix from "to_read" table
	  prefix = kh_key(to_read->ipv4_prefixes_table, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ipv4_prefixes_table_t, to_update->ipv4_prefixes_table,
			       prefix)) == kh_end(to_update->ipv4_prefixes_table))
	    {
	      k_check = kh_put(ipv4_prefixes_table_t, to_update->ipv4_prefixes_table, 
			       prefix, &khret);
	    }
	}
    }
  // update ipv6 prefixes table
  for(k = kh_begin(to_read->ipv6_prefixes_table);
      k != kh_end(to_read->ipv6_prefixes_table); ++k)
    {
      if (kh_exist(to_read->ipv6_prefixes_table, k))
	{
	  // get prefix from "to_read" table
	  prefix = kh_key(to_read->ipv6_prefixes_table, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ipv6_prefixes_table_t, to_update->ipv6_prefixes_table,
			       prefix)) == kh_end(to_update->ipv6_prefixes_table))
	    {
	      k_check = kh_put(ipv6_prefixes_table_t, to_update->ipv6_prefixes_table, 
			       prefix, &khret);
	    }
	}
    }
}


static void ases_table_union(ases_table_wrapper_t * to_update, ases_table_wrapper_t * to_read)
{
  khiter_t k;
  khiter_t k_check;
  int khret;
  uint32_t origin_as;
  // update ases table
  for(k = kh_begin(to_read->table);
      k != kh_end(to_read->table); ++k)
    {
      if (kh_exist(to_read->table, k))
	{
	  // get prefix from "to_read" table
	  origin_as = kh_key(to_read->table, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ases_table_t, to_update->table,
			       origin_as)) == kh_end(to_update->table))
	    {
	      k_check = kh_put(ases_table_t, to_update->table, 
			       origin_as, &khret);
	    }
	}
    }
}


static void peerdata_new_rib(peerdata_t *peer_data)
{
  assert(peer_data != NULL); 
  assert(peer_data->prefixes_table != NULL); 
  assert(peer_data->ases_table != NULL); 
  /* we remove all the prefixes from the tables */
  ribs_table_reset(peer_data->ribs_table);
  /* we remove all the ASes from the table */
  ases_table_reset(peer_data->ases_table);
}

static void peerdata_end_of_interval(peerdata_t *peer_data)
{
  assert(peer_data != NULL); 
  memset(peer_data->num_elem, 0, sizeof(peer_data->num_elem));
  // we remove all the prefixes affected in the interval
  prefixes_table_reset(peer_data->affected_prefixes_table);
  // we remove all the ases (we recompute them at every interval)
  ases_table_reset(peer_data->ases_table);
  // we remove all the ases affected in the interval
  ases_table_reset(peer_data->announcing_ases_table);
  // we reset the number of non standard origin ASes events
  peer_data->non_std_origin_as_cnt = 0;
  // we reset the number of empty origin ASes events
  peer_data->empty_origin_as_cnt = 0;
}

static void peerdata_destroy(peerdata_t *peer_data)
{
  if(peer_data == NULL) 
    {
      return;
    }
  if(peer_data->ribs_table != NULL) 
    {
      ribs_table_destroy(peer_data->ribs_table);
      peer_data->ribs_table = NULL;
    }
  if(peer_data->affected_prefixes_table != NULL) 
    {
      prefixes_table_destroy(peer_data->affected_prefixes_table);
      peer_data->affected_prefixes_table = NULL;
    }
  if(peer_data->ases_table != NULL) 
    {
      ases_table_destroy(peer_data->ases_table);
      peer_data->ases_table = NULL;
    }
  if(peer_data->announcing_ases_table != NULL) 
    {
      ases_table_destroy(peer_data->announcing_ases_table);
      peer_data->announcing_ases_table = NULL;
    }
  free(peer_data);
}


/** Peer_table (khash) related functions */

static khint32_t bgpstream_ipv4_address_hash_func(bgpstream_ip_address_t ip)
{
  assert(ip.type == BST_IPV4); // check type is ipv4
  khint32_t h = ip.address.v4_addr.s_addr;  
  return __ac_Wang_hash(h);  // decreases the chances of collisions
}

static khint64_t bgpstream_ipv6_address_hash_func(bgpstream_ip_address_t ip) 
{
  assert(ip.type == BST_IPV6); // check type is ipv6
  khint64_t h = *((khint64_t *) &(ip.address.v6_addr.s6_addr[0]));
  return __ac_Wang_hash(h);  // decreases the chances of collisions
}

int bgpstream_ipv4_address_hash_equal(bgpstream_ip_address_t ip1,
				      bgpstream_ip_address_t ip2)
{
  assert(ip1.type == BST_IPV4); // check type is ipv4
  assert(ip2.type == BST_IPV4); // check type is ipv4
  // we cannot use memcmp as these are unions and ipv4 is not the
  // largest data structure that can fit in the union, ipv6 is
  return (ip1.address.v4_addr.s_addr == ip2.address.v4_addr.s_addr);
}

int bgpstream_ipv6_address_hash_equal(bgpstream_ip_address_t ip1,
				      bgpstream_ip_address_t ip2) 
{
  assert(ip1.type == BST_IPV6); // check type is ipv6
  assert(ip2.type == BST_IPV6); // check type is ipv6
  return (memcmp(&ip1,&ip2, sizeof(bgpstream_ip_address_t)) == 0);
}

KHASH_INIT(ipv4_peers_table_t /* name */,
	   bgpstream_ip_address_t /* khkey_t */,
	   peerdata_t * /* khval_t */,
	   1 /* kh_is_map */,
	   bgpstream_ipv4_address_hash_func /*__hash_func */,
	   bgpstream_ipv4_address_hash_equal /* __hash_equal */);

KHASH_INIT(ipv6_peers_table_t /* name */,
	   bgpstream_ip_address_t /* khkey_t */,
	   peerdata_t * /* khval_t */,
	   1 /* kh_is_map */,
	   bgpstream_ipv6_address_hash_func /*__hash_func */,
	   bgpstream_ipv6_address_hash_equal /* __hash_equal */);

typedef struct struct_peers_table_t {
  khash_t(ipv4_peers_table_t) * ipv4_peers_table;
  khash_t(ipv6_peers_table_t) * ipv6_peers_table;
} peers_table_t;


/* Peers related functions */

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

static void peers_table_update(bgpstream_elem_t * elem,
			       peers_table_t *peers_table) 
{
  assert(peers_table != NULL);
  assert(elem != NULL);
  khiter_t k;
  int khret;
  peerdata_t * peer_data = NULL;

  // ipv4 peer
  if(elem->peer_address.type == BST_IPV4) {
    /* check if this peer is in the hash already */
    if((k = kh_get(ipv4_peers_table_t, peers_table->ipv4_peers_table,
		   elem->peer_address)) ==
       kh_end(peers_table->ipv4_peers_table))
      {
	/* create a new data structure */
	peer_data = peerdata_create();
	/* add it to the hash */
	k = kh_put(ipv4_peers_table_t, peers_table->ipv4_peers_table, 
		   elem->peer_address, &khret);
	kh_value(peers_table->ipv4_peers_table, k) = peer_data;
      }
    else
      {
	/* already exists, just get it */
	peer_data = kh_value(peers_table->ipv4_peers_table, k);
      }
  }
  // ipv6 peer
  if(elem->peer_address.type == BST_IPV6) {
    /* check if this peer is in the hash already */
    if((k = kh_get(ipv6_peers_table_t, peers_table->ipv6_peers_table,
		   elem->peer_address)) ==
       kh_end(peers_table->ipv6_peers_table))
      {
	/* create a new data structure */
	peer_data = peerdata_create();
	/* add it to the hash */
	k = kh_put(ipv6_peers_table_t, peers_table->ipv6_peers_table, 
		   elem->peer_address, &khret);
	kh_value(peers_table->ipv6_peers_table, k) = peer_data;
      }
    else
      {
	/* already exists, just get it */
	peer_data = kh_value(peers_table->ipv6_peers_table, k);
      }    
  }
  
  if(peer_data != NULL) {
    peerdata_update(elem, peer_data);
  }  
}


static void peers_table_new_rib(peers_table_t *peers_table) 
{
  khiter_t k;  
  /* send a new rib signal to all ipv4 peers */
  for (k = kh_begin(peers_table->ipv4_peers_table);
       k != kh_end(peers_table->ipv4_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv4_peers_table, k))
	{
	  /* reset the value */
	  peerdata_new_rib(kh_value(peers_table->ipv4_peers_table, k));
	}
    }   

  /* send a new rib signal to all ipv6 peers */
  for (k = kh_begin(peers_table->ipv6_peers_table);
       k != kh_end(peers_table->ipv6_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv6_peers_table, k))
	{
	  /* reset the value */
	  peerdata_new_rib(kh_value(peers_table->ipv6_peers_table, k));
	}
    }  
}


static void peers_table_end_of_interval(peers_table_t *peers_table) 
{
  /** we do not remove peers from peers_table as they are
   *  a stable entity, we just zeroes their peerdata 
   *  such process is performed by peerdata_end_of_interval
   *  everytime we dump with peerdata_dump*/
}


static void peers_table_destroy(peers_table_t *peers_table) 
{
  if(peers_table == NULL) 
    {
      return;
    }
  khiter_t k;
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


/* Collector related functions */

typedef struct collectordata {
  char *dump_project;
  char *dump_collector; /* graphite-safe version of the name */
  uint64_t num_records[BGPSTREAM_RECORD_TYPE_MAX];
  uint64_t num_elem[BGPSTREAM_ELEM_TYPE_MAX];
  // table containing information about each peer of the collector
  peers_table_t * peers_table;
  // set of unique prefixes seen by all the peers
  prefixes_table_t * prefixes_table;   
  /* set of unique prefixes that have been affected by a change
   * during the interval */
  prefixes_table_t * affected_prefixes_table;
  /* set of origin ases in routing table */
  ases_table_wrapper_t * ases_table; 
  // origin ases active in the current interval (i.e. announcing)
  ases_table_wrapper_t * announcing_ases_table;
}collectordata_t;


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

  if((collector_data->prefixes_table = prefixes_table_create()) == NULL)
    {
      peers_table_destroy(collector_data->peers_table);
      free(collector_data->dump_collector);
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }

  if((collector_data->affected_prefixes_table = prefixes_table_create()) == NULL)
    {
      prefixes_table_destroy(collector_data->prefixes_table);
      peers_table_destroy(collector_data->peers_table);
      free(collector_data->dump_collector);
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }


  if((collector_data->ases_table = ases_table_create()) == NULL)
    {
      prefixes_table_destroy(collector_data->affected_prefixes_table);
      prefixes_table_destroy(collector_data->prefixes_table);
      peers_table_destroy(collector_data->peers_table);
      free(collector_data->dump_collector);
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }

  if((collector_data->announcing_ases_table = ases_table_create()) == NULL)
    {
      ases_table_destroy(collector_data->ases_table);
      prefixes_table_destroy(collector_data->affected_prefixes_table);
      prefixes_table_destroy(collector_data->prefixes_table);
      peers_table_destroy(collector_data->peers_table);
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



static void collectordata_update(bgpcorsaro_record_t * record,
				 collectordata_t *collector_data)
{
  assert(collector_data != NULL);
  assert(collector_data->peers_table != NULL);
  bgpstream_record_t * bs_record = BS_REC(record);
  bgpstream_elem_t * bs_elem_queue;
  bgpstream_elem_t * bs_iterator;

  collector_data->num_records[bs_record->status]++;
  if(bs_record->status == VALID_RECORD)
    {
      /* if it is the first record of a RIB
       * send a message to all peers, so they
       * can reset their prefixes tables */
      if(bs_record->attributes.dump_type == BGPSTREAM_RIB &&
	 bs_record->dump_pos == DUMP_START) {
	peers_table_new_rib(collector_data->peers_table);
      }
      bs_elem_queue = bgpstream_get_elem_queue(bs_record);
      bs_iterator = bs_elem_queue;
      while(bs_iterator != NULL)
	{
	  collector_data->num_elem[bs_iterator->type]++;
	  /* update peer information */
	  peers_table_update(bs_iterator, collector_data->peers_table);
	  // other information are computed at dump time
	  bs_iterator = bs_iterator->next;
	}
      bgpstream_destroy_elem_queue(bs_elem_queue);

    }
  return;
}


// ----------------- other dump functions here -------------------------- //


static void peerdata_dump(char *dump_project, char *dump_collector, char *peer_address,
			  int int_start_time, peerdata_t *peer_data, 
			  collectordata_t *collector_data)
{
  graphite_safe(peer_address);
  /* rib_entries */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.rib_entry_cnt %"PRIu64" %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->num_elem[BST_RIB],
	  int_start_time);

  /* announcements */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.announcement_entry_cnt %"PRIu64" %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->num_elem[BST_ANNOUNCEMENT],
	  int_start_time);
  
  /* withdrawals */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.withdrawal_entry_cnt %"PRIu64" %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->num_elem[BST_WITHDRAWAL],
	  int_start_time);

  /* withdrawals */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.state_messages_cnt %"PRIu64" %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->num_elem[BST_STATE],
	  int_start_time);

  /* number of ipv4/ipv6 prefixes */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.ipv4_rib_size %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->ribs_table->ipv4_rib),
	  int_start_time);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.ipv6_rib_size %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->ribs_table->ipv6_rib),
	  int_start_time);

  /* number of ipv4/ipv6 prefixes affected by a change during the current interval */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.ipv4_affected_prefixes %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->affected_prefixes_table->ipv4_prefixes_table),
	  int_start_time);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.ipv6_affected_prefixes %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->affected_prefixes_table->ipv6_prefixes_table),
	  int_start_time);

  /* number of unique ASes (no sets/confeds) announcing at least
   * one new prefix in the current interval */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.announcing_ases %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->announcing_ases_table->table),
	  int_start_time);

  /* number non_std_origin_as occurrencies (updates only) */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.non_std_origin_as_cnt %d %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->non_std_origin_as_cnt,
	  int_start_time);

  /* number empty_origin_as_cnt occurrencies (updates only) */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.empty_origin_as_cnt %d %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->empty_origin_as_cnt,
	  int_start_time);  


  /* populate collector_data structures with peer information */ 
  ribs_table_parser(collector_data->prefixes_table, peer_data->ases_table,
		    collector_data->ases_table, peer_data->ribs_table);

  prefixes_table_union(collector_data->affected_prefixes_table, peer_data->affected_prefixes_table);
  ases_table_union(collector_data->announcing_ases_table, peer_data->announcing_ases_table);

  /* number of unique ASes in the ribs */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.origin_ases_in_ribs_cnt %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->ases_table->table),
	  int_start_time);

}


static void peers_table_dump(char *dump_project, char *dump_collector,
			     int int_start_time, peers_table_t *peers_table,
			     collectordata_t *collector_data) 
{
  assert(ch_prefix != NULL);
  assert(peers_table != NULL);

  khiter_t k;
  int khret;
  peerdata_t * peer_data = NULL;
  bgpstream_ip_address_t ip;
  char ip4_str[INET_ADDRSTRLEN];
  char ip6_str[INET6_ADDRSTRLEN];

  // ipv4 peers informations
  for(k = kh_begin(peers_table->ipv4_peers_table);
      k != kh_end(peers_table->ipv4_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv4_peers_table, k))
	{
	  ip = kh_key(peers_table->ipv4_peers_table, k);
	  inet_ntop(AF_INET, &(ip.address.v4_addr), ip4_str, INET_ADDRSTRLEN);
	  peer_data = kh_value(peers_table->ipv4_peers_table, k);
	  peerdata_dump(dump_project, dump_collector, ip4_str, int_start_time, peer_data, collector_data);
	  peerdata_end_of_interval(kh_value(peers_table->ipv4_peers_table, k));
	}      
    }

  // ipv6 peers informations
  for(k = kh_begin(peers_table->ipv6_peers_table);
      k != kh_end(peers_table->ipv6_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv6_peers_table, k))
	{
	  ip = kh_key(peers_table->ipv6_peers_table, k);	  
	  inet_ntop(AF_INET6, &(ip.address.v6_addr), ip6_str, INET6_ADDRSTRLEN);
	  peer_data = kh_value(peers_table->ipv6_peers_table, k);
	  peerdata_dump(dump_project, dump_collector, ip6_str, int_start_time, peer_data, collector_data);	
	  peerdata_end_of_interval(kh_value(peers_table->ipv6_peers_table, k));
	}
    }
}


// ----------------- other dump functions here -------------------------- //





static void collectordata_dump(collectordata_t *collector_data, int int_start_time)
{
  assert(collector_data);
  /* num_valid_records */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.valid_record_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_records[VALID_RECORD],
	  int_start_time);

  /* rib_entries */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.rib_entry_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_elem[BST_RIB],
	  int_start_time);

  /* announcements */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.announcement_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_elem[BST_ANNOUNCEMENT],
	  int_start_time);

  /* withdrawals */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.withdrawal_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_elem[BST_WITHDRAWAL],
	  int_start_time);

  /* state messages */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.state_messages_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_elem[BST_STATE],
	  int_start_time);

  /* peer-related statistics */ 

  /* ipv4 / ipv6 peers */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv4_peers_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->peers_table->ipv4_peers_table),
	  int_start_time);	  

  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv6_peers_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->peers_table->ipv6_peers_table),
	  int_start_time);


  peers_table_dump(collector_data->dump_project, collector_data->dump_collector,
		   int_start_time, collector_data->peers_table, collector_data);

  peers_table_end_of_interval(collector_data->peers_table);


  /* number of ipv4/ipv6 prefixes per collector */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv4_rib_size %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->prefixes_table->ipv4_prefixes_table),
	  int_start_time);	  

  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv6_rib_size %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->prefixes_table->ipv6_prefixes_table),
	  int_start_time);	  

  /* number of ipv4/ipv6 prefixes affected by a change during the current interval */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv4_affected_prefixes %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->affected_prefixes_table->ipv4_prefixes_table),
	  int_start_time);	  

  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv6_affected_prefixes %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->affected_prefixes_table->ipv6_prefixes_table),
	  int_start_time);

  /* number of unique origin ases in ribs per collector */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.origin_ases_in_ribs_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->ases_table->table),
	  int_start_time);
  
  /* number of announcing ases per collector */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.announcing_ases %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->announcing_ases_table->table),
	  int_start_time);
  
}


static void collectordata_end_of_interval(collectordata_t *collector_data)
{
  memset(collector_data->num_records, 0,
	 sizeof(collector_data->num_records));

  memset(collector_data->num_elem, 0,
	 sizeof(collector_data->num_elem));

  // resetting all the collector information that need
  // to be recomputed at the end of every interval
  prefixes_table_reset(collector_data->prefixes_table);
  prefixes_table_reset(collector_data->affected_prefixes_table);
  ases_table_reset(collector_data->ases_table);
  ases_table_reset(collector_data->announcing_ases_table);

  /* every "piece" take care of his own end of interval 
   * just right after the dump function 
   * this way we reduce the number of loops to reset data
   */
}


static void collectordata_destroy(collectordata_t *collector_data)
{
  if(collector_data == NULL)
    {
      return;
    }

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
  if(collector_data->prefixes_table != NULL)
    {
      prefixes_table_destroy(collector_data->prefixes_table);
      collector_data->prefixes_table = NULL;
    }
  if(collector_data->affected_prefixes_table != NULL)
    {
      prefixes_table_destroy(collector_data->affected_prefixes_table);
      collector_data->affected_prefixes_table = NULL;
    }
  if(collector_data->ases_table != NULL)
    {
      ases_table_destroy(collector_data->ases_table);
      collector_data->ases_table = NULL;
    }

  free(collector_data);
}

/** Collectors table khash related functions */

KHASH_INIT(collector_table_t,        /* name */
	   char *,                   /* khkey_t */
	   collectordata_t *,        /* khval_t */
	   1,                        /* kh_is_map */
	   kh_str_hash_func,         /* __hash_func */
	   kh_str_hash_equal         /* __hash_equal */
	   )

/** Holds the state for an instance of this plugin */
struct bgpcorsaro_bgpstats_state_t {
  /** The outfile for the plugin */
  iow_t *outfile;
  /** A set of pointers to outfiles to support non-blocking close */
  iow_t *outfile_p[OUTFILE_POINTERS];
  /** The current outfile */
  int outfile_n;

  /** interval start time */
  int interval_start;
  /** number of records read for each type */
  int num_records[BGPSTREAM_RECORD_TYPE_MAX];
  /** Hash of collector string to collector data */
  khash_t(collector_table_t) *collectors_table; 
};

/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, bgpstats, BGPCORSARO_PLUGIN_ID_BGPSTATS))
/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_BGPSTATS))

#if 0
/** Print usage information to stderr */
static void usage(bgpcorsaro_plugin_t *plugin)
{
  //TODO modify usage
  fprintf(stderr,
	  "plugin usage: %s [-HmM] [-t mode]\n"
	  "       -H         multi-line, human-readable (default)\n"
	  "       -m         one-line per entry with unix timestamps\n"
	  "       -M         one-line per entry with human readable timestamps (and some other differences that no human could ever comprehend)\n"
	  "       -t dump    timestamps for RIB dumps reflect the time of the dump (default)\n"
	  "       -t change  timestamps for RIB dumps reflect the last route modification\n",
	  plugin->argv[0]);
}

/** Parse the arguments given to the plugin */
static int parse_args(bgpcorsaro_t *bgpcorsaro)
{
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);
  int opt;
  //TODO modify this: right now it doesn't do anything

  if(plugin->argc <= 0)
    {
      return 0;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, ":t:HmM?")) >= 0)
    {
      switch(opt)
	{
	case 'H':
	  break;

	case 'm':
	  break;

	case 'M':
	  break;

	case 't':
	  break;

	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  /* dump doesn't take any arguments */
  if(optind != plugin->argc)
    {
      usage(plugin);
      return -1;
    }

  return 0;
}
#endif


/** saves the beginning of the interval in the state */
static void stats_init(struct bgpcorsaro_bgpstats_state_t *state,
		       bgpcorsaro_interval_t *int_start) {
  assert(state != NULL);
  state->interval_start = int_start->time;
}


/** struct bgpdatainfo_t update (inside the interval) */
static int stats_update(struct bgpcorsaro_bgpstats_state_t *state,
			bgpcorsaro_record_t * record)
{
  assert(state != NULL);
  assert(record != NULL);

  khiter_t k;
  collectordata_t * collector_data;
  char *name_cpy = NULL;
  int khret;
  bgpstream_record_t * bs_record = BS_REC(record);
  assert(bs_record != NULL);

  state->num_records[BS_REC(record)->status]++;

  /* check if this collector is in the hash already */
  if((k = kh_get(collector_table_t, state->collectors_table,
		 bs_record->attributes.dump_collector)) ==
     kh_end(state->collectors_table))
    {
      /* create a new data structure */
      collector_data =
	collectordata_create(bs_record->attributes.dump_project,
			     bs_record->attributes.dump_collector);

      /* copy the name of the collector */
      /* TODO: work out how to not use strings to identify a collector */
      /* TODO: what if two projects use the same collector name? */
      if((name_cpy = strdup(bs_record->attributes.dump_collector)) == NULL)
	{
	  return -1;
	}

      /* add it to the hash */
      k = kh_put(collector_table_t, state->collectors_table,
		 name_cpy, &khret);

      kh_value(state->collectors_table, k) = collector_data;
    }
  else
    {
      /* already exists, just get it */
      collector_data = kh_value(state->collectors_table, k);
    }

  collectordata_update(record, collector_data);
  return 0;
}

/** struct bgpdatainfo_t eoi (end of interval) - write stats at end of
    interval*/
static void stats_dump(struct bgpcorsaro_bgpstats_state_t *state)
{
  assert(state != NULL);
  khiter_t k;
  collectordata_t *collector_data;

  for(k = kh_begin(state->collectors_table);
      k != kh_end(state->collectors_table); ++k)
    {
      if (kh_exist(state->collectors_table, k))
	{
	  collector_data = kh_value(state->collectors_table, k);
	  collectordata_dump(collector_data, state->interval_start);
	  /* might as well reset here to avoid walking the hash a second time */
	  collectordata_end_of_interval(collector_data);
	}
    }

  memset(state->num_records, 0, sizeof(state->num_records));
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
bgpcorsaro_plugin_t *bgpcorsaro_bgpstats_alloc(bgpcorsaro_t *bgpcorsaro)
{
  return &bgpcorsaro_bgpstats_plugin;
}

/** Implements the init_output function of the plugin API */
int bgpcorsaro_bgpstats_init_output(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_bgpstats_state_t *state;
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct bgpcorsaro_bgpstats_state_t))) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not malloc bgpcorsaro_bgpstats_state_t");
      goto err;
    }

  state->collectors_table = kh_init(collector_table_t);

  bgpcorsaro_plugin_register_state(bgpcorsaro->plugin_manager, plugin, state);

#if 0
  /* parse the arguments */
  if(parse_args(bgpcorsaro) != 0)
    {
      return -1;
    }
#endif

  /* defer opening the output file until we start the first interval */

  return 0;

 err:
  bgpcorsaro_bgpstats_close_output(bgpcorsaro);
  return -1;
}

/** Implements the close_output function of the plugin API */
int bgpcorsaro_bgpstats_close_output(bgpcorsaro_t *bgpcorsaro)
{
  int i;
  khiter_t k;
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);

  if(state != NULL)
    {
      /* close all the outfile pointers */
      for(i = 0; i < OUTFILE_POINTERS; i++)
	{
	  if(state->outfile_p[i] != NULL)
	    {
	      wandio_wdestroy(state->outfile_p[i]);
	      state->outfile_p[i] = NULL;
	    }
	}
      state->outfile = NULL;

      /* free all the keys and values in the collectors table */
      for (k = kh_begin(state->collectors_table);
	   k != kh_end(state->collectors_table); ++k)
	{
	  if (kh_exist(state->collectors_table, k))
	    {
	      /* free the value */
	      collectordata_destroy(kh_value(state->collectors_table, k));
	      /* free the key (string) */
	      free(kh_key(state->collectors_table, k));
	    }
	}

      /* destroy collectors table */
      kh_destroy(collector_table_t, state->collectors_table);

      bgpcorsaro_plugin_free_state(bgpcorsaro->plugin_manager,
				   PLUGIN(bgpcorsaro));
    }
  return 0;
}

/** Implements the start_interval function of the plugin API */
int bgpcorsaro_bgpstats_start_interval(bgpcorsaro_t *bgpcorsaro,
				       bgpcorsaro_interval_t *int_start)
{
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);
  if(state->outfile == NULL)
    {
      if((
	  state->outfile_p[state->outfile_n] =
	  bgpcorsaro_io_prepare_file(bgpcorsaro,
				     PLUGIN(bgpcorsaro)->name,
				     int_start)) == NULL)
	{
	  bgpcorsaro_log(__func__, bgpcorsaro, "could not open %s output file",
			 PLUGIN(bgpcorsaro)->name);
	  return -1;
	}
      state->outfile = state->
	outfile_p[state->outfile_n];
    }

  stats_init(state, int_start);

  bgpcorsaro_io_write_interval_start(bgpcorsaro, state->outfile, int_start);

  return 0;
}

/** Implements the end_interval function of the plugin API */
int bgpcorsaro_bgpstats_end_interval(bgpcorsaro_t *bgpcorsaro,
				     bgpcorsaro_interval_t *int_end)
{
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);

  bgpcorsaro_log(__func__, bgpcorsaro, "Dumping stats for interval %d",
		 int_end->number);
  /* dump statistics */
  stats_dump(state);

  bgpcorsaro_io_write_interval_end(bgpcorsaro, state->outfile, int_end);

  /* if we are rotating, now is when we should do it */
  if(bgpcorsaro_is_rotate_interval(bgpcorsaro))
    {
      /* leave the current file to finish draining buffers */
      assert(state->outfile != NULL);

      /* move on to the next output pointer */
      state->outfile_n = (state->outfile_n+1) %
	OUTFILE_POINTERS;

      if(state->outfile_p[state->outfile_n] != NULL)
	{
	  /* we're gonna have to wait for this to close */
	  wandio_wdestroy(state->outfile_p[state->outfile_n]);
	  state->outfile_p[state->outfile_n] =  NULL;
	}

      state->outfile = NULL;
    }
  return 0;
}

/** Implements the process_record function of the plugin API */
int bgpcorsaro_bgpstats_process_record(bgpcorsaro_t *bgpcorsaro,
				       bgpcorsaro_record_t *record)
{
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);

  /* no point carrying on if a previous plugin has already decided we should
     ignore this record */
  if((record->state.flags & BGPCORSARO_RECORD_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }

  assert(state != NULL);
  return stats_update(state, record);
}

