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

#include <bgpwatcher_common.h>

/** @file
 *
 * @brief Header file that exposes the public interface of the bgpwatcher
 * server.
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/* shared constants are in bgpwatcher_common.h */

/** The default number of views in the window */
#define BGPWATCHER_SERVER_WINDOW_LEN 30

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

/** @} */

/** Initialize a new BGP Watcher server instance
 *
 * @return a pointer to a bgpwatcher server instance if successful, NULL if an
 * error occurred.
 */
bgpwatcher_server_t *bgpwatcher_server_init();

/** Start the given bgpwatcher server instance
 *
 * @param server       pointer to a bgpwatcher server instance to start
 * @return 0 if the server started successfully, -1 otherwise.
 *
 * This function will block and run until the server is stopped.
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

/** Set the size of the view window
 *
 * @param               pointer to a bgpwatcher server instance to configure
 * @param               length of the view window (in number of views)
 */
void bgpwatcher_server_set_window_len(bgpwatcher_server_t *server,
				      int window_len);

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


#endif
