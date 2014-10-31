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

#ifndef __BGPSTORE_BGPVIEW_H
#define __BGPSTORE_BGPVIEW_H

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include <khash.h>
#include <assert.h>

#include "bgpwatcher_common.h"

#include "bl_bgp_utils.h"
#include "bl_str_set.h"

#include "khash.h"


/************ prefix info ************/

typedef struct struct_pfxinfo_t {
  uint32_t orig_asn;
  // TODO: add new fields
} pfxinfo_t;


/************ per peer prefix view ************/

KHASH_INIT(bsid_pfxview, uint16_t, pfxinfo_t, 1,
	   kh_int_hash_func, kh_int_hash_equal);

typedef struct struct_peerview_t {
  khash_t(bsid_pfxview) *peer_pfxview;
} peerview_t;


/************ aggregated prefix views ************/

KHASH_INIT(aggr_pfxview_ipv4, bl_ipv4_pfx_t, peerview_t*, 1,
	   bl_ipv4_pfx_hash_func, bl_ipv4_pfx_hash_equal);

KHASH_INIT(aggr_pfxview_ipv6, bl_ipv6_pfx_t, peerview_t*, 1,
	   bl_ipv6_pfx_hash_func, bl_ipv6_pfx_hash_equal);


/************ client #peer_tables_rcvd map ************/

// KHASH_INIT(client_rcv_pt_cnt, char*, uint8_t , 1,
// 	   kh_int_hash_func, kh_int_hash_equal);


/************ set of unique numeric ids ************/

KHASH_INIT(id_set, uint16_t, char , 0,
	   kh_int_hash_func, kh_int_hash_equal);


/************ status of a single collector ************/

typedef struct struct_coll_status_t {
  // number of peer tables expected
  uint8_t expected_peer_tables_cnt;
  // number of peer tables already received/processed
  uint8_t received_peer_tables_cnt;
  // set of ids associated with peers up
  khash_t(id_set) *active_peer_ids_list;
  // set of ids associated with peers not up
  khash_t(id_set) *inactive_peer_ids_list;
} coll_status_t;


/************ collector -> status ************/

KHASH_INIT(collectorstr_status, char*, coll_status_t *, 1,
	   kh_int_hash_func, kh_int_hash_equal);

/*** status of a single (collector,peer) aka id ***/

typedef struct struct_id_status_t {
  // number of expected prefix tables expected from this peer
  uint32_t expected_pfx_tables_cnt;
  // number of expected prefix tables received from this peer
  uint32_t received_pfx_tables_cnt;
  // number of ipv4 prefixes received
  uint32_t recived_ipv4_pfx_cnt;
  // number of ipv6 prefixes received
  uint32_t recived_ipv6_pfx_cnt; 
} id_status_t;


/************ id -> status ************/
KHASH_INIT(id_status, char*, id_status_t , 1,
	   kh_int_hash_func, kh_int_hash_equal);


/************ bgpview ************/

typedef struct struct_bgpview_t {

  /** a table that aggregates all the information
   *  collected for any received prefix */
  khash_t(aggr_pfxview_ipv4) *aggregated_pfxview_ipv4;
  khash_t(aggr_pfxview_ipv6) *aggregated_pfxview_ipv6;

  /** list of clients that have sent at least one
   *   complete table  */
  kh_bl_string_set_t *done_clients;

  /** for each client (string) we store the number of 
   *  peer tables received (uint8) */
  // khash_t(client_rcv_pt_cnt) *client_peertable_rcvd;

  /** for each collector (string) we store the number of 
   *  peer tables received (uint8) and the number of peer
   *  tables received (uint8), also we register the status
   *  of each single peer */
  khash_t(collectorstr_status) *collector_status;
  
  /** for each active id we store its status */
  khash_t(id_status) *peer_status;

} bgpview_t;


/** Allocate memory for a strucure that maintains
 *  the bgp information collected for a single timestamp
 *  (ts = table_time received in peer and pfx records).
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bgpview_t *bgpview_create();



// TODO: add documentation

int bgpview_add_peer(bgpview_t *bgp_view, char *collector, bgpwatcher_peer_t* peer_info);


// TODO: add documentation

int bgpview_add_row(bgpview_t *bgp_view, bgpwatcher_pfx_table_t *table,
		    bgpwatcher_pfx_row_t *row);


// TODO: add documentation

int bgpview_table_end(bgpview_t *bgp_view, char *client_name,
		      bgpwatcher_pfx_table_t *table);


/** Deallocate memory for the bgpview structure
 *
 * @param bgp_view a pointer to the bgpview memory
 */
void bgpview_destroy(bgpview_t *bgp_view);


#endif /* __BGPSTORE_BGPVIEW_H */
