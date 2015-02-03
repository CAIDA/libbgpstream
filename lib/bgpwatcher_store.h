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

#ifndef __BGPWATCHER_STORE_H
#define __BGPWATCHER_STORE_H

#include "bgpwatcher_server.h"

/** @file
 *
 * @brief Header file that exposes the protected interface of bgpwatcher store.
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

typedef struct bgpwatcher_store bgpwatcher_store_t;

/** @} */

/** Create a new bgpwatcher store instance
 *
 * @param server        pointer to the bgpwatcher server instance
 * @param window_len    number of consecutive views in the store's window
 * @return a pointer to a bgpwatcher store instance, or NULL if an error
 * occurred
 */
bgpwatcher_store_t *bgpwatcher_store_create(bgpwatcher_server_t *server,
					    int window_len);

/** Destroy the given bgpwatcher store instance
 *
 * @param store         pointer to the store instance to destroy
 */
void bgpwatcher_store_destroy(bgpwatcher_store_t *store);

/** Register a new bgpwatcher client
 *
 * @param store         pointer to a store instance
 * @param name          string name of the client
 * @param interests     client interests
 * @param intents       client intents
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_store_client_connect(bgpwatcher_store_t *store,
                                    bgpwatcher_server_client_info_t *client);

/** Deregister a bgpwatcher client
 *
 * @param store         pointer to a store instance
 * @param name          string name of the client that disconnected
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_store_client_disconnect(bgpwatcher_store_t *store,
                                       bgpwatcher_server_client_info_t *client);


/** Begin receiving a new table from the server
 *
 * @param store         pointer to a store instance
 * @param table         pointer to the table to begin
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_store_prefix_table_begin(bgpwatcher_store_t *store,
                                        bgpwatcher_pfx_table_t *table);

/** Handle a prefix row for an existing table
 *
 * @param store         pointer to a store instance
 * @param table         pointer to the table the row belongs to
 * @param row           pointer to the row to handle
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_store_prefix_table_row(bgpwatcher_store_t *store,
                                      bgpwatcher_pfx_table_t *table,
                                      bl_pfx_storage_t *pfx,
                                      bgpwatcher_pfx_peer_info_t *peer_infos);

/** Complete the given table
 *
 * @param store         pointer to a store instance
 * @param client        string name of the client that completed the table
 * @param table         pointer to the table to complete
 * @return 0 if successful, -1 otherwise
 *
 * @note every table end triggers a completion check for the table_time
 * associated
 */
int bgpwatcher_store_prefix_table_end(bgpwatcher_store_t *store,
                                      bgpwatcher_server_client_info_t *client,
                                      bgpwatcher_pfx_table_t *table);

/** Force a timeout check on the views currently in the store
 *
 * @param store         pointer to a store instance
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_store_check_timeouts(bgpwatcher_store_t *store);

#endif /* __BGPSTORE_LIB_H */
