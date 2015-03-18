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

#ifndef __BGPWATCHER_CLIENT_H
#define __BGPWATCHER_CLIENT_H

#include <stdint.h>

#include "bgpwatcher_view.h"
#include "bgpwatcher_common.h"

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

/** Default URI for the server -> client pub/sub connection */
#define BGPWATCHER_CLIENT_SERVER_SUB_URI_DEFAULT "tcp://127.0.0.1:6301"

/** Default time that the client will wait for outstanding messages when
    shutting down (in milliseconds) */
#define BGPWATCHER_CLIENT_SHUTDOWN_LINGER_DEFAULT 600000

/** Default request timeout */
#define BGPWATCHER_CLIENT_REQUEST_TIMEOUT_DEFAULT 300000

/** Default request retry count  */
#define BGPWATCHER_CLIENT_REQUEST_RETRIES_DEFAULT 3

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

typedef struct bgpwatcher_client bgpwatcher_client_t;

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

typedef enum {
  BGPWATCHER_CLIENT_RECV_MODE_NONBLOCK = 0,
  BGPWATCHER_CLIENT_RECV_MODE_BLOCK    = 1,
} bgpwatcher_client_recv_mode_t;

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

/** Queue the given View for transmission to the server
 *
 * @param client        pointer to a bgpwatcher client instance
 * @param view          pointer to the view to transmit
 * @return 0 if the view was transmitted successfully, -1 otherwise
 *
 * This function only sends 'active' fields. Any fields that are 'inactive' in
 * the view **will not** be present in the view received by the server.
 *
 * @note The actual transmission may happen asynchronously, so a return from
 * this function simply means that the view was queued for transmission.
 */
int bgpwatcher_client_send_view(bgpwatcher_client_t *client,
                                bgpwatcher_view_t *view);

/** Attempt to receive an BGP View from the bgpwatcher server
 *
 * @param client        pointer to the client instance to receive from
 * @param mode          receive mode (blocking/non-blocking)
 * @param[out] interests  set to all the interests the view satisfies
 * @param view          pointer to the view to fill
 * @return all the interests the view satisfies, -1 if an error occurred.
 *
 * @note this function will only receive messages for which an interest was set
 * when initializing the client, but a view may satisfy *more* interests than
 * were explicitly asked for. For example, when subscribing to PARTIAL tables, a
 * table that is marked as PARTIAL could also be marked as FIRSTFULL (if it also
 * satisfies that interest).
 *
 * The view provided to this function must have been created using
 * bgpwatcher_view_create, and if it is being re-used, it *must* have been
 * cleared using bgpwatcher_view_clear.
 */
int bgpwatcher_client_recv_view(bgpwatcher_client_t *client,
				bgpwatcher_client_recv_mode_t blocking,
				bgpwatcher_view_t *view);

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

/** Set the URI for the client to subscribe to server table messages on
 *
 * @param client        pointer to a bgpwatcher client instance to update
 * @param uri           pointer to a uri string
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_client_set_server_sub_uri(bgpwatcher_client_t *client,
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
