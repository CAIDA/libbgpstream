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

#ifndef __BGPRIBS_BGPWATCHER_CLIENT_H
#define __BGPRIBS_BGPWATCHER_CLIENT_H

#ifdef WITH_BGPWATCHER

#include <bgpwatcher_client.h>

/** @file
 *
 * @brief Header file that exposes the structures needed
 * to handle the bgpwatcher_client within the bgpribs
 * plugin.
 *
 * @author Chiara Orsini
 *
 */

/** Set of data structures that are required to
 *  send peers and prefixes tables from this client
 *  to the server */
typedef struct bw_client {
  /** bgpwatcher client */
  bgpwatcher_client_t *client;
  /** prefix table on client */
  bgpwatcher_client_pfx_table_t *pfx_table;
  /** prefix record on client */
  bgpwatcher_pfx_record_t *pfx_record;
  /** peer table on client */
  bgpwatcher_client_peer_table_t *peer_table;
  /** peer record on client */
  bgpwatcher_peer_record_t *peer_record;
} bw_client_t;


/** Create a connection to the bgpwatcher server
 *  and allocate memory for peer and prefix tables
 *
 * @return a pointer to the bw_client structure, or
 *  NULL if an error occurred
 */
bw_client_t *bw_client_create();

/** Close the connection to the bgpwatcher server
 *  and deallocate the memory used for peer and
 *  prefix tables
 *
 * @param bwc   bw_client to destroy
 *
 */
void bw_client_destroy(bw_client_t * bwc);


#endif


#endif /* __BGPRIBS_BGPWATCHER_CLIENT_H */
