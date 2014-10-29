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

#include <bgpstream_elem.h>

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

/** Default time that the client will wait for outstanding messages when
    shutting down (in milliseconds) */
#define BGPWATCHER_CLIENT_SHUTDOWN_LINGER_DEFAULT 120000

/** Default request timeout */
#define BGPWATCHER_CLIENT_REQUEST_TIMEOUT_DEFAULT 60000

/** Default request retry count  */
#define BGPWATCHER_CLIENT_REQUEST_RETRIES_DEFAULT 3

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

  /** Signals that the server has received a request made by the client.
   *
   * @param client      pointer to the client instance that received the reply
   * @param seq_num     sequence number for matching with the request
   *
   * @note receipt of this message does not indicate that the server
   * successfully processed the message, just that it was successfully received.
   */
typedef void (bgpwatcher_client_cb_handle_reply_t)(bgpwatcher_client_t *client,
						   seq_num_t seq_num,
						   void *user);

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** @} */

/** Initialize a new BGP Watcher client instance
 *
 * @param interests     set of bgpwatcher_consumer_interest_t flags
 * @param intents       set of bgpwatcher_producer_intent_t flags
 * @return a pointer to a bgpwatcher client instance if successful, NULL if an
 * error occurred.
 *
 * @note calling a producer function or registering a consumer callback for an
 * intent/interest not registered will trigger an assert.
 */
bgpwatcher_client_t *bgpwatcher_client_init(uint8_t interests, uint8_t intents);

/** Register a function to be called to handle message replies from the server
 *
 * @param client        pointer to a client instance to set callback for
 * @param cb            pointer to a handle_reply callback function
 */
void bgpwatcher_client_set_cb_handle_reply(bgpwatcher_client_t *client,
				       bgpwatcher_client_cb_handle_reply_t *cb);

/** Set the user data that will provided to each callback function */
void bgpwatcher_client_set_cb_userdata(bgpwatcher_client_t *client,
				       void *user);

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

/** Begin a new prefix table dump
 *
 * @param time          new time to set for the table
 * @return a unique message id used for asynchronous replies. this number can
 * (optionally) be matched against those received in the handle_reply callbacks.
 */
int bgpwatcher_client_pfx_table_begin(bgpwatcher_client_pfx_table_t *table,
                                      char *collector_name,
                                      bgpstream_ip_address_t *peer_ip,
				      uint32_t time);

/** Add a prefix record to the given prefix table
 *
 * @param table           pointer to prefix table to add prefix record to
 * @param prefix          pointer to a bgpstream prefix
 * @param peer_ip         pointer to a bgpstream ip address of the peer
 * @param orig_asn        value of the origin ASN
 * @param collector_name  pointer to a string collector name
 * @return 0 if the prefix was added successfully, -1 otherwise
 *
 * @note the caller maintains ownership of the prefix record
 */
int bgpwatcher_client_pfx_table_add(bgpwatcher_client_pfx_table_t *table,
				    bgpstream_prefix_t *prefix,
                                    uint32_t orig_asn);

/** Flush the given prefix table to the bgpwatcher server
 *
 * @param table         pointer to prefix table to flush
 * @return 0 if the table was flushed successfully, -1 otherwise
 *
 * @note you may safely re-use the table after calling this function followed by
 * bgpwatcher_client_pfx_table_begin
 */
int bgpwatcher_client_pfx_table_end(bgpwatcher_client_pfx_table_t *table);

/** Create a re-usable peer table
 *
 * @param client        pointer to bgpwatcher client instance to associate the
 *                      table with
 * @return pointer to a bgpwatcher peer table instance if successful, NULL
 * otherwise
 */
bgpwatcher_client_peer_table_t *bgpwatcher_client_peer_table_create(
						   bgpwatcher_client_t *client);

/** Free a peer table
 *
 * @param table         pointer to the peer table to free
 */
void bgpwatcher_client_peer_table_free(bgpwatcher_client_peer_table_t **table);

/** Set the time that a table represents
 *
 * @param table           pointer to an initialized table object
 * @param collector_name  collector that this table refers to
 * @param time            new time to set for the table
 * @return a unique message id used for asynchronous replies. this number can
 * (optionally) be matched against those received in the handle_reply callbacks
 */
int bgpwatcher_client_peer_table_begin(bgpwatcher_client_peer_table_t *table,
                                       char *collector_name,
                                       uint32_t time);

/** Add a peer record to the given peer table
 *
 * @param table         pointer to peer table to add peer record to
 * @param peer_ip       pointer to the peer ip
 * @param status        status value
 * @return 0 if the peer was added successfully, -1 otherwise
 *
 * @note the caller maintains ownership of the peer record
 */
int bgpwatcher_client_peer_table_add(bgpwatcher_client_peer_table_t *table,
                                     bgpstream_ip_address_t *peer_ip,
                                     uint8_t status);

/** Flush the given peer table to the bgpwatcher server
 *
 * @param table         pointer to peer table to flush
 * @return 0 if the table was flushed successfully, -1 otherwise
 *
 * @note you may safely re-use the table after calling this function followed by
 * bgpwatcher_client_pfx_table_begin
 */
int bgpwatcher_client_peer_table_end(bgpwatcher_client_peer_table_t *table);

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

/** Set the amount of time to wait for outstanding requests on shutdown
 *
 * @param client        pointer to a bgpwatcher client instance to update
 * @param linger        time in ms to wait for outstanding requests
 *
 * @note defaults to BGPWATCHER_CLIENT_SHUTDOWN_LINGER_DEFAULT
 */
void bgpwatcher_client_set_shutdown_linger(bgpwatcher_client_t *client,
					   uint64_t linger);

/** Set timeout for a single request
 *
 * @param client        pointer to a client instance to update
 * @param timeout_ms    time in msec before request is retried
 *
 * @note defaults to BGPWATCHER_CLIENT_REQUEST_TIMEOUT_DEFAULT
 */
void bgpwatcher_client_set_request_timeout(bgpwatcher_client_t *client,
					   uint64_t timeout_ms);

/** Set the number of retries before a request is abandoned
 *
 * @param client        pointer to a client instance to update
 * @param retry_cnt     number of times to retry a request before giving up
 *
 * @note defaults to BGPWATCHER_CLIENT_REQUEST_RETRIES_DEFAULT
 */
void bgpwatcher_client_set_request_retries(bgpwatcher_client_t *client,
					   int retry_cnt);

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
