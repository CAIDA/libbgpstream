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

#ifndef __BGPRIBS_LIB_H
#define __BGPRIBS_LIB_H

#include "config.h"
#include "bgpstream_lib.h"


typedef struct bgpribs bgpribs_t;


/** Allocate memory for a strucure that maintains
 *  the information required to run the bgpribs plugin.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bgpribs_t *bgpribs_create(char *metric_pfx);


/** Set the prefix that will be prepended to each
 *  metric dumped to standard output.
 *
 */
void bgpribs_set_metric_pfx(bgpribs_t *bgp_ribs, char* met_pfx);


#ifdef WITH_BGPWATCHER
/** Turn on the bgpwatcher communication. */
int bgpribs_set_watcher(bgpribs_t *bgp_ribs);

/** Set the communication filters related to full feed peers */
void bgpribs_set_fullfeed_filters(bgpribs_t *bgp_ribs,
				  uint8_t ipv4_fff_on, uint8_t ipv6_fff_on,
				  uint32_t ipv4_ff_size, uint32_t ipv6_ff_size);
#endif


/** The function modifies the bgpribs plugin state in order
 *  to prepare it for a new interval to process.
 * 
 * @param bgp_ribs a pointer to the plugin data structure
 * @return 0 if the function ended correctly
 *        -1 if something went wrong during the function execution
 */
void bgpribs_interval_start(bgpribs_t *bgp_ribs, int interval_start);


/** The function considers the BGP information embedded into the
 *  bgpstream record structure and it modifies the current plugin status
 *  accordingly.
 * 
 * @param bgp_ribs a pointer to the plugin data structure
 * @param bs_record a pointer to the bgpstream record under processing
 * @return 0 if the function ended correctly
 *        -1 if something went wrong during the function execution
 */
int bgpribs_process_record(bgpribs_t *bgp_ribs, bgpstream_record_t *bs_record);


/** The function modifies the bgpribs plugin state in order
 *  to handle the end of the interval: it dumps statistics related
 *  to the interval that just finished and reset the structures to
 *  deal with a new one.
 * 
 * @param bgp_ribs a pointer to the plugin data structure
 * @return 0 if the function ended correctly
 *        -1 if something went wrong during the function execution
 */
int bgpribs_interval_end(bgpribs_t *bgp_ribs, int interval_end);


/** Deallocate memory for the bgpribs plugin
 *
 * @param bgp_ribs a pointer to the bgpribs's plugin
 */
void bgpribs_destroy(bgpribs_t *bgp_ribs);


#endif /* __BGPRIBS_LIB_H */
