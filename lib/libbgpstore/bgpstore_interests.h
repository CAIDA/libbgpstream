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


#ifndef __BGPSTORE_INTERESTS_H
#define __BGPSTORE_INTERESTS_H



#include "bgpstore_common.h"  // clientinfo_map_t
#include "bgpstore_lib.h"
#include "bgpstore_int.h"
#include "bgpstore_bgpview.h" // bgpview_t


#define IPV4_FULLFEED   400000
#define IPV6_FULLFEED    10000
#define ROUTED_PFX_PEERCOUNT    10


// TODO: comments about interest

typedef struct perasvisibility_interest perasvisibility_interest_t;

perasvisibility_interest_t* perasvisibility_interest_create(bgpview_t *bgp_view, uint32_t ts);
int perasvisibility_interest_send(perasvisibility_interest_t* peras_vis, char *client);
void perasvisibility_interest_destroy(perasvisibility_interest_t* peras_vis);



typedef struct bgpviewstatus_interest bgpviewstatus_interest_t;

bgpviewstatus_interest_t* bgpviewstatus_interest_create(bgpview_t *bgp_view, uint32_t ts);
int bgpviewstatus_interest_send(bgpviewstatus_interest_t* bvstatus, char* client);
void bgpviewstatus_interest_destroy(bgpviewstatus_interest_t* bvstatus);




#endif /* __BGPSTORE_INTERESTS */
