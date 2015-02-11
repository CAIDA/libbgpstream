/*
 * bgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpstream.
 *
 * bgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef __BGPSTREAM_UTILS_PEER_SIG_MAP_H
#define __BGPSTREAM_UTILS_PEER_SIG_MAP_H

#include <bgpstream_utils.h>      /** < for COLLECTOR_NAME_LEN */
#include <bgpstream_utils_addr.h>

/** @file
 *
 * @brief Header file that exposes the public interface of the BGP Stream Peer
 * Signature Map.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Opaque Data Structures
 *
 * @{ */

/** Type of a peer ID */
typedef uint16_t bgpstream_peer_id_t;

/** Opaque structure containing a peer signature map instance */
typedef struct bgpstream_peer_sig_map bgpstream_peer_sig_map_t;

/** @} */


/**
 * @name Public Data Structures
 *
 * @{ */

/** Structure that uniquely identifies a single peer. */
typedef struct struct_bgpstream_peer_sig_t {

  /** The string name of the collector that this peer belongs to */
  char collector_str[BGPSTREAM_UTILS_COLLECTOR_NAME_LEN];

  /** The IP address of this peer */
  bgpstream_addr_storage_t peer_ip_addr;

} bgpstream_peer_sig_t;

/** @} */



/**
 * @name Public API Functions
 *
 * @{ */


/** Create a new peer signature map
 *
 * @return a pointer to the created peer sig map if successful, NULL otherwise.
 */
bgpstream_peer_sig_map_t *bgpstream_peer_sig_map_create();

/** Get (or set and get) the peer ID for the given peer signature
 *
 * @param map            pointer to the peer sig map to query
 * @param collector_str  string name of the collector
 * @param peer_ip_addr   pointer to the IP address of the peer
 * @return the peer ID for this peer signature, 0 if an error occurred
 *
 * @todo add peer ASN
 */
bgpstream_peer_id_t bgpstream_peer_sig_map_get_id(
                                        bgpstream_peer_sig_map_t *map,
                                        char *collector_str,
					bgpstream_addr_storage_t *peer_ip_addr);

/** Get the peer signature for the given peer ID
 *
 * @param map           pointer to the peer sig map to query
 * @param peer_id       peer ID to retrieve signature for
 * @return pointer to the peer signature for the given peer ID, NULL if it was
 * not found
 */
bgpstream_peer_sig_t *bgpstream_peer_sig_map_get_sig(
                                                  bgpstream_peer_sig_map_t *map,
                                                  bgpstream_peer_id_t peer_id);

/** Get the number of peer signatures in the given map
 *
 * @param map           pointer to the peer sig map to find the size of
 * @return the number of peer signatures in the given map
 */
int bgpstream_peer_sig_map_get_size(bgpstream_peer_sig_map_t *map);

/** Destroy the given peer signature map
 *
 * @param map           pointer to the peer sig map to destroy
 */
void bgpstream_peer_sig_map_destroy(bgpstream_peer_sig_map_t *map);

/** Empty the given peer signature map
 *
 * @param map           peer sig map
 */
void bgpstream_peer_sig_map_clear(bgpstream_peer_sig_map_t *map);

/** @} */

#endif /* __BGPSTREAM_UTILS_PEER_SIG_MAP_H */

