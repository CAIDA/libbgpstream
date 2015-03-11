/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2015 The Regents of the University of California.
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

#ifndef __ROUTINGTABLES_INT_H
#define __ROUTINGTABLES_INT_H

#include <stdint.h>

#include "bgpwatcher_view.h"

#ifdef WITH_BGPWATCHER
#include "bgpwatcher_client.h"
#endif

#include "khash.h"
#include "utils.h"

/** Maximum number of collectors supported by the
 *  current implementation */
#define ROUTINGTABLES_MAX_COLLECTORS 256

/** Default metric prefix */
#define ROUTINGTABLES_DEFAULT_METRIC_PFX "bgp.routingtables."

/** Maximum string length for the metric prefix */
#define ROUTINGTABLES_METRIC_PFX_LEN 256 

/** Default full feed prefix count threshold for IPv4
 * routing tables */
#define ROUTINGTABLES_DEFAULT_IPV4_FULLFEED_THR 400000 

/** Default full feed prefix count threshold for IPv6
 * routing tables */
#define ROUTINGTABLES_DEFAULT_IPV6_FULLFEED_THR 10000 


/** Information about the current status 
 *  of a pfx-peer info */
typedef struct struct_perpfx_perpeer_info_t {

  /** Last bgp time associated with the most
   *  recent operation involving the current
   *  prefix and the current peer  */
  uint32_t bgp_time_last_ts;   

} perpfx_perpeer_info_t;


/** Information about the current status 
 *  of a peer */
typedef struct struct_perpeer_info_t {
  
  /** Graphite-safe peer string: peer_ASn.peer_IP */
  char peer_str[BGPSTREAM_UTILS_STR_NAME_LEN];

  /** BGP Finite State Machine of the current peer.
   *  If the peer is active, then its state
   *  is assumed BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED,
   *  if the peer becomes inactive because of a state 
   *  change then the bgp_fsm_state reflects the current
   *  fsm state, finally if the peer is inactive and no
   *  fsm state is known, then state is set to
   *  BGPSTREAM_ELEM_PEERSTATE_UNKNOWN
   */
  bgpstream_elem_peerstate_t bgp_fsm_state;

} perpeer_info_t;


/** Information about the current status 
 *  of a view */
typedef struct struct_perview_info_t {
  
  /** first timestamp in the current reference RIB */
  uint32_t bgp_time_ref_rib_start;
  
  /** last timestamp in the current reference RIB */
  uint32_t bgp_time_ref_rib_end;

  /** dump time of the current reference RIB */
  uint32_t bgp_time_ref_rib_dump_time;

} perview_info_t;


/** Information about the current status 
 *  of a collector */
typedef struct struct_collector_t {

  /** Graphite-safe collector string: project.collector */
  char collector_str[BGPSTREAM_UTILS_STR_NAME_LEN];
  
  /** Table of peerid -> peersign */
  bgpstream_peer_sig_map_t *peersigns;

  /** Active bgp view: it represents a consistent
   *  state of the routing tables as seen by each
   *  peer of the current collector */
  bgpwatcher_view_t *active_view;

  /** Inprogress bgp view: it is the temporary view
   *  that is used to build a new state of the
   *  collector's routing tables */
  bgpwatcher_view_t *inprogress_view;

  /** last time this collector was involved
   *  in bgp operations (bgp time) */
  uint32_t bgp_time_last;
  
  /** last time this collector was involved
   *  in bgp operations (wall time) */
  uint32_t wall_time_last;

  /** dump time of the current reference RIB */
  uint32_t bgp_time_ref_rib_dump_time;

  /** Current status of the collector */
  /** @todo add here collector status */

  /** Indicates whether a new bgpview is
   *  under construction or not */
  
  /** @todo add here collector rib in progress
   *  status */

} collector_t;


/** A map that associate an incremental
 *  numerical id with each collector */
KHASH_INIT(str_id_map, char *, uint8_t, 1, kh_str_hash_func, kh_str_hash_equal);
typedef khash_t(str_id_map) collector_id_t;

/** Structure that manages all the routing
 *  tables that can be possibly built using
 *  the bgp stream in input */
struct struct_routingtables_t {

  /** A map that associate an incremental
   *  numerical id with each collector,
   *  the id can be used as an index to
   *  access collector data */
  collector_id_t *collector_id_map;
  
  /** Sparse list of collectors, where the
   *  index is the collector id and the value
   *  is a pointer to a collector structure */
  collector_t *collectors[ROUTINGTABLES_MAX_COLLECTORS];

  /** Metric prefix */
  char metric_prefix[ROUTINGTABLES_METRIC_PFX_LEN];

  /** Full feed prefix count threshold for IPv4
   * routing tables */
  uint32_t ipv4_fullfeed_th;

  /** Full feed prefix count threshold for IPv4
   * routing tables */
  uint32_t ipv6_fullfeed_th;

  /** beginning of the interval (bgp time) */
  uint32_t bgp_time_interval_start;

  /** end of the interval (bgp time) */
  uint32_t bgp_time_interval_end;
  
  /** last time (wall time) we received
   *  an interval_start signal */
  uint32_t wall_time_interval_start;

  /** @todo add other state variables here */
  
#ifdef WITH_BGPWATCHER
  /** Flags that indicates whether the tx
   *  to the watcher is on or off */
  uint8_t watcher_tx_on;
  /** BGP Watcher client instance */
  bgpwatcher_client_t *watcher_client;
  /** Masks that indicates whether we
   *  transmit IPv4 or IPv6 prefixes, or
   *  both of them, and if we send just
   *  full feed tables or partial too */
  uint8_t tables_mask;
#endif
  
};


#endif /* __ROUTINGTABLES_INT_H */





