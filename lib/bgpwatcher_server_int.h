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

#ifndef __BGPWATCHER_SERVER_INT_H
#define __BGPWATCHER_SERVER_INT_H

#include <czmq.h>
#include <stdint.h>

#include "bgpwatcher_server.h"
#include "bgpwatcher_view.h"
#include "bgpwatcher_common_int.h"
#include "bgpwatcher_store.h"

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

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

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

  /** Array of peer infos used by prefix rx */
  bgpwatcher_pfx_peer_info_t *peer_infos;

  /** Number of elements allocated in the peer infos array */
  int peer_infos_alloc_cnt;

  /** Indicates if a table_begin message has been received for the pfx table */
  int pfx_table_started;

} bgpwatcher_server_client_t;

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

  /** Next table number */
  uint64_t table_num;

  /** BGP Watcher Store instance */
  bgpwatcher_store_t *store;

  /** The number of heartbeats that have gone by since the last timeout check */
  int store_timeout_cnt;

};

/** @} */

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
