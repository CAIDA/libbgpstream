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

#ifndef __BGPWATCHER_COMMON_H
#define __BGPWATCHER_COMMON_H

#include <stdint.h>

#include <bgpstream_utils_addr.h>

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

/** Default the server/client heartbeat interval to 2000 msec */
#define BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT 2000

/** Default the server/client heartbeat liveness to 450 beats (15min) */
#define BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT 450

/** Default the client reconnect minimum interval to 1 second */
#define BGPWATCHER_RECONNECT_INTERVAL_MIN 1000

/** Default the client reconnect maximum interval to 32 seconds */
#define BGPWATCHER_RECONNECT_INTERVAL_MAX 32000

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
  bgpstream_addr_storage_t ip;

  /** Peer status */
  uint8_t status;

  /** Internal store info */
  uint16_t server_id;

  /** Internal store info */
  void *ap_status;

} bgpwatcher_peer_t;


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
  bgpwatcher_peer_t *peers;

  /** Number of peers referenced in this table */
  int peers_cnt;

  /** Number of peers allocated in this table */
  int peers_alloc_cnt;

  /** Internal store state */
  void *sview;

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
  BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL    = 0b001,
  BGPWATCHER_CONSUMER_INTEREST_FULL         = 0b010,
  BGPWATCHER_CONSUMER_INTEREST_PARTIAL      = 0b100,
} bgpwatcher_consumer_interest_t;

/* Consumer subscription strings.
 *
 * 0MQ subscriptions are simply a prefix match on the first message part. We can
 * leverage this to get hierarchical subscriptions (i.e. the most general
 * subscription should be the shortest, and all others should contain the
 * subscription of their parent. Clear as mud?
 */
#define BGPWATCHER_CONSUMER_INTEREST_SUB_PARTIAL "P"

#define BGPWATCHER_CONSUMER_INTEREST_SUB_FULL    \
  BGPWATCHER_CONSUMER_INTEREST_SUB_PARTIAL"F"

#define BGPWATCHER_CONSUMER_INTEREST_SUB_FIRSTFULL \
  BGPWATCHER_CONSUMER_INTEREST_SUB_FULL"1"

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

  /** store error */
  BGPWATCHER_ERR_STORE        = -7,

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

/** Dump the given interests to stdout in a human-readable format
 *
 * @param interests     set of interests
 */
void bgpwatcher_consumer_interest_dump(int interests);

#endif
