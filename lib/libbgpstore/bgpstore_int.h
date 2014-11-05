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

#include "bgpstore_common.h"

#include "bl_bgp_utils.h"
#include "bl_peersign_map.h"

#include "khash.h"



#define BGPSTORE_TS_WDW_LEN      60
#define BGPSTORE_TS_WDW_SIZE     30 * BGPSTORE_TS_WDW_LEN
#define BGPSTORE_BGPVIEW_TIMEOUT 1800 


typedef enum {BGPSTORE_STATE_UNKNOWN        = 0,
	      BGPSTORE_WDW_EXCEEDED         = 1,
	      BGPSTORE_CLIENT_DISCONNECT    = 2,
	      BGPSTORE_TABLE_END            = 3,
	      BGPSTORE_TIMEOUT_EXPIRED      = 4
} bgpstore_completion_trigger_t;




KHASH_INIT(timebgpview, uint32_t, bgpview_t*, 1,
	   kh_int_hash_func, kh_int_hash_equal);


struct bgpstore {
  /** aggregated view of bgpdata organized by time:
   *  for each timestamp we build a bgpview */
  khash_t(timebgpview) *bgp_timeseries;
  /** active_clients contains, for each registered/active
   *  client (i.e. those that are currently connected)
   *  its status.*/
  clientinfo_map_t *active_clients;
  /**  id <-> (collector,peer) caches
   *   These structures associate a numeric id (a bpgstore 
   *    id, aka bs_id, represented by a uint16) to each
   *   (collector,peer) pair. This identifier is shared
   *   by all the bgpviews and it is constant over time.
   *   Also, it enables faster lookup in the bgpview
   *   prefix tables. */
  bl_peersign_map_t *peer_signature_id;
  /** sliding window interval:
   *  we process a maximum of 30 timestamps 
   *  at a given time, when a new ts arrive
   *  TODO: finish documentation */
  uint32_t min_ts;
  
};

int bpgstore_check_timeouts(bgpstore_t *bgp_store);
int bgpstore_completion_check(bgpstore_t *bgp_store, bgpview_t *bgp_view, uint32_t ts, bgpstore_completion_trigger_t trigger);
int bgpstore_remove_view(bgpstore_t *bgp_store, uint32_t ts);


#endif /* __BGPSTORE_INT_H */






