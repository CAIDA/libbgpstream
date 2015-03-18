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

/** The time granularity that is used to update the
 *  last wall time for a collector */
#define ROUTINGTABLES_COLLECTOR_WALL_UPDATE_FR 10000 

typedef enum {

  /** It is not possible to infer the state of 
   *  the collector (e.g. initialization time,
   *  or corrupted data) */
  ROUTINGTABLES_COLLECTOR_STATE_UNKNOWN   = 0b000,

  /** The collector is active */
  ROUTINGTABLES_COLLECTOR_STATE_UP        = 0b001,

  /** The collector is inactive */
  ROUTINGTABLES_COLLECTOR_STATE_DOWN      = 0b010,

} collector_state_t;


/** Information about the current status 
 *  of a pfx-peer info */
typedef struct struct_perpfx_perpeer_info_t {

  /** Last bgp time associated with the most
   *  recent operation involving the current
   *  prefix and the current peer  */
  uint32_t bgp_time_last_ts;   

  /** Difference between the current under
   *  construction RIB start time for the 
   *  current peer and the last RIB message
   *  received for the prefix  */
  uint16_t bgp_time_uc_delta_ts;

  /** Origin AS number observed in the current
   *  under construction RIB. If the origin AS
   *  number is 0 then it means the current 
   *  prefix is not observed in the under 
   *  construction RIB */
  uint32_t uc_origin_asn;  

} __attribute__((packed)) perpfx_perpeer_info_t;


/** Indices of the peer metrics for a KP */
typedef struct peer_metric_idx {

  /* routing tables metrics */  

  /* end of interval metrics */
  uint32_t status_idx;
  
  uint32_t active_v4_pfxs_idx;
  uint32_t inactive_v4_pfxs_idx;
  uint32_t active_v6_pfxs_idx;
  uint32_t inactive_v6_pfxs_idx;
  uint32_t unique_origin_as_idx;
  uint32_t avg_aspath_len_idx;

  /* per interval metrics */
  uint32_t affected_v4_pfxs_idx;
  uint32_t affected_v6_pfxs_idx;
  uint32_t announced_origin_as_idx;
  
  uint32_t rib_messages_cnt_idx;
  uint32_t pfx_announcements_cnt_idx;
  uint32_t pfx_withdrawal_cnt_idx;
  uint32_t state_messages_cnt_idx;
  
} __attribute__((packed)) peer_metric_idx_t;


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

  /** first timestamp in the current reference RIB,
   *  or the time we set the current status
   *  (e.g. time of a peer established state) */
  uint32_t bgp_time_ref_rib_start;
  
  /** last timestamp in the current reference RIB,
   *  or the time we set the current status
   *  (e.g. time of a peer established state) */
  uint32_t bgp_time_ref_rib_end;

  /** first timestamp in the current under construction RIB,
   *  0 when the under construction process is off */
  uint32_t bgp_time_uc_rib_start;
  
  /** last timestamp in the current under construction RIB,
   *  0 when the under construction process is off */
  uint32_t bgp_time_uc_rib_end;

  /** Timeseries Key Package for the peer */
  timeseries_kp_t *kp;

  /** Indices of the peer metrics in the peer Key Package */
  peer_metric_idx_t kp_idxs;

} perpeer_info_t;


/** Indices of the collector metrics for a KP */
typedef struct collector_metric_idx {
  
  /* performance monitoring metrics */  
  uint32_t realtime_delay_idx;

  /* routing tables metrics */  
  uint32_t status_idx;
  uint32_t active_peers_cnt_idx;
  
  uint32_t unique_v4_pfxs_idx;
  uint32_t unique_v6_pfxs_idx;
  uint32_t unique_origin_as_idx;

  uint32_t affected_v4_pfxs_idx;
  uint32_t affected_v6_pfxs_idx;
  uint32_t announced_origin_as_idx;
  
  uint32_t valid_record_cnt_idx;
  uint32_t corrupted_record_cnt_idx;
  uint32_t empty_record_cnt_idx;
  
} __attribute__((packed)) collector_metric_idx_t;

/** A set that contains a unique set of peer ids */
KHASH_INIT(peer_id_set, uint32_t, char, 0, kh_int_hash_func, kh_int_hash_equal);
typedef khash_t(peer_id_set) peer_id_set_t;

/** Information about the current status 
 *  of a collector */
typedef struct struct_collector_t {

  /** graphite-safe collector string: project.collector */
  char collector_str[BGPSTREAM_UTILS_STR_NAME_LEN];

  /** unique set of peer ids that are associated
   *  peers providing information to the current */
  peer_id_set_t *collector_peerids;
  
  /** last time this collector was involved
   *  in bgp operations (bgp time) */
  uint32_t bgp_time_last;
  
  /** last time this collector was involved
   *  in valid bgp operations (wall time) */
  uint32_t wall_time_last;

  /** dump time of the current reference RIB */
  uint32_t bgp_time_ref_rib_dump_time;

  /** start time of the current reference RIB */
  uint32_t bgp_time_ref_rib_start_time;
  
  /** dump time of the current under construction
   *  RIB, if 0 the under construction process is
   *  off, is on otherwise */
  uint32_t bgp_time_uc_rib_dump_time;

  /** start time of the current under construction RIB */
  uint32_t bgp_time_uc_rib_start_time;

  /** Current status of the collector */
  collector_state_t state;

  /** Timeseries Key Package for the collector */
  timeseries_kp_t *kp;

  /** Indices of the collector metrics in the collector Key Package */
  collector_metric_idx_t kp_idxs;

} collector_t;


/** A map that associates an a collector_t
 *  structure with each collector */
KHASH_INIT(collector_data, char *, collector_t, 1, kh_str_hash_func, kh_str_hash_equal);
typedef khash_t(collector_data) collector_data_t;

/** Structure that manages all the routing
 *  tables that can be possibly built using
 *  the bgp stream in input */
struct struct_routingtables_t {

  /** Table of peer id <-> peer signature */
  bgpstream_peer_sig_map_t *peersigns;
  
  /** BGP view that contains the information associated
   *  with the active and inactive prefixes/peers/pfx-peer
   *  information. Every active field represents consistent
   *  states of the routing tables as seen by each peer
   *  of the each collector */
  bgpwatcher_view_t *view;

  /** iterator associated with the view*/
  bgpwatcher_view_iter_t *iter;
  
  /** per collector information: name, peers and
   *  current state */
  collector_data_t *collectors;
  
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

  /** a borrowed pointer for timeseries */
  timeseries_t *timeseries;
  
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

/** Read the view in the current routingtables instance and populate
 *  the metrics to be sent to the active timeseries back-ends
 *  
 * @param rt            pointer to a routingtables instance to read
 * @param timeseries    pointer to an initialized timeseries instance
 * @return 0 if the metrics were dumped correctly, <0 if an error occurred.
 */
int
routingtables_dump_metrics(routingtables_t *rt);


/** Generate the metrics associated to a specific peer
 *  
 * @param p             pointer to a peer user pointer
 * @return 0 if the metrics were generated correctly, <0 if an error occurred.
 */
int
peer_generate_metrics(perpeer_info_t *p);

/** Generate the metrics associated to a specific collector
 *  
 * @param c             pointer to a collector structure
 * @return 0 if the metrics were generated correctly, <0 if an error occurred.
 */
int
collector_generate_metrics(collector_t *c);

#endif /* __ROUTINGTABLES_INT_H */





