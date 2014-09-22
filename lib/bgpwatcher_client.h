/*
 * bgpwatcher
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#ifndef __BGPWATCHER_CLIENT_H
#define __BGPWATCHER_CLIENT_H

#include <stdint.h>

#include <bgpwatcher_common.h>

/** @file
 *
 * @brief Header file that exposes the public interface of the bgpwatcher client
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/** Default URI for the server -> client connection */
#define BGPWATCHER_CLIENT_SERVER_URI_DEFAULT "tcp://127.0.0.1:6300"

/** Default the client request timeout to 2.5 seconds */
#define BGPWATCHER_CLIENT_REQUEST_TIMEOUT 2500

/** Default the client request retry count to 3 */
#define BGPWATCHER_CLIENT_REQUEST_RETRIES 3

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

typedef struct bgpwatcher_client bgpwatcher_client_t;

typedef struct bgpwatcher_client_pfx_table bgpwatcher_client_pfx_table_t;

typedef struct bgpwatcher_client_peer_table bgpwatcher_client_peer_table_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** @} */

/** Initialize a new BGP Watcher client instance
 *
 * @return a pointer to a bgpwatcher client instance if successful, NULL if an
 * error occurred.
 */
bgpwatcher_client_t *bgpwatcher_client_init();

/** Start the given bgpwatcher client instance
 *
 * @param client       pointer to a bgpwatcher client instance to start
 * @return 0 if the client started successfully, -1 otherwise.
 */
int bgpwatcher_client_start(bgpwatcher_client_t *client);

/** Prints the error status (if any) to standard error and clears the error
 * state
 *
 * @param client       pointer to bgpwatcher client instance to print error for
 */
void bgpwatcher_client_perr(bgpwatcher_client_t *client);

/** @todo add other error functions if needed (is_err, get_err) */

/** Create a re-usable prefix table
 *
 * @param client        pointer to bgpwatcher client instance to associate the
 *                      table with
 * @return pointer to a bgpwatcher pfx table instance if successful, NULL
 * otherwise
 */
bgpwatcher_client_pfx_table_t *bgpwatcher_client_pfx_table_create(
						   bgpwatcher_client_t *client);

/** Free a prefix table
 *
 * @param table         pointer to the prefix table to free
 */
void bgpwatcher_client_pfx_table_free(bgpwatcher_client_pfx_table_t **table);

/** Set the time that a table represents
 *
 * @param time          new time to set for the table
 */
void bgpwatcher_client_pfx_table_set_time(bgpwatcher_client_pfx_table_t *table,
					  uint32_t time);

/** Add a prefix record to the given prefix table
 *
 * @param table         pointer to prefix table to add prefix record to
 * @param prefix        pointer to a completed prefix record
 * @return 0 if the prefix was added successfully, -1 otherwise
 *
 * @note the caller maintains ownership of the prefix record
 */
int bgpwatcher_client_pfx_table_add(bgpwatcher_client_pfx_table_t *table,
				    bgpwatcher_pfx_record_t *pfx);

/** Flush the given prefix table to the bgpwatcher server
 *
 * @param table         pointer to prefix table to flush
 * @return 0 if the table was flushed successfully, -1 otherwise
 *
 * @note depending on implementation, this function may send the entire table in
 * one message, or the rows may have been sent as they were added to the
 * table. In either case, this function will notify the server that all rows for
 * the table have been received. You may also safely reuse the table after
 * calling flush -- all rows will have been removed.
 */
int bgpwatcher_client_pfx_table_flush(bgpwatcher_client_pfx_table_t *table);

/** Create a re-usable peer table
 *
 * @param client        pointer to bgpwatcher client instance to associate the
 *                      table with
 * @return pointer to a bgpwatcher peer table instance if successful, NULL
 * otherwise
 */
bgpwatcher_client_peer_table_t *bgpwatcher_client_peer_table_create(
						   bgpwatcher_client_t *client);

/** Set the time that a table represents
 *
 * @param time          new time to set for the table
 */
void bgpwatcher_client_peer_table_set_time(bgpwatcher_client_peer_table_t *table,
					   uint32_t time);

/** Add a peer record to the given peer table
 *
 * @param table         pointer to peer table to add peer record to
 * @param peer          pointer to a completed peer record
 * @return 0 if the peer was added successfully, -1 otherwise
 *
 * @note the caller maintains ownership of the peer record
 */
int bgpwatcher_client_peer_table_add(bgpwatcher_client_peer_table_t *table,
				     bgpwatcher_peer_record_t *peer);

/** Flush the given peer table to the bgpwatcher server
 *
 * @param table         pointer to peer table to flush
 * @return 0 if the table was flushed successfully, -1 otherwise
 *
 * @note depending on implementation, this function may send the entire table in
 * one message, or the rows may have been sent as they were added to the
 * table. In either case, this function will notify the server that all rows for
 * the table have been received. You may also safely reuse the table after
 * calling flush -- all rows will have been removed.
 */
int bgpwatcher_client_peer_table_flush(bgpwatcher_client_peer_table_t *table);

/** Stop the given bgpwatcher client instance
 *
 * @param client       pointer to the bgpwatcher client instance to stop
 */
void bgpwatcher_client_stop(bgpwatcher_client_t *client);

/** Free the given bgpwatcher client instance
 *
 * @param client       pointer to the bgpwatcher client instance to free
 */
void bgpwatcher_client_free(bgpwatcher_client_t *client);

/** Set the URI for the client to connect to the server on
 *
 * @param client        pointer to a bgpwatcher client instance to update
 * @param uri           pointer to a uri string
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_client_set_server_uri(bgpwatcher_client_t *client,
				     const char *uri);

/** Set the heartbeat interval
 *
 * @param client        pointer to a bgpwatcher client instance to update
 * @param interval_ms   time in ms between heartbeats
 *
 * @note defaults to BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT
 */
void bgpwatcher_client_set_heartbeat_interval(bgpwatcher_client_t *client,
					      uint64_t interval_ms);

/** Set the heartbeat liveness
 *
 * @param client        pointer to a bgpwatcher client instance to update
 * @param beats         number of heartbeats that can go by before a client is
 *                      declared dead
 *
 * @note defaults to BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT
 */
void bgpwatcher_client_set_heartbeat_liveness(bgpwatcher_client_t *client,
					   int beats);

/** Set the minimum reconnect time
 *
 * @param client        pointer to a bgpwatcher client instance to update
 * @param time          min time in ms to wait before reconnecting to server
 *
 * @note defaults to BGPWATCHER_RECONNECT_INTERVAL_MIN
 */
void bgpwatcher_client_set_reconnect_interval_min(bgpwatcher_client_t *client,
					       uint64_t reconnect_interval_min);

/** Set the maximum reconnect time
 *
 * @param client        pointer to a bgpwatcher client instance to update
 * @param time          max time in ms to wait before reconnecting to server
 *
 * @note defaults to BGPWATCHER_RECONNECT_INTERVAL_MAX
 */
void bgpwatcher_client_set_reconnect_interval_max(bgpwatcher_client_t *client,
					       uint64_t reconnect_interval_max);

/** Set the identity string for this client
 *
 * @param client        pointer to a bgpwatcher client instance to update
 * @param identity      globally unique identity string
 * @return 0 if the identity was update successfully, -1 otherwise
 *
 * @note if an identity is not set, a random ID will be generated on every
 * connect to the server. This may/will cause problems if/when a server goes
 * away. Any pending transactions may be lost. Please set an ID.
 */
int bgpwatcher_client_set_identity(bgpwatcher_client_t *client,
				   const char *identity);

#endif
