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

#ifndef __BGPWATCHER_SERVER_H
#define __BGPWATCHER_SERVER_H

#include <czmq.h>
#include <stdint.h>

#include "bgpwatcher_common.h"

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
#define BGPWATCHER_SERVER_WINDOW_LEN 6

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

/** Set bgpwatcher prefix metric
 *
 * @param server        pointer to a bgpwatcher server instance 
 * @param metric_prefix string that represents the prefix to prepend to metrics
 */
void bgpwatcher_server_set_metric_prefix(bgpwatcher_server_t *server, char *metric_prefix);

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
