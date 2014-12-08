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

#ifndef __BGPWATCHER_COMMON_H
#define __BGPWATCHER_COMMON_H

#include <stdint.h>

#include <bl_bgp_utils.h>

/** @file
 *
 * @brief Header file that exposes the public structures used by both
 * bgpwatcher_client and bgpwatcher_server
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Constants
 *
 * @{ */

/** Default URI for the server to listen for client requests on */
#define BGPWATCHER_CLIENT_URI_DEFAULT "tcp://*:6300"

/** Default URI for the server to publish tables on (subscribed to by consumer
    clients) */
#define BGPWATCHER_CLIENT_PUB_URI_DEFAULT "tcp://*:6301"

/** Default the server/client heartbeat interval to 100 msec */
#define BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT 1000

/** Default the server/client heartbeat liveness to 120 beats */
#define BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT 120

/** Default the client reconnect minimum interval to 1 second */
#define BGPWATCHER_RECONNECT_INTERVAL_MIN 1000

/** Default the client reconnect maximum interval to 32 seconds */
#define BGPWATCHER_RECONNECT_INTERVAL_MAX 32000

#define BGPWATCHER_PEER_MAX_CNT 64

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

/** Type of a sequence number */
typedef uint32_t seq_num_t;

/** Information about a peer */
typedef struct bgpwatcher_peer {

  /** Peer IP address */
  bl_addr_storage_t ip;

  /** Peer status */
  uint8_t status;

  /** User-specific peer id (set by server) */
  uint16_t server_id;

} bgpwatcher_peer_t;

/** Information about a prefix from a peer */
typedef struct bgpwatcher_pfx_peer_info {

  /** Origin ASN */
  uint32_t orig_asn;

  /** @todo add other pfx info fields here (AS path, etc) */

  /** If set, this prefix is seen by this peer */
  uint8_t in_use;

} bgpwatcher_pfx_peer_info_t;

/** Information about a prefix row */
typedef struct bgpwatcher_pfx_row {

  /** Prefix */
  bl_pfx_storage_t prefix;

  /** Per-Peer Information
   * @note index in this array corresponds to index in table.peers array */
  bgpwatcher_pfx_peer_info_t info[BGPWATCHER_PEER_MAX_CNT];

} bgpwatcher_pfx_row_t;

/** Information about the a prefix table */
typedef struct bgpwatcher_pfx_table {

  /** Generated table ID (server-global) */
  uint64_t id;

  /** Time that the table represents */
  uint32_t time;

  /** Collector that the table corresponds to */
  char *collector;

  /** Number of prefixes in the table */
  uint32_t prefix_cnt;

  /** Peers that the table contains information for */
  bgpwatcher_peer_t peers[BGPWATCHER_PEER_MAX_CNT];

  /** Number of peers referenced in this table */
  int peers_cnt;

} bgpwatcher_pfx_table_t;

/** bgpwatcher error information */
typedef struct bgpwatcher_err {
  /** Error code */
  int err_num;

  /** String representation of the error that occurred */
  char problem[255];
} bgpwatcher_err_t;

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** Consumer interests
 *
 * A consumer has interests: it interested in being sent notifications about
 * something. E.g. a new prefix table being available.
 */
typedef enum {

  /** Current status of bgpview */
  BGPWATCHER_CONSUMER_INTEREST_BGPVIEWSTATUS = 0x01,
  BGPWATCHER_CONSUMER_INTEREST_ASVISIBILITY  = 0x02,
} bgpwatcher_consumer_interest_t;

/** @todo add more generic consumer filters so that a consumer can filter
    interests based on more than client ID */

/** Producer Intents
 *
 * A producer has intents: it intends to send messages about something. E.g. a
 * new prefix table.
 */
typedef enum {

  /** Prefix Table */
  BGPWATCHER_PRODUCER_INTENT_PREFIX = 0x01,

} bgpwatcher_producer_intent_t;


/** Enumeration of error codes
 *
 * @note these error codes MUST be <= 0
 */
typedef enum {

  /** No error has occured */
  BGPWATCHER_ERR_NONE         = 0,

  /** bgpwatcher failed to initialize */
  BGPWATCHER_ERR_INIT_FAILED  = -1,

  /** bgpwatcher failed to start */
  BGPWATCHER_ERR_START_FAILED = -2,

  /** bgpwatcher was interrupted */
  BGPWATCHER_ERR_INTERRUPT    = -3,

  /** unhandled error */
  BGPWATCHER_ERR_UNHANDLED    = -4,

  /** protocol error */
  BGPWATCHER_ERR_PROTOCOL     = -5,

  /** malloc error */
  BGPWATCHER_ERR_MALLOC       = -6,

} bgpwatcher_err_code_t;

/** @} */

/** Set an error state on the given watcher instance
 *
 * @param err           pointer to an error status instance to set the error on
 * @param errcode       error code to set (> 0 indicates errno)
 * @param msg...        string message to set
 */
void bgpwatcher_err_set_err(bgpwatcher_err_t *err, int errcode,
			const char *msg, ...);

/** Check if the given error status instance has an error set
 *
 * @param err           pointer to an error status instance to check for error
 * @return 0 if there is no error, 1 otherwise
 */
int bgpwatcher_err_is_err(bgpwatcher_err_t *err);

/** Prints the error status (if any) to standard error and clears the error
 * state
 *
 * @param err       pointer to bgpwatcher error status instance
 */
void bgpwatcher_err_perr(bgpwatcher_err_t *err);

#endif
