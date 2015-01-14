/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
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

#ifndef __BGPWATCHER_VIEW_INT_H
#define __BGPWATCHER_VIEW_INT_H

#include "config.h" // needed for time header detection

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "khash.h"
#include "utils.h"

#include "bl_bgp_utils.h"

#include "bgpwatcher_view.h"
#include "bgpwatcher_common.h"

/************ map from peer -> prefix info ************/

KHASH_INIT(bwv_peerid_pfxinfo, bl_peerid_t, bgpwatcher_pfx_peer_info_t, 1,
	   kh_int_hash_func, kh_int_hash_equal)

/** Value for a prefix in the v4pfxs and v6pfxs tables */
typedef struct bwv_peerid_pfxinfo {

  /** hash {peerid} -> pfx_peer_info */
  khash_t(bwv_peerid_pfxinfo) *peers;

  /** The number of peers in the peers hash that are actually valid */
  uint16_t peers_cnt;

} bwv_peerid_pfxinfo_t;
// TODO: add documentation



/************ map from prefix -> peers [-> prefix info] ************/

KHASH_INIT(bwv_v4pfx_peerid_pfxinfo, bl_ipv4_pfx_t, bwv_peerid_pfxinfo_t *, 1,
	   bl_ipv4_pfx_hash_func, bl_ipv4_pfx_hash_equal)
typedef khash_t(bwv_v4pfx_peerid_pfxinfo) bwv_v4pfx_peerid_pfxinfo_t;

KHASH_INIT(bwv_v6pfx_peerid_pfxinfo, bl_ipv6_pfx_t, bwv_peerid_pfxinfo_t *, 1,
	   bl_ipv6_pfx_hash_func, bl_ipv6_pfx_hash_equal)
typedef khash_t(bwv_v6pfx_peerid_pfxinfo) bwv_v6pfx_peerid_pfxinfo_t;



/***** map from peerid to peerinfo *****/

/** Additional per-peer info */
typedef struct bwv_peerinfo {

  /** The ID of this peer */
  bl_peerid_t id;

  /** The number of v4 prefixes that this peer observed */
  uint32_t v4_pfx_cnt;

  /** The number of v6 prefixes that this peer observed */
  uint32_t v6_pfx_cnt;

} bwv_peerinfo_t;

KHASH_INIT(bwv_peerid_peerinfo, bl_peerid_t, bwv_peerinfo_t, 1,
           kh_int_hash_func, kh_int_hash_equal)


/************ bgpview ************/

// TODO: documentation
struct bgpwatcher_view {

  /** BGP Time that the view represents */
  uint32_t time;

  /** Wall time when the view was created */
  struct timeval time_created;

  /** Table of prefix info for v4 prefixes */
  bwv_v4pfx_peerid_pfxinfo_t *v4pfxs;

  /** The number of in-use v4pfxs */
  uint32_t v4pfxs_cnt;

  /** Table of prefix info for v6 prefixes */
  bwv_v6pfx_peerid_pfxinfo_t *v6pfxs;

  /** The number of in-use v6pfxs */
  uint32_t v6pfxs_cnt;

  /** Table of peerid -> peersign */
  bl_peersign_map_t *peersigns;

  /** Is the peersigns table shared? */
  int peersigns_shared;

  /** Table of peerid -> peerinfo */
  kh_bwv_peerid_peerinfo_t *peerinfo;

  /** The number of times this view has been published since it was cleared */
  int pub_cnt;

};

/** Add a prefix to a view
 *
 * @param view          view to add prefix to
 * @param prefix        borrowed pointer to prefix to add
 * @param peerid        id of peer to add info for
 * @param pfx_info      prefix info to add for given peer/prefix
 * @param cache         pass a pointer to NULL on the first call, and then
 *                      re-use the pointer to successive calls that use the
 *                      same prefix to improve performance
 * @return 0 if successful, -1 otherwise
 */
int bgpwatcher_view_add_prefix(bgpwatcher_view_t *view,
                               bl_pfx_storage_t *prefix,
                               bl_peerid_t peerid,
                               bgpwatcher_pfx_peer_info_t *pfx_info,
			       void **cache);

/** Send the given view to the given socket
 *
 * @param dest          socket to send the prefix to
 * @param view          pointer to the view to send
 * @return 0 if the view was sent successfully, -1 otherwise
 */
int bgpwatcher_view_send(void *dest, bgpwatcher_view_t *view);

/** Receive a view from the given socket
 *
 * @param src           socket to receive on
 * @param view          pointer to the clear/new view to receive into
 * @return pointer to the view instance received, NULL if an error occurred.
 */
int bgpwatcher_view_recv(void *src, bgpwatcher_view_t *view);

#endif /* __BGPWATCHER_VIEW_INT_H */
