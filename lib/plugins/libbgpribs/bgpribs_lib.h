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

#ifndef __BGPRIBS_LIB_H
#define __BGPRIBS_LIB_H

#include "khash.h"
#include "bgpstream_lib.h"
#include "bgpribs_khash.h"


/** prefixdata
 *  this structure contains information about a given prefix
 *  - origin_as (if any) is the AS announcing that prefix
 *  
 *  TODO: possible other metrics - ts, as_path, IBGP, EBGP
 */

typedef struct struct_prefixdata_t {
  uint32_t origin_as;  
} prefixdata_t;


/** ribs table
 *  this structure contains two separate ribs (one for ipv4 prefixes
 *  and one for ipv6 prefixes), each entry of the rib associate a:
 *  bgpstream_prefix_t -- (to) --> a prefixdata_t
 */

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


ribs_table_t *ribs_table_create();
void ribs_table_destroy(ribs_table_t *ribs_table);


/** ases table
 *  this structure maintains a set of unique
 *  AS numbers (16/32 bits AS numbers are hashed
 *  using a uint32 type)
 */

KHASH_INIT(ases_table_t /* name */, 
	   uint32_t /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   kh_int_hash_func /*__hash_func */,  
	   kh_int_hash_equal /* __hash_equal */);

typedef struct struct_ases_table_wrapper_t {
  khash_t(ases_table_t) * table;
} ases_table_wrapper_t;

ases_table_wrapper_t *ases_table_create();
void ases_table_destroy(ases_table_wrapper_t *ases_table);


/** prefixes table
 *  this structure contains two separate sets: 
 *  the set  of unique ipv4 prefixes and the set
 *  of unique ipv6 prefixes
 */

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

prefixes_table_t *prefixes_table_create();
void prefixes_table_destroy(prefixes_table_t *prefixes_table);


/** peerdata
 *  this structure contains information about a single
 *  peer
 */

typedef struct struct_peerdata_t {
  // ribs table
  ribs_table_t * ribs_table;
  // TODO: add status variables and metrics here
} peerdata_t;

peerdata_t *peerdata_create();
void peerdata_destroy(peerdata_t *peer_data);


/** peers table
 *  this structure contains two maps, one for ipv4 peers
 *  and one for ipv6 peers. Each one associate the IP
 *  address of the peer to a peerdata structure
 */

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

peers_table_t *peers_table_create();
void peers_table_destroy(peers_table_t *peers_table);


/** collectordata
 *  this structure contains information about a single
 *  collector
 */

typedef struct collectordata {
  char *dump_project;
  char *dump_collector; /* graphite-safe version of the name */
  // table containing information about each peer of the collector
  peers_table_t * peers_table;
} collectordata_t;

collectordata_t *collectordata_create(const char *project,
				      const char *collector);
void collectordata_destroy(collectordata_t *collector_data);


/** collectors table
 *  this structure contains a map that associate to
 *  each collector (string) a collectordata structure
 */

KHASH_INIT(collectors_table_t,        /* name */
	   char *,                   /* khkey_t */
	   collectordata_t *,        /* khval_t */
	   1,                        /* kh_is_map */
	   kh_str_hash_func,         /* __hash_func */
	   kh_str_hash_equal         /* __hash_equal */
	   )

typedef struct struct_collectors_table_wrapper_t {
  khash_t(collectors_table_t) * table;
} collectors_table_wrapper_t;

collectors_table_wrapper_t *collectors_table_create();
void collectors_table_destroy(collectors_table_wrapper_t *collectors_table);



#endif /* __BGPRIBS_LIB_H */
