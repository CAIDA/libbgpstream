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

#ifndef __BGPRIBS_INT_H
#define __BGPRIBS_INT_H


#include "khash.h"
#include "bgpribs_lib.h"
#include "bgpstream_lib.h"
#include "bgpribs_khash.h"
#include <bgpwatcher_client.h>


/** ases table (set of unique ases)
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

/* ases_table_wrapper_t *ases_table_create(); */
/* void ases_table_insert(ases_table_wrapper_t *ases_table, uint32_t as); */
/* void ases_table_reset(ases_table_wrapper_t *ases_table); */
/* void ases_table_destroy(ases_table_wrapper_t *ases_table); */


/** prefixes table (set of unique prefixes)
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

/* prefixes_table_t *prefixes_table_create(); */
/* void prefixes_table_insert(prefixes_table_t *prefixes_table, bgpstream_prefix_t prefix);  */
/* void prefixes_table_reset(prefixes_table_t *prefixes_table); */
/* void prefixes_table_destroy(prefixes_table_t *prefixes_table); */


/** prefixdata
 *  this structure contains information about a given prefix
 *  - origin_as (if any) is the AS announcing that prefix
 *  
 *  TODO: possible other metrics - ts, as_path, IBGP, EBGP
 */

typedef struct struct_prefixdata_t {
  uint32_t origin_as;
  bgpstream_aspath_t aspath;
  uint8_t is_active;  // is this prefix up or down
  long int ts;        // when this entry modified the rib
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
  // reference rib = last rib applied to this ribs_table
  long int reference_rib_start; // when the reference rib starts
  long int reference_rib_end;   // when the reference rib ends
  long int reference_dump_time; // dump_time associated with the reference rib
} ribs_table_t;

/* ribs_table_t *ribs_table_create(); */
/* void ribs_table_apply_elem(ribs_table_t *ribs_table, bgpstream_elem_t *bs_elem); */
/* void ribs_table_reset(ribs_table_t *ribs_table); */
/* void ribs_table_destroy(ribs_table_t *ribs_table); */

/** peerdata
 *  this structure contains information about a single
 *  peer
 */

typedef enum  {
  PEER_NULL = 0,   // status of peer is unknown
  PEER_DOWN = 1,   // status of peer is DOWN
  PEER_UP = 2      // status of peer is UP
} peer_status_t;

typedef enum  {
  UC_OFF = 0,     // there is no under construction ribs_table
  UC_ON = 1,      // we are constructing a new ribs_table
} ribs_tables_status_t;

typedef struct struct_peerdata_t {
  char * peer_address_str; /* graphite-safe version of the peer ip address */
  // ribs tables
  ribs_table_t * active_ribs_table; // active ribs table
  ribs_table_t * uc_ribs_table;     // under-construction ribs table
  long int most_recent_ts;
  ribs_tables_status_t rt_status;
  peer_status_t status;
  // information to dump and reset at every interval_end
  uint64_t elem_types[BGPSTREAM_ELEM_TYPE_MAX];  
  ases_table_wrapper_t * unique_origin_ases;
  // information related to announcements/withdrawals in interval
  prefixes_table_t * affected_prefixes; // prefixes affected by updates
  ases_table_wrapper_t * announcing_origin_ases; // origin ases announcing a prefix
} peerdata_t;

/* peerdata_t *peerdata_create(bgpstream_ip_address_t * peer_address); */
/* int peerdata_apply_elem(peerdata_t *peer_data,  */
/* 			bgpstream_record_t * bs_record, bgpstream_elem_t *elem); */
/* int peerdata_apply_record(peerdata_t *peer_data, bgpstream_record_t * bs_record); */
/* void peerdata_interval_end(peerdata_t *peer_(data, int interval_start, */
/* 			   collectordata_t *collector_data); */
/* void peerdata_destroy(peerdata_t *peer_data); */


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

/* peers_table_t *peers_table_create(); */
/* int peers_table_process_record(peers_table_t *peers_table,  */
/* 			          bgpstream_record_t * bs_record);  */
/* void  peers_table_interval_end(peers_table_t *peers_table, int interval_start, */
/* 			          collectordata_t *collector_data); */
/* void peers_table_destroy(peers_table_t *peers_table); */


/** collectordata
 *  this structure contains information about a single
 *  collector
 */

typedef enum  {
  COLLECTOR_NULL = 0,    // status of collector is unknown
  COLLECTOR_DOWN = 1,    // status of collector is DOWN
  COLLECTOR_UP =   2     // status of collector is UP
  } collector_status_t;

typedef struct collectordata {
  char * dump_project;        /* graphite-safe version of the name */
  char * dump_collector;      /* graphite-safe version of the name */
  long int most_recent_ts;    /* most recent timestamp received */
  int active_peers;
  collector_status_t status; /* it tells whether the collector is up or down */
  // table containing information about each peer of the collector
  peers_table_t * peers_table;
  // information to dump and reset at every interval_end
  uint64_t record_types[BGPSTREAM_RECORD_TYPE_MAX];
  prefixes_table_t * unique_prefixes;
  ases_table_wrapper_t * unique_origin_ases;
  prefixes_table_t * affected_prefixes;          // prefixes affected by updates
  ases_table_wrapper_t * announcing_origin_ases; // origin ases announcing a prefix
} collectordata_t;

/* collectordata_t *collectordata_create(const char *project, */
/* 				      const char *collector); */
/* int collectordata_process_record(collectordata_t *collector_data, */
/* 				 bgpstream_record_t * bs_record); */
/* void collectordata_interval_end(collectordata_t *collector_data,  */
/* 				int interval_start); */
/* void collectordata_destroy(collectordata_t *collector_data); */


typedef struct bw_client {
  bgpwatcher_client_t *client;
  bgpwatcher_client_pfx_table_t *pfx_table;
  bgpwatcher_pfx_record_t *pfx_record;
  bgpwatcher_client_peer_table_t *peer_table;
  bgpwatcher_peer_record_t *peer_record;
} bw_client_t;

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

struct collectors_table_wrapper {
  khash_t(collectors_table_t) * table;
  bw_client_t *bw_client;
};


#endif /* __BGPRIBS_INT_H */
