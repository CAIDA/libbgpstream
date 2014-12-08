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

#ifndef __BGPWATCHER_SERVER_H
#define __BGPWATCHER_SERVER_H

#include <czmq.h>
#include <stdint.h>

#include <bgpwatcher_common_int.h>

#include "khash.h"

/** @file
 *
 * @brief Header file that exposes the (protected) interface of the bgpwatcher
 * server. This interface is only used by bgpwatcher
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/* shared constants are in bgpwatcher_common.h */

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** @} */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

typedef struct bgpwatcher_server bgpwatcher_server_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** Public information about a client given to bgpwatcher when a client connects
 *  or disconnects
 */
typedef struct bgpwatcher_server_client_info {
  /** Client name */
  char *name;

  /** Consumer Interests (bgpwatcher_consumer_interest_t flags) */
  uint8_t interests;

  /** Producer Intents (bgpwatcher_consumer_interest_t flags) */
  uint8_t intents;

} bgpwatcher_server_client_info_t;

/** Protected information about a client used to handle client connections */
typedef struct bgpwatcher_server_client {

  /** Identity frame data that the client sent us */
  zmq_msg_t identity;

  /** Printable ID of client (for debugging and logging) */
  char *id;

  /** Time at which the client expires */
  uint64_t expiry;

  /** info about this client that we will send to the client connect handler */
  bgpwatcher_server_client_info_t info;

  /** Current prefix table */
  bgpwatcher_pfx_table_t pfx_table;

  /** Indicates if a table_begin message has been received for the pfx table */
  int pfx_table_started;

} bgpwatcher_server_client_t;

typedef struct bgpwatcher_server_callbacks {

  /** Signals that a new client has connected
   *
   * @param server      pointer to the server instance originating the signal
   * @param client      pointer to a client info structure (server owns this)
   * @param user        pointer to user data given at init time
   * @return 0 if signal successfully handled, -1 otherwise
   */
  int (*client_connect)(bgpwatcher_server_t *server,
			bgpwatcher_server_client_info_t *client,
			void *user);

  /** Signals that a client has disconnected or timed out
   *
   * @param server      pointer to the server instance originating the signal
   * @param client      pointer to a client info structure (server owns this)
   * @param user        pointer to user data given at init time
   * @return 0 if signal successfully handled, -1 otherwise
   */
  int (*client_disconnect)(bgpwatcher_server_t *server,
			   bgpwatcher_server_client_info_t *client,
			   void *user);

  /** Signals that a new prefix table is starting
   *
   * @param server      pointer to the server instance originating the signal
   * @param client      pointer to a client info structure (server owns this)
   * @param table       pointer to a completed prefix table structure
   * @param user        pointer to user data given at init time
   * @return 0 if signal successfully handled, -1 otherwise
   */
  int (*table_begin_prefix)(bgpwatcher_server_t *server,
                            bgpwatcher_server_client_info_t *client,
                            bgpwatcher_pfx_table_t *table,
                            void *user);

  /** Signals that a prefix row has been received
   *
   * @param server      pointer to the server instance originating the signal
   * @param client      pointer to a client info structure (server owns this)
   * @param table       pointer to the table for this record
   * @param row         pointer to a prefix row structure (server owns this)
   * @param user        pointer to user data given at init time
   * @return 0 if signal successfully handled, -1 otherwise
   */
  int (*prefix_row)(bgpwatcher_server_t *server,
                    bgpwatcher_server_client_info_t *client,
                    bgpwatcher_pfx_table_t *table,
                    bgpwatcher_pfx_row_t *row,
                    void *user);

  /** Signals that a prefix table is ending
   *
   * @param server      pointer to the server instance originating the signal
   * @param client      pointer to a client info structure (server owns this)
   * @param table       pointer to a completed prefix table structure
   * @param user        pointer to user data given at init time
   * @return 0 if signal successfully handled, -1 otherwise
   */
  int (*table_end_prefix)(bgpwatcher_server_t *server,
                          bgpwatcher_server_client_info_t *client,
                          bgpwatcher_pfx_table_t *table,
                          void *user);

  /** User data passed along with each callback */
  void *user;

} bgpwatcher_server_callbacks_t;

KHASH_INIT(strclient, char*, bgpwatcher_server_client_t*, 1,
	   kh_str_hash_func, kh_str_hash_equal);

struct bgpwatcher_server {

  /** Error status */
  bgpwatcher_err_t err;

  /** 0MQ context pointer */
  zctx_t *ctx;

  /** URI to listen for clients on */
  char *client_uri;

  /** URI to pub tables on */
  char *client_pub_uri;

  /** Socket to bind to for client connections */
  void *client_socket;

  /** Socket to pub tables on */
  void *client_pub_socket;

  /** List of clients that are connected */
  khash_t(strclient) *clients;

  /** Time (in ms) between heartbeats sent to clients */
  uint64_t heartbeat_interval;

  /** Time (in ms) to send the next heartbeat to clients */
  uint64_t heartbeat_next;

  /** The number of heartbeats that can go by before a client is declared
      dead */
  int heartbeat_liveness;

  /** Indicates that the server should shutdown at the next opportunity */
  int shutdown;

  /* Functions to call when we get a message from a client */
  bgpwatcher_server_callbacks_t *callbacks;

  /** Next table number */
  uint64_t table_num;

};

/** @} */

/** Initialize a new BGP Watcher server instance
 *
 * @param cb_p     double-pointer to a dynamically allocated bgpwatcher
 *                      server callback structure
 * @return a pointer to a bgpwatcher server instance if successful, NULL if an
 * error occurred.
 *
 * @note the caller no longer owns the memory passed, and the caller's pointer
 * will be nullified to reflect this.
 */
bgpwatcher_server_t *bgpwatcher_server_init(
				      bgpwatcher_server_callbacks_t **cb_p);

/** Start the given bgpwatcher server instance
 *
 * @param server       pointer to a bgpwatcher server instance to start
 * @return 0 if the server started successfully, -1 otherwise.
 *
 * This function will block and run until the server is stopped. Control will
 * return to the calling library only by way of the callback functions specified
 * in the call to bgpwatcher_server_init.
 */
int bgpwatcher_server_start(bgpwatcher_server_t *server);

/** Prints the error status (if any) to standard error and clears the error
 * state
 *
 * @param server       pointer to bgpwatcher server instance to print error for
 */
void bgpwatcher_server_perr(bgpwatcher_server_t *server);

/** Stop the given bgpwatcher server instance at the next safe occasion.
 *
 * This is useful to initiate a clean shutdown if you are handling signals in
 * bgpwatcher. Call this from within your signal handler. It should also be
 * called from bgpwatcher_stop to pass the signal through.
 *
 * @param watcher       pointer to the bgpwatcher instance to stop
 */
void bgpwatcher_server_stop(bgpwatcher_server_t *server);

/** Free the given bgpwatcher server instance
 *
 * @param server       pointer to the bgpwatcher server instance to free
 */
void bgpwatcher_server_free(bgpwatcher_server_t *server);

/** Set the URI for the server to listen for client connections on
 *
 * @param server        pointer to a bgpwatcher server instance to update
 * @param uri           pointer to a uri string
 * @return 0 if the uri was set successfully, -1 otherwise
 *
 * @note defaults to BGPWATCHER_CLIENT_URI_DEFAULT
 */
int bgpwatcher_server_set_client_uri(bgpwatcher_server_t *server,
				      const char *uri);

/** Set the URI for the server to publish tables on
 *  (subscribed to by consumer clients)
 *
 * @param server        pointer to a bgpwatcher server instance to update
 * @param uri           pointer to a uri string
 * @return 0 if the uri was set successfully, -1 otherwise
 *
 * @note defaults to BGPWATCHER_CLIENT_PUB_URI_DEFAULT
 */
int bgpwatcher_server_set_client_pub_uri(bgpwatcher_server_t *server,
                                         const char *uri);

/** Set the heartbeat interval
 *
 * @param server        pointer to a bgpwatcher server instance to update
 * @param interval_ms   time in ms between heartbeats
 *
 * @note defaults to BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT
 */
void bgpwatcher_server_set_heartbeat_interval(bgpwatcher_server_t *server,
					      uint64_t interval_ms);

/** Set the heartbeat liveness
 *
 * @param server        pointer to a bgpwatcher server instance to update
 * @param beats         number of heartbeats that can go by before a server is
 *                      declared dead
 *
 * @note defaults to BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT
 */
void bgpwatcher_server_set_heartbeat_liveness(bgpwatcher_server_t *server,
					      int beats);


/**
 * @name Server Publish Functions
 *
 * @{ */

/** Publish the given BGP View to any interested consumers
 *
 * @param server        pointer to the bgpwatcher server instance
 * @param table         pointer to a bgp view to publish
 * @param interests     flags indicating which interests this table satisfies
 */
int bgpwatcher_server_publish_view(bgpwatcher_server_t *server,
                                   bgpwatcher_view_t *view,
                                   int interests);

/** @} */

#endif
