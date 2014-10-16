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

#ifndef __BGPRIBS_PEERDATA_H
#define __BGPRIBS_PEERDATA_H

#include <assert.h>
#include "khash.h"
#include "config.h"
#include "utils.h"
#include "bgpribs_common.h"
#include "bgpribs_ribs_table.h"
#ifdef WITH_BGPWATCHER
#include "bgpribs_bgpwatcher_client.h"
#endif

/** @file
 *
 * @brief Header file that exposes the structures needed
 * to manage the information related to a single peer.
 *
 * @author Chiara Orsini
 *
 */


/** status of the peer */
typedef enum  {
  PEER_NULL = 0,   /// status of peer is unknown (i.e. data available are not enough to tell what is the peer status)
  PEER_DOWN = 1,   /// status of peer is DOWN (i.e. the peer went down)
  PEER_UP =   2    /// status of peer is UP (i.e. there is a consistent RIB in memory)
} peer_status_t;

/** flag that indicates if there is a RIB table
 *  under construction */
typedef enum  {
  UC_OFF = 0,     // there is no under construction ribs_table
  UC_ON = 1,      // we are constructing a new ribs_table
} ribs_tables_status_t;

/** peerdata
 *  this structure contains information about a single
 *  peer.
 */
typedef struct struct_peerdata_t {
  char * peer_address_str;          /// graphite-safe version of the peer ip address string
  peer_status_t status;             /// current status of the peer
  ribs_table_t * active_ribs_table; /// active ribs table, i.e. the current consistent RIB
  ribs_table_t * uc_ribs_table;     /// under-construction RIB
  long int most_recent_ts;          /// last time this peer was updated
  ribs_tables_status_t rt_status;   /// current status of RIBs
  // statistics to reset every time the interval ends
  uint64_t elem_types[BGPSTREAM_ELEM_TYPE_MAX];  /// number of elements of a given type received in an interval
  /** statistics associated with one or multiple peers 
   *  that describe the behavior within a single interval */
  aggregated_bgp_stats_t *aggr_stats;            
} peerdata_t;


/** Allocate memory for a strucure that maintains
 *  the information about a single peer.
 *
 * @param peer_address a pointer to the IP address of the peer
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
peerdata_t *peerdata_create(bgpstream_ip_address_t * peer_address);

/** The function considers the BGP information embedded into the
 *  bgpstream element as well as the information within the bgpstream
 *  record structure and it modifies the current peer status and ribs
 *  so that the peerdata is updated.
 * 
 * @param peer_data a pointer to the peerdata structure
 * @param bs_record a pointer to the bgpstream record under processing
 * @param bs_elem a pointer to the bgpstream element under processing
 * @return 0 if the function ended correctly, or a negative 
 *  number if an error occurred
 */
int peerdata_apply_elem(peerdata_t *peer_data, bgpstream_record_t * bs_record, bgpstream_elem_t *bs_elem);


/** The function considers the BGP information embedded into the
 *  bgpstream record structure and it modifies the current peer status
 *  accordingly.
 * 
 * @param peer_data a pointer to the peerdata structure
 * @param bs_record a pointer to the bgpstream record under processing
 * @return 1 if the peer status is UP at the end of the function,
 *         0 if the peer status is NULL or DOWN at the end of the function,
 *        -1 if something went wrong during the function execution
 */
int peerdata_apply_record(peerdata_t *peer_data, bgpstream_record_t * bs_record);



#ifdef WITH_BGPWATCHER
/** The function prints the statistics of a given peer for the interval of time
 *  that starts at interval_start, and it sends the current peer RIB to a
 *  a bgpwatcher server if the status of the peer is currently UP.
 * 
 * @param project_str name of the project
 * @param collector_str name of the collector
 * @param peer_address IP address of the current peer
 * @param peer_data a pointer to the peerdata structure
 * @param collector_aggr_stats a pointer to a structure that aggregates data collector-wise
 * @param bw_client a pointer to the bgpwatcher client used to send data to the bgpwatcher
 * @param interval_start start of the interval in epoch time
 * @return 0 if the function ended correctly,
 *        -1 if something went wrong during the function execution
 */
int peerdata_interval_end(char *project_str, char *collector_str,
			  bgpstream_ip_address_t peer_address, peerdata_t *peer_data,
			  aggregated_bgp_stats_t * collector_aggr_stats,
			  bw_client_t *bw_client,
			  int interval_start);
#else
/** The function prints the statistics of a given peer for the interval of time
 *  that starts at interval_start.
 * 
 * @param project_str name of the project
 * @param collector_str name of the collector
 * @param peer_address IP address of the current peer
 * @param peer_data a pointer to the peerdata structure
 * @param collector_aggr_stats a pointer to a structure that aggregates data collector-wise
 * @param interval_start start of the interval in epoch time
 * @return 0 if the function ended correctly,
 *        -1 if something went wrong during the function execution
 */
int peerdata_interval_end(char *project_str, char *collector_str,
			  bgpstream_ip_address_t peer_address, peerdata_t *peer_data,
			  aggregated_bgp_stats_t * collector_aggr_stats,
			  int interval_start);
#endif


/** Deallocate memory for the peer's data.
 *
 * @param peer_data a pointer to the peer's data
 */
void peerdata_destroy(peerdata_t *peer_data);


#endif /* __BGPRIBS_PEERDATA_H */
