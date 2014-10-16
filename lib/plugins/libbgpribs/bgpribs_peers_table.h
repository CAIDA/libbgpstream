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

#ifndef __BGPRIBS_PEERS_TABLE_H
#define __BGPRIBS_PEERS_TABLE_H

#include "config.h"
#include "bgpribs_peerdata.h"
#include "bgpribs_common.h"
#include "bgpribs_khash.h"
#ifdef WITH_BGPWATCHER
#include "bgpribs_bgpwatcher_client.h"
#endif

/** @file
 *
 * @brief Header file that exposes the structures needed
 * to manage a set of ipv4 and ipv6 peers.
 *
 * @author Chiara Orsini
 *
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


/** peers table
 *  this structure contains two maps, one for ipv4 peers
 *  and one for ipv6 peers. Each one associates the IP
 *  address of the peer to a peerdata structure
 */
typedef struct struct_peers_table_t {
  khash_t(ipv4_peers_table_t) * ipv4_peers_table;
  khash_t(ipv6_peers_table_t) * ipv6_peers_table;
} peers_table_t;



/** Allocate memory for a strucure that maintains
 *  the information about a set of ipv4 and ipv6
 *  peers (owned by a single collector).
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
peers_table_t *peers_table_create();


/** The function considers the BGP information embedded into the
 *  bgpstream record structure, and it forward such information to
 *  the appropriate set of involved peers.
 * 
 * @param peers_table a pointer to the peers_table structure
 * @param bs_record a pointer to the bgpstream record under processing
 * @return the number of active peers (i.e. peers whose state is UP), or
 *        -1 if something went wrong during the function execution
 */
int peers_table_process_record(peers_table_t *peers_table, 
			       bgpstream_record_t * bs_record);



#ifdef WITH_BGPWATCHER
/** The function prints the statistics of a set of peers belonging to a
 *  specific peers. Such information relates to the interval that starts
 *  at interval_start. The function also sends the current status of each
 *  peer to a bgpwatcher server.
 * 
 * @param project_str name of the project
 * @param collector_str name of the collector
 * @param peers_table set of peers for the current collector
 * @param collector_aggr_stats a pointer to a structure that aggregates data collector-wise
 * @param bw_client a pointer to the bgpwatcher client used to send data to the bgpwatcher
 * @param interval_start start of the interval in epoch time
 * @return 0 if the function ended correctly,
 *        -1 if something went wrong during the function execution
 */
int peers_table_interval_end(char *project_str, char *collector_str,
			     peers_table_t *peers_table,
			     aggregated_bgp_stats_t * collector_aggr_stats,
			     bw_client_t *bw_client,
			     int interval_start);
#else
/** The function prints the statistics of a set of peers belonging to a
 *  specific peers. Such information relates to the interval that starts
 *  at interval_start. The function also sends the current status of each
 *  peer to a bgpwatcher server.
 * 
 * @param project_str name of the project
 * @param collector_str name of the collector
 * @param peers_table set of peers for the current collector
 * @param collector_aggr_stats a pointer to a structure that aggregates data collector-wise
 * @param bw_client a pointer to the bgpwatcher client used to send data to the bgpwatcher
 * @param interval_start start of the interval in epoch time
 * @return 0 if the function ended correctly,
 *        -1 if something went wrong during the function execution
 */
int peers_table_interval_end(char *project_str, char *collector_str,
			     peers_table_t *peers_table,
			     aggregated_bgp_stats_t * collector_aggr_stats,
			     int interval_start);
#endif


/** Deallocate memory for the peers-table's data.
 *
 * @param peers_table a pointer to the peers-table's data
 */
void peers_table_destroy(peers_table_t *peers_table) ;


#endif /* __BGPRIBS_PEERS_TABLE_H */
