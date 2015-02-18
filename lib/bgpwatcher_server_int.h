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

  /** The number of views in the store */
  int store_window_len;

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
