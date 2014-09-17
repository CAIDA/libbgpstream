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

/** @} */

/**
 * @name Public Enums
 *
 * @{ */


/** @} */

#endif
