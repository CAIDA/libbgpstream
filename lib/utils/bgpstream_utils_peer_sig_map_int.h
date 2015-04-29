/*
 * This file is part of bgpstream
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


#ifndef __BGPSTREAM_PEER_SIG_MAP_INT_H
#define __BGPSTREAM_PEER_SIG_MAP_INT_H

#include "khash.h"

#include "bgpstream_utils_peer_sig_map.h"

/** @file
 *
 * @brief Header file that exposes the private interface of the BGP Stream Peer
 * Signature Map
 *
 * @author Chiara Orsini
 *
 * @note this interface MUST NOT be used. It will be removed in the next version
 * of BGP Stream.
 *
 */

/** Hash a peer signature into a 64bit number
 *
 * @param               the peer signature to hash
 * @return 64bit hash of the given peer signature
 */
khint64_t bgpstream_peer_sig_hash(bgpstream_peer_sig_t *ps);

/** Check if two peer signatures are equal
 *
 * @param ps1           pointer to the first peer signature
 * @param ps2           pointer to the second peer signature
 * @return 0 if the signatures are not equal, non-zero if they are equal
 */
int bgpstream_peer_sig_equal(bgpstream_peer_sig_t *ps1,
                             bgpstream_peer_sig_t *ps2);

/** Set the peer ID for the given collector/peer
 *
 * @param map           peer sig map to set peer ID for
 * @param peer_id       peer id to set
 * @param collector_str name of the collector the peer is associated with
 * @param peer_ip_addr  pointer to the peer IP address
 * @param peer_asnumber  AS number of the peer
 * @return 0 if the ID was associated successfully, -1 otherwise
 *
 * @note the peer sig map expects to be able to allocate IDs internally. This
 * function must be used with care.
 */
int bgpstream_peer_sig_map_set(bgpstream_peer_sig_map_t *map,
                               bgpstream_peer_id_t peer_id,
                               char *collector_str,
                               bgpstream_addr_storage_t *peer_ip_addr,
                               uint32_t peer_asnumber);


/** Map from peer signature to peer ID */
KHASH_INIT(bgpstream_peer_sig_id_map,
           bgpstream_peer_sig_t*,
           bgpstream_peer_id_t,
           1,
	   bgpstream_peer_sig_hash,
           bgpstream_peer_sig_equal);

/** Map from peer ID to signature */
KHASH_INIT(bgpstream_peer_id_sig_map,
           bgpstream_peer_id_t,
           bgpstream_peer_sig_t*,
           1,
	   kh_int_hash_func,
           kh_int_hash_equal);

/** Structure representing an instance of a Peer Signature Map */
struct bgpstream_peer_sig_map {
  khash_t(bgpstream_peer_sig_id_map) * ps_id;
  khash_t(bgpstream_peer_id_sig_map) * id_ps;
};


#endif /* __BGPSTREAM_PEER_SIG_MAP_INT_H */

