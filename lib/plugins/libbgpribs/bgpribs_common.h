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

#ifndef __BGPRIBS_COMMON_H
#define __BGPRIBS_COMMON_H

#include <assert.h>
#include <stdio.h>
#include "bl_bgp_utils.h"
#include "bl_pfx_set.h"
#include "bl_id_set.h"
#include "bgpstream_lib.h"

// #include "bgpribs_ases_table.h"
// #include "bgpribs_prefixes_table.h"

/** @file
 *
 * @brief Header file that exposes some common
 * utility functions and constants.
 *
 * @author Chiara Orsini
 *
 */

#define METRIC_PREFIX "bgp.test.bgpribs"


/** Modifies the string provided so that it
 *  does not create conflicts in charthouse
 *  metrics- hierarchy/organization.
 *
 * @param p a pointer to the string to check and modify
 */
void graphite_safe(char *p);

/** An set of BGP statistics that 
 *  could be computed per peer or aggregated
 *  per collector, per project, or any other
 *  grouping that considers one peer or more.
 */
typedef struct aggregated_bgp_stats {
  /// set of unique prefixes that are at least in one rib at the end of the interval
  bl_ipv4_pfx_set_t *unique_ipv4_prefixes;    
  bl_ipv6_pfx_set_t *unique_ipv6_prefixes;
  /// number of unique origin ASes observed at the end of the interval
  bl_id_set_t *unique_origin_ases;  
   /// set of unique prefixes affected by at least one update during the interval
  bl_ipv4_pfx_set_t *affected_ipv4_prefixes;
  bl_ipv6_pfx_set_t *affected_ipv6_prefixes;
  /// set of unique origin ASes announcing at least one prefix during the interval
  bl_id_set_t *announcing_origin_ases; 
} aggregated_bgp_stats_t;


#endif /* __BGPRIBS_COMMON_H */

