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

#ifndef __ROUTINGTABLES_H
#define __ROUTINGTABLES_H

#include "bgpstream.h"
#include "timeseries.h"

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque handle that represents a routingtables instance */
typedef struct struct_routingtables_t routingtables_t;

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

#ifdef WITH_BGPWATCHER
/** Type of feed */
typedef enum {

  ROUTINGTABLES_FEED_IPV4_PARTIAL  = 0b0001,

  ROUTINGTABLES_FEED_IPV4_FULL     = 0b0010,

  ROUTINGTABLES_FEED_IPV6_PARTIAL  = 0b0100,

  ROUTINGTABLES_FEED_IPV6_FULL     = 0b1000,

} routingtables_feed_type_t;
#endif
/** @} */

/**
 * @name Public Constants
 *
 * @{ */

/** ROUTINGTABLES_ALL_FEEDS is the expression to use
 *  when we want to select all kinds of peers (ipv4 and
 *  ipv6, full and partial feeds) */
#define ROUTINGTABLES_ALL_FEEDS  ROUTINGTABLES_FEED_IPV4_PARTIAL | ROUTINGTABLES_FEED_IPV4_FULL | \
                                 ROUTINGTABLES_FEED_IPV6_PARTIAL | ROUTINGTABLES_FEED_IPV6_FULL

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Create a new routingtables instance
 *
 * @param plugin_name   string representing the plugin name
 * @param timeseries    pointer to an initialized timeseries instance
 * @return a pointer to a routingtables instance if successful, NULL otherwise
 */
routingtables_t *routingtables_create(char *plugin_name, timeseries_t *timeseries);

/** Set the metric prefix to be used for when outpting the time series
 *  variables at the end of the interval
 *  
 * @param rt               pointer to a routingtables instance to update
 * @param metric_prefix    metric prefix string
 */
void routingtables_set_metric_prefix(routingtables_t *rt,
                                    char *metric_prefix);

/** Get the metric prefix to be used for when outpting the time series
 *  variables at the end of the interval
 *  
 * @param rt               pointer to a routingtables instance to update
 * @return the current metric prefix string, NULL if an error occurred.
 */
char *routingtables_get_metric_prefix(routingtables_t *rt);

#ifdef WITH_BGPWATCHER
/** Activate the transmission of bgp views to a bgp watcher server
 *  at the end of every interval
 *  
 * @param rt               pointer to a routingtables instance to update
 * @param client_name      name representing this client identity
 *                         if NULL, default is chosen
 * @param server_uri       0MQ-style URI to connect to server
 *                         if NULL, default is chosen
 * @param tables_mask      mask that tells which routing tables should
 *                         populate the view to be sent to the watcher,
 *                         if 0 ALL kinds of tables are sent.
 * @return 0 if the option was set correctly, <0 if an error occurred.
 *
 * @todo add notes that indicates how to use the mask
 */
int routingtables_activate_watcher_tx(routingtables_t *rt,
                                      char *client_name,
                                      char *server_uri,
                                      uint8_t tables_mask);
#endif


/** Set the value of threshold used to consider a peer a full feed
 *  
 * @param rt            pointer to a routingtables instance to update
 * @param ip_version    IP version 
 * @param threshold     number of prefixes that populate a full feed
 *                      routing table
 */
void routingtables_set_fullfeed_threshold(routingtables_t *rt,
                                         bgpstream_addr_version_t ip_version,
                                         uint32_t threshold);

/** get the value of threshold used to consider a peer a full feed
 *  
 * @param rt            pointer to a routingtables instance to update
 * @param ip_version    IP version 
 * @return the value of the threshold.
 */
int routingtables_get_fullfeed_threshold(routingtables_t *rt,
                                         bgpstream_addr_version_t ip_version);

/** Receive the beginning of interval signal
 *  
 * @param rt            pointer to a routingtables instance to update
 * @param start_time    start of the interval in epoch time (bgp time)
 * @return 0 if the signal was processed correctly, <0 if an error occurred.
 */
int routingtables_interval_start(routingtables_t *rt,
                                 int start_time);

/** Receive the end of interval signal (and trigger the output of 
 *  statistics as well as the transmission of bgp views to the bgpwatcher
 *  if the transmission is activated)
 *  
 * @param rt            pointer to a routingtables instance to update
 * @param end_time      end of the interval in epoch time (bgp time)
 * @return 0 if the signal was processed correctly, <0 if an error occurred.
 */
int routingtables_interval_end(routingtables_t *rt,
                               int end_time);

/** Process the bgpstream record, i.e. use the information contained in the
 *  bgpstream record to update the current routing tables
 *
 * @param rt            pointer to a routingtables instance to update
 * @param record        pointer to a bgpstream record instance 
 * @return 0 if the record was processed correctly, <0 if an error occurred.
 */
int routingtables_process_record(routingtables_t *rt,
                                 bgpstream_record_t *record);

/** Destroy the given routingtables instance
 *
 * @param rt pointer to a routingtables instance to destroy
 */
void routingtables_destroy(routingtables_t *rt);


/** @} */


#endif /* __ROUTINGTABLES_H */
