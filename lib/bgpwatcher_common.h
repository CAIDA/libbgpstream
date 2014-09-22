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

#include <czmq.h>
#include <stdint.h>
#include <sys/socket.h>

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

/** Default the server/client heartbeat interval to 1 second */
#define BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT 1000

/** Default the server/client heartbeat liveness to 3 beats */
#define BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT 3

/** Default the client reconnect minimum interval to 1 second */
#define BGPWATCHER_RECONNECT_INTERVAL_MIN 1000

/** Default the client reconnect maximum interval to 32 seconds */
#define BGPWATCHER_RECONNECT_INTERVAL_MAX 32000

/* shared constants are in bgpwatcher_common.h */

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

/** State for a prefix row */
typedef struct bgpwatcher_pfx_record {

  /** IPv4 or IPv6 prefix */
  struct sockaddr_storage prefix;

  /** Prefix length */
  uint8_t prefix_len;

  /** IPv4 or IPv6 peer IP */
  struct sockaddr_storage peer_ip;

  /** Originating ASN */
  uint32_t orig_asn;

  /** Collector Name (string) */
  char *collector_name;

} bgpwatcher_pfx_record_t;

/** State for a peer row */
typedef struct bgpwatcher_peer_record {

  /** IPv4 or IPv6 peer IP */
  struct sockaddr_storage ip;

  /** Peer Status */
  uint8_t status;

} bgpwatcher_peer_record_t;

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

/** Table types */
typedef enum {

  /** Invalid table */
  BGPWATCHER_TABLE_TYPE_NONE = 0,

  /** Prefix table */
  BGPWATCHER_TABLE_TYPE_PREFIX = 1,

  /** Peer table */
  BGPWATCHER_TABLE_TYPE_PEER = 2,

  /** Highest table number in use */
  BGPWATCHER_TABLE_TYPE_MAX = BGPWATCHER_TABLE_TYPE_PEER,

} bgpwatcher_table_type_t;

#define bgpwatcher_table_type_size_t sizeof(uint8_t)

/** Enumeration of message types
 *
 * @note these will be cast to a uint8_t, so be sure that there are fewer than
 * 2^8 values
 */
typedef enum {
  /** Invalid message */
  BGPWATCHER_MSG_TYPE_UNKNOWN   = 0,

  /** Client is ready to send requests/Server is ready for requests */
  BGPWATCHER_MSG_TYPE_READY     = 1,

  /** Client is explicitly disconnecting (clean shutdown) */
  BGPWATCHER_MSG_TYPE_TERM      = 2,

  /** Server/Client is still alive */
  BGPWATCHER_MSG_TYPE_HEARTBEAT = 3,

  /** A request for the server to process */
  BGPWATCHER_MSG_TYPE_DATA   = 4,

  /** Server is sending a response to a client */
  BGPWATCHER_MSG_TYPE_REPLY     = 5,

  /** Highest message number in use */
  BGPWATCHER_MSG_TYPE_MAX      = BGPWATCHER_MSG_TYPE_REPLY,

} bgpwatcher_msg_type_t;

#define bgpwatcher_msg_type_size_t sizeof(uint8_t)

/** Enumeration of request message types
 *
 * @note these will be cast to a uint8_t, so be sure that there are fewer than
 * 2^8 values
 */
typedef enum {
  /** Invalid message */
  BGPWATCHER_DATA_MSG_TYPE_UNKNOWN   = 0,

  /** Client is beginning a new table */
  BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN = 1,

  /* Client has completed a table */
  BGPWATCHER_DATA_MSG_TYPE_TABLE_END = 2,

  /** Client is sending a prefix record */
  BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD  = 3,

  /** Client is sending a peer record */
  BGPWATCHER_DATA_MSG_TYPE_PEER_RECORD  = 4,

  /** Highest message number in use */
  BGPWATCHER_DATA_MSG_TYPE_MAX      = BGPWATCHER_DATA_MSG_TYPE_PEER_RECORD,
} bgpwatcher_data_msg_type_t;

#define bgpwatcher_data_msg_type_size_t sizeof(uint8_t)

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

/** Decodes the message type for the given message
 *
 * @param msg           zmsg object to inspect
 * @return the type of the message, or BGPWATCHER_MSG_TYPE_UNKNOWN if an error
 *         occurred
 *
 * This function will pop the type frame from the beginning of the message
 */
bgpwatcher_msg_type_t bgpwatcher_msg_type(zmsg_t *msg);

/** Decodes the request type for the given message
 *
 * @param msg           zmsg object to inspect
 * @return the type of the message, or BGPWATCHER_REQ_MSG_TYPE_UNKNOWN if an
 *         error occurred
 *
 * This function will pop the type frame from the beginning of the message
 */
bgpwatcher_data_msg_type_t bgpwatcher_data_msg_type(zmsg_t *msg);

/** Create a new prefix record
 *
 * @return pointer to a prefix record if successful, NULL otherwise
 */
bgpwatcher_pfx_record_t *bgpwatcher_pfx_record_init();

/** Free a prefix record
 *
 * @param pfx           pointer to prefix record to free
 */
void bgpwatcher_pfx_record_free(bgpwatcher_pfx_record_t **pfx_p);

/** Create a new pfx record from the given msg
 *
 * @param msg           pointer to a 0mq msg to extract the pfx from
 * @return pointer to a new pfx record if successful, NULL otherwise
 */
bgpwatcher_pfx_record_t *bgpwatcher_pfx_record_deserialize(zmsg_t *msg);

/** Create a new 0mq msg from the given pfx record
 *
 * @param pfx           pointer to a pfx record to serialize
 * @return pointer to a new zmsg if successful, NULL otherwise
 */
zmsg_t *bgpwatcher_pfx_record_serialize(bgpwatcher_pfx_record_t *pfx);

/** Dump the given prefix record to stderr
 *
 * @param pfx           pointer to a prefix record to dump
 */
void bgpwatcher_pfx_record_dump(bgpwatcher_pfx_record_t *pfx);

/** Create a new peer record
 *
 * @return pointer to a peer record if successful, NULL otherwise
 */
bgpwatcher_peer_record_t *bgpwatcher_peer_record_init();

/** Free a peer record
 *
 * @param peer           pointer to peer record to free
 */
void bgpwatcher_peer_record_free(bgpwatcher_peer_record_t **peer_p);

/** Create a new peer record from the given msg
 *
 * @param msg           pointer to a 0mq msg to extract the peer from
 * @return pointer to a new peer record if successful, NULL otherwise
 */
bgpwatcher_peer_record_t *bgpwatcher_peer_record_deserialize(zmsg_t *msg);

/** Create a new 0mq msg from the given peer record
 *
 * @param peer           pointer to a peer record to serialize
 * @return pointer to a new zmsg if successful, NULL otherwise
 */
zmsg_t *bgpwatcher_peer_record_serialize(bgpwatcher_peer_record_t *peer);

/** Dump the given peer record to stderr
 *
 * @param peer           pointer to a peer record to dump
 */
void bgpwatcher_peer_record_dump(bgpwatcher_peer_record_t *peer);

#endif
