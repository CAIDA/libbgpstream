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

#ifndef __BGPSTORE_INT_H
#define __BGPSTORE_INT_H

#include "bgpstore_lib.h"
#include "bgpstore_bgpview.h"
#include "bgpwatcher_common.h"
#include "bgpstream_elem.h"

#include "bl_bgp_utils.h"
#include "khash.h"



KHASH_INIT(timebgpview, uint32_t, bgpview_t*, 1,
	   kh_int_hash_func, kh_int_hash_equal);

/** The client status is a structure that maintains
 *  the interests of each client, i.e.: which data
 *  is the client interested as a consumer, and
 *  which data is the client interested as a 
 *  producer.
 *  Every bit in the  array indicates whether
 *  an type of information is interesting or not.
 */
typedef struct struct_clientstatus_t {
  uint32_t producer_intents;
  uint32_t consumer_interests;
} clientstatus_t;

KHASH_INIT(strclientstatus, char*, clientstatus_t , 1,
	   kh_str_hash_func, kh_str_hash_equal);


KHASH_INIT(peerbsid, bl_addr_storage_t, uint16_t, 1,
	   bl_addr_storage_hash_func, bl_addr_storage_hash_equal); 

typedef struct struct_peeridtable_t {
  khash_t(peerbsid) *peer_bsid;
} peeridtable_t;

KHASH_INIT(collectorpeeridtable, char*, peeridtable_t, 1,
	   kh_str_hash_func, kh_str_hash_equal);

typedef struct struct_collector_peer_t {
  char collector_str[BGPWATCHER_COLLECTOR_NAME_LEN];
  bl_addr_storage_t peer_ip_addr;
} collector_peer_t;


KHASH_INIT(bsidtable, uint16_t, collector_peer_t, 1,
	   kh_int_hash_func, kh_int_hash_equal);


struct bgpstore {
  /** aggregated view of bgpdata organized by time:
   *  for each timestamp we build a bgpview */
  khash_t(timebgpview) *bgp_timeseries;
  /** active_clients contains, for each registered/active
   *  client (i.e. those that are currently connected)
   *  its status.*/
  khash_t(strclientstatus) *active_clients;
  /**  id <-> (collector,peer) caches
   *   These structures associate a numeric id (a bpgstore 
   *    id, aka bs_id, represented by a uint16) to each
   *   (collector,peer) pair. This identifier is shared
   *   by all the bgpviews and it is constant over time.
   *   Also, it enables faster lookup in the bgpview
   *   prefix tables. */
  khash_t(collectorpeeridtable) *collectorpeer_bsid;
  khash_t(bsidtable) *bsid_collectorpeer;
  /** next id that will be used to store a new (collector,
   *  peer) pair */
  uint16_t next_bs_id;
};

#endif /* __BGPSTORE_INT_H */



