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


#include "bgpstore_interests.h"
#include "bgpstore_common.h"

#include "bgpwatcher_common.h" // interests masks

#include "utils.h"

#include "bl_bgp_utils.h"
#include "bl_str_set.h"
#include "bl_id_set.h"
#include "bl_pfx_set.h"

#include "khash.h"
#include <assert.h>



struct bgpviewstatus_interest {
  // timestamp
  uint32_t ts;
  // TODO
};


bgpviewstatus_interest_t* bgpviewstatus_interest_create(bgpview_t *bgp_view,
							uint32_t ts)
{
  bgpviewstatus_interest_t *bvstatus = (bgpviewstatus_interest_t *)malloc_zero(sizeof(bgpviewstatus_interest_t));

  // --------------------------------------------------------------------
  khiter_t k;
  uint16_t i;
  active_peer_status_t *aps;
  /* printf("TIME: %d\n", ts); */
  /* printf("\tC: %d \tA: %d \tI: %d \n", */
  /* 	 kh_size(bgp_view->done_clients), */
  /* 	 kh_size(bgp_view->active_peers_info), */
  /* 	 kh_size(bgp_view->inactive_peers)); */
  /* printf("ipv4: %d \tipv6: %d\n", */
  /* 	 kh_size(bgp_view->aggregated_pfxview_ipv4), */
  /* 	 kh_size(bgp_view->aggregated_pfxview_ipv6)); */
  /* for (k = kh_begin(bgp_view->active_peers_info); */
  /*      k != kh_end(bgp_view->active_peers_info); ++k) */
  /*   { */
  /*     if (kh_exist(bgp_view->active_peers_info, k))	 */
  /* 	{ */
  /* 	  i = kh_key(bgp_view->active_peers_info, k); */
  /* 	  aps = &(kh_value(bgp_view->active_peers_info, k)); */
  /* 	  printf("%d: tables (%d - %d)\n\tipv4: %d \tipv6: %d\n", */
  /* 		 i, aps->expected_pfx_tables_cnt, aps->received_pfx_tables_cnt, */
  /* 		 aps->recived_ipv4_pfx_cnt, aps->recived_ipv6_pfx_cnt); */
  /* 	  printf("\n"); */
  /* 	} */
  /*   } */
  // --------------------------------------------------------------------
	     
  return bvstatus;
}


int bgpviewstatus_interest_send(bgpviewstatus_interest_t* bvstatus, char* client)
{
  return 0;
}


void bgpviewstatus_interest_destroy(bgpviewstatus_interest_t* bvstatus)
{
  if(bvstatus !=NULL)
    {
      free(bvstatus);
    }
}


