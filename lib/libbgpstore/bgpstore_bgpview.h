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
#include "bgpstore_common.h"

#include "bl_bgp_utils.h"
#include "bl_str_set.h"
#include "bl_id_set.h"

#include <sys/time.h>

#include "khash.h"


/************ prefix info ************/

typedef struct struct_pfxinfo_t {
  uint32_t orig_asn;
  // TODO: add new fields
} pfxinfo_t;


/************ per peer prefix view ************/

KHASH_INIT(bsid_pfxview, uint16_t, pfxinfo_t, 1,
	   kh_int_hash_func, kh_int_hash_equal)

// TODO: add documentation

typedef khash_t(bsid_pfxview) peerview_t;


/************ aggregated prefix views ************/

KHASH_INIT(aggr_pfxview_ipv4, bl_ipv4_pfx_t, peerview_t *, 1,
	   bl_ipv4_pfx_hash_func, bl_ipv4_pfx_hash_equal)

typedef khash_t(aggr_pfxview_ipv4) aggr_pfxview_ipv4_t;
			      
KHASH_INIT(aggr_pfxview_ipv6, bl_ipv6_pfx_t, peerview_t *, 1,
	   bl_ipv6_pfx_hash_func, bl_ipv6_pfx_hash_equal)

typedef khash_t(aggr_pfxview_ipv6) aggr_pfxview_ipv6_t;
				   
/*** status of an active peer ***/

typedef struct struct_active_peer_status_t {
  // number of expected prefix tables expected from this peer
  uint32_t expected_pfx_tables_cnt;
  // number of expected prefix tables received from this peer
  uint32_t received_pfx_tables_cnt;
  // number of ipv4 prefixes received
  uint32_t recived_ipv4_pfx_cnt;
  // number of ipv6 prefixes received
  uint32_t recived_ipv6_pfx_cnt; 
} active_peer_status_t;


/************ id -> status ************/
KHASH_INIT(peer_status_map, uint16_t, active_peer_status_t, 1,
	   kh_int_hash_func, kh_int_hash_equal)

typedef khash_t(peer_status_map) peer_status_map_t;
				 
typedef enum {BGPVIEW_STATE_UNKNOWN = 0,
	      BGPVIEW_PARTIAL       = 1,
	      BGPVIEW_FULL          = 2
} bgpview_state_t;

				 

/************ bgpview ************/

typedef struct struct_bgpview_t {

  // TODO: documentation
  bgpview_state_t state;

  /** time when the bgpview was created */
  struct timeval bv_created_time;
  
  /** a table that aggregates all the information
   *  collected for any received prefix */
  aggr_pfxview_ipv4_t *aggregated_pfxview_ipv4;
  aggr_pfxview_ipv6_t *aggregated_pfxview_ipv6;

  /** list of clients that have sent at least one
   *   complete table  */
  bl_string_set_t *done_clients;

  /** list of inactive peers (status null or down) */
  bl_id_set_t *inactive_peers;

  /** for each active id we store its status */
  peer_status_map_t *active_peers_info;

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

int bgpview_add_peer(bgpview_t *bgp_view, bgpwatcher_peer_t* peer_info);


// TODO: add documentation

int bgpview_add_row(bgpview_t *bgp_view, bgpwatcher_pfx_table_t *table,
		    bgpwatcher_pfx_row_t *row);


/** Update the control information associated with specific
 *  bgp view and performs a completion check to verify if
 *  additional prefix tables are expected for this timestamp.
 *
 * @return -1 if an error occurred, 0 if the function updated
 *         correctly the bgp_view structure and more pfx tables
 *         are expected
 */
int bgpview_table_end(bgpview_t *bgp_view, char *client_name,
		      bgpwatcher_pfx_table_t *table);


// TODO: add documentation
// 1 if completed, 0 if incomplete, -1 if something wrong
int bgpview_completion_check(bgpview_t *bgp_view, clientinfo_map_t *active_clients);


/** Deallocate memory for the bgpview structure
 *
 * @param bgp_view a pointer to the bgpview memory
 */
void bgpview_destroy(bgpview_t *bgp_view);


#endif /* __BGPSTORE_BGPVIEW_H */
