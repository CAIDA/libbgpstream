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

#ifndef __BGPSTORE_LIB_H
#define __BGPSTORE_LIB_H

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include <assert.h>

#include "bl_bgp_utils.h"


typedef struct bgpstore bgpstore_t;


/** Allocate memory for a strucure that maintains
 *  the information required to store and orgranize
 *  peer and prefix tables received from bgpwatcher
 *  clients.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
bgpstore_t *bgpstore_create();

// TODO: documentation
int bgpstore_client_connect(bgpstore_t *bgp_store, char *client_name,
			    uint8_t client_interests, uint8_t client_intents);

// TODO: documentation
int bgpstore_client_disconnect(bgpstore_t *bgp_store, char *client_name);



// TODO: documentation
// every table end triggers a completion check for the table_time associated
int bgpstore_some_table_end(bgpstore_t *bgp_store, char *client_name,
			    uint32_t table_time, char *collector_str,
			    bl_addr_storage_t *peer_ip);


/** Deallocate memory for the bgpstore structure
 *
 * @param bgp_store a pointer to the bgpstore memory
 */
void bgpstore_destroy(bgpstore_t *bgp_store);


#endif /* __BGPSTORE_LIB_H */
