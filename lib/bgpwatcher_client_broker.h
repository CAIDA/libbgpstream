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

#ifndef __BGPWATCHER_CLIENT_BROKER_H
#define __BGPWATCHER_CLIENT_BROKER_H

#include <czmq.h>
#include <stdint.h>

#include <khash.h>

#include <bgpwatcher_common_int.h>

/** @file
 *
 * @brief Header file that exposes the private interface of the bgpwatcher
 * client broker
 *
 * @author Alistair King
 *
 */

/** The maximum number of requests that we allow to be outstanding at any time */
#define MAX_OUTSTANDING_REQ 100000

#define BGPWATCHER_CLIENT_BROKER_REQ_MSG_FRAMES_MAX 10

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

/** Collection of asynchronous callbacks used to notify the client of incoming
    messages from the server. */
typedef struct bgpwatcher_client_broker_callbacks {

  bgpwatcher_client_cb_handle_reply_t *handle_reply;

  /** @todo add other signals from server here (table rx, etc) */

  /** pointer to user-provided data */
  void *user;

} bgpwatcher_client_broker_callbacks_t;

/** Holds information about a single outstanding request sent to the server */
typedef struct bgpwatcher_client_broker_req {

  /** The sequence number in the request (used to match replies) */
  seq_num_t seq_num;

  /** Message type in the request (and reply) */
  bgpwatcher_msg_type_t msg_type;

  /** The time that this request should next be retried */
  uint64_t retry_at;

  /** The number of retries that remain */
  uint8_t retries_remaining;

  /** Message to send to the server */
  zmq_msg_t msg_frames[BGPWATCHER_CLIENT_BROKER_REQ_MSG_FRAMES_MAX];

  /** Number of used msg frames */
  int msg_frames_cnt;

} bgpwatcher_client_broker_req_t;

/** Config for the broker. Populated by the client */
typedef struct bgpwatcher_client_broker_config {

  /** set of bgpwatcher_consumer_interest_t flags */
  uint8_t interests;

  /** set of bgpwatcher_producer_intent_t flags */
  uint8_t intents;

  /** Pointer to the master's state (passed to callbacks) */
  struct bgpwatcher_client *master;

  /** Client callbacks */
  bgpwatcher_client_broker_callbacks_t callbacks;

  /** 0MQ context pointer (for broker->server comms) */
  zctx_t *ctx;

  /** URI to connect to the server on */
  char *server_uri;

  /** Time (in ms) between heartbeats sent to the server */
  uint64_t heartbeat_interval;

  /** The number of heartbeats that can go by before the server is declared
      dead */
  int heartbeat_liveness;

  /** The minimum time (in ms) after a server disconnect before we try to
      reconnect */
  uint64_t reconnect_interval_min;

  /** The maximum time (in ms) after a server disconnect before we try to
      reconnect (after exponential back-off) */
  uint64_t reconnect_interval_max;

  /** The time that we will linger once a shutdown request has been received */
  uint64_t shutdown_linger;

  /** Request timeout in msec */
  uint64_t request_timeout;

  /** Request retries */
  int request_retries;

  /** Error status (needs to be here so the master to retrieve err status) */
  bgpwatcher_err_t err;

  /** Identity of this client. MUST be globally unique.  If this field is set
   * when the broker is started, it will be used to set the identity of the zmq
   * socket
   */
  char *identity;

} bgpwatcher_client_broker_config_t;


/** State for a broker instance */
typedef struct bgpwatcher_client_broker {

  /** Pointer to the config info that our master prepared for us (READ-ONLY) */
  bgpwatcher_client_broker_config_t *cfg;

  /** Pointer to the pipe used to talk to the master */
  zsock_t *master_pipe;

  /** Pointer to the resolved zmq socket used to talk to the master */
  void *master_zocket;

  /** Has the master pipe been removed from the reactor? */
  int master_removed;

  /** Socket used to connect to the server */
  void *server_socket;

  /** Ordered list of outstanding requests (used for re-transmits) */
  zlist_t *req_list;

  /** Time (in ms) to send the next heartbeat to server */
  uint64_t heartbeat_next;

  /** The number of beats before the server is declared dead */
  int heartbeat_liveness_remaining;

  /** The time before we will next attempt to reconnect */
  uint64_t reconnect_interval_next;

  /** Indicates the time that the broker must shut down by (calculated as
      $TERM.time + shutdown_linger) */
  uint64_t shutdown_time;

  /** Event loop */
  zloop_t *loop;

  /** Heartbeat timer ID */
  int timer_id;

} bgpwatcher_client_broker_t;

/** @} */

/** Main event loop of the client broker. Conforms to the zactor_fn spec.
 *
 * @param pipe          pointer to a pipe used to communicate with the client
 * @param args          pointer to a pre-populated client broker state struct
 *
 * @note all communication with the broker must be through the pipe. NO shared
 * memory is to be used.
 */
void bgpwatcher_client_broker_run(zsock_t *pipe, void *args);

/** Initialize a request instance
 *
 * @return pointer to a request instance if successful, NULL otherwise
 */
bgpwatcher_client_broker_req_t *bgpwatcher_client_broker_req_init();

/** Free a request instance
 *
 * @param req_p           double-pointer to a request instance
 */
void bgpwatcher_client_broker_req_free(bgpwatcher_client_broker_req_t **req_p);

#endif
