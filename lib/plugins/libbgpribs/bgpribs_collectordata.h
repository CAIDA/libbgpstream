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

#ifndef __BGPRIBS_COLLECTORDATA_H
#define __BGPRIBS_COLLECTORDATA_H

#include "config.h"
#include "bgpribs_common.h"
#include "bgpribs_peers_table.h"
#ifdef WITH_BGPWATCHER
#include "bgpribs_bgpwatcher_client.h"
#endif

/** @file
 *
 * @brief Header file that exposes the structures needed
 * to manage the information related to a single collector.
 *
 * @author Chiara Orsini
 *
 */


typedef enum {
  COLLECTOR_NULL = 0,    /// status of collector is unknown
  COLLECTOR_DOWN = 1,    /// status of collector is DOWN
  COLLECTOR_UP =   2     /// status of collector is UP
} collector_status_t;


/** collectordata
 *  this structure contains information about a single
 *  collector
 */
typedef struct collectordata {
  char * dump_project;        /// graphite-safe version of the project name 
  char * dump_collector;      /// graphite-safe version of the collector name
  long int most_recent_ts;    /// most recent timestamp "processed"
  int active_peers;           /// number of peers whose state is up
  collector_status_t status;  /// it tells whether the collector is up or down, or null  
  peers_table_t *peers_table; /// table containing information about each peer of the collector
  uint64_t record_types[BGPSTREAM_RECORD_TYPE_MAX]; /// number of records of a given type received in the interval
  /** bgp statistics extracted from all the active peers
   *  that describe the behavior within a single interval */
  aggregated_bgp_stats_t *aggr_stats;
} collectordata_t;



/** Allocate memory for a strucure that maintains
 *  the information about a single collector.
 *
 * @param project name of the project
 * @param project name of the collector
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
collectordata_t *collectordata_create(const char *project,
				      const char *collector);

/** The function considers the BGP information embedded into the
 *  bgpstream record structure and it modifies the current collector status
 *  accordingly.
 * 
 * @param collector_data a pointer to the collector data structure
 * @param bs_record a pointer to the bgpstream record under processing
 * @return 0 if the function ended correctly
 *        -1 if something went wrong during the function execution
 */
int collectordata_process_record(collectordata_t *collector_data,
				 bgpstream_record_t * bs_record);

#ifdef WITH_BGPWATCHER
/** The function prints the statistics of a given collector for the interval of time
 *  that starts at interval_start if the status is not NULL.
 * 
 * @param collector_data a pointer to the collector data structure
 * @param interval_start start of the interval in epoch time
 * @param bw_client a pointer to the bgpwatcher client used to send data to the bgpwatcher
 * @return 0 if the function ended correctly,
 *        -1 if something went wrong during the function execution
 */
int collectordata_interval_end(collectordata_t *collector_data, 
			       int interval_start, bw_client_t *bw_client);
#else
/** The function prints the statistics of a given collector for the interval of time
 *  that starts at interval_start if the status is not NULL.
 * 
 * @param collector_data a pointer to the collector data structure
 * @param interval_start start of the interval in epoch time
 * @return 0 if the function ended correctly,
 *        -1 if something went wrong during the function execution
 */
int collectordata_interval_end(collectordata_t *collector_data, 
			       int interval_start);
#endif

/** Deallocate memory for the collector's data.
 *
 * @param collector_data a pointer to the collector's data
 */
void collectordata_destroy(collectordata_t *collector_data);

#endif /* __BGPRIBS_COLLECTORDATA_H */
