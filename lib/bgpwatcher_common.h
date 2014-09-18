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
