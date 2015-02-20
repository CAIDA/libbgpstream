/*
 * This file is part of bgpwatcher
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */


#include "bgpstore_interests.h"
#include "bgpstore_common.h"

#include "bgpwatcher_common.h" // interests masks

#include "utils.h"

#include "bl_bgp_utils.h"
#include "bgpstream_utils_id_set.h"
#include "bl_pfx_set.h"

#include "khash.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>



struct bgpviewstatus_interest {
  // timestamp
  uint32_t ts;
  int done_clients;
  int active_peers;
  int peers_done;
  int peers_full_feed_done;
  int inactive_peers;
};


bgpviewstatus_interest_t* bgpviewstatus_interest_create(bgpview_t *bgp_view,
							uint32_t ts)
{
  bgpviewstatus_interest_t *bvstatus = (bgpviewstatus_interest_t *)malloc_zero(sizeof(bgpviewstatus_interest_t));

  khiter_t k;
  uint16_t i;
  active_peer_status_t *aps;
  bvstatus->ts = ts;
  bvstatus->done_clients = kh_size(bgp_view->done_clients);
  bvstatus->active_peers = kh_size(bgp_view->active_peers_info);
  bvstatus->inactive_peers = kh_size(bgp_view->inactive_peers);

  for (k = kh_begin(bgp_view->active_peers_info); 
       k != kh_end(bgp_view->active_peers_info); ++k)
    {
      if (kh_exist(bgp_view->active_peers_info, k))
  	{
  	  i = kh_key(bgp_view->active_peers_info, k);
  	  aps = &(kh_value(bgp_view->active_peers_info, k));
	  if(aps->expected_pfx_tables_cnt == aps->received_pfx_tables_cnt)
	    {
	      bvstatus->peers_done++;
	      if(aps->recived_ipv4_pfx_cnt > IPV4_FULLFEED ||
		 aps->recived_ipv6_pfx_cnt > IPV6_FULLFEED )
		{
		  bvstatus->peers_full_feed_done++;		  
		}
	    }
	}
    }
  	     
  return bvstatus;
}


int bgpviewstatus_interest_send(bgpviewstatus_interest_t* bvstatus, char* client)
{
  time_t timer;
  char buffer[25];
  struct tm* tm_info;
  time(&timer);
  tm_info = localtime(&timer);
  strftime(buffer, sizeof(buffer), "%H:%M:%S", tm_info);

  fprintf(stdout,"\n[%s] processing bgp time: %d \n", buffer, bvstatus->ts);
  fprintf(stdout,"\tDONE CLIENTS:\t%d\n", bvstatus->done_clients);
  fprintf(stdout,"\tINACTIVE PEERS:\t%d\n", bvstatus->inactive_peers);
  fprintf(stdout,"\tACTIVE PEERS:\t%d\n", bvstatus->active_peers);
  fprintf(stdout,"\tDONE PEERS:\t%d\n", bvstatus->peers_done);
  fprintf(stdout,"\tFULL FEED DONE PEERS:\t%d\n", bvstatus->peers_full_feed_done);
  return 0;
}


void bgpviewstatus_interest_destroy(bgpviewstatus_interest_t* bvstatus)
{
  if(bvstatus !=NULL)
    {
      free(bvstatus);
    }
}


