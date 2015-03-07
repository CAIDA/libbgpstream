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


#include "bgpwatcher_view.h"
#include "bgpwatcher_common.h"
#include "bgpstream_utils_pfx.h"


/** Information about a prefix as seen from a peer */
typedef struct bgpwatcher_pfx_peer_info {

  /** Origin ASN */
  uint32_t orig_asn;

  /** @todo add other pfx info fields here (AS path, etc) */

  /** State of the per-pfx per-peer data 
   *  if ACTIVE the prefix is currently
   *  seen by a peer. */
  bgpwatcher_view_field_state_t state;

  /** Generic pointer to store per-pfx-per-peer information */
  void *user;

} __attribute__((packed)) bgpwatcher_pfx_peer_info_t;


/** Value for a prefix in the v4pfxs and v6pfxs tables */
typedef struct bwv_peerid_pfxinfo {

  /** Sparse list of peers, where idx is peerid */
  bgpwatcher_pfx_peer_info_t *peers;

  uint16_t peers_alloc_cnt;

  /** The number of peers in the peers list that are actually valid */
  uint16_t peers_cnt;

  /** State of the prefix, if ACTIVE 
   *  the prefix is currently seen
   *  by at least one peer.
   *  if active <==> peers_cnt >0
   */
  bgpwatcher_view_field_state_t state;
  
  /** Generic pointer to store per-pfx information on consumers */
  void *user;

} __attribute__((packed)) bwv_peerid_pfxinfo_t;


/** @todo: add documentation ? */



/************ map from prefix -> peers [-> prefix info] ************/

KHASH_INIT(bwv_v4pfx_peerid_pfxinfo, bgpstream_ipv4_pfx_t, bwv_peerid_pfxinfo_t *, 1,
	   bgpstream_ipv4_pfx_storage_hash_val, bgpstream_ipv4_pfx_storage_equal_val)
typedef khash_t(bwv_v4pfx_peerid_pfxinfo) bwv_v4pfx_peerid_pfxinfo_t;

KHASH_INIT(bwv_v6pfx_peerid_pfxinfo, bgpstream_ipv6_pfx_t, bwv_peerid_pfxinfo_t *, 1,
	   bgpstream_ipv6_pfx_storage_hash_val, bgpstream_ipv6_pfx_storage_equal_val)
typedef khash_t(bwv_v6pfx_peerid_pfxinfo) bwv_v6pfx_peerid_pfxinfo_t;



/***** map from peerid to peerinfo *****/

/** Additional per-peer info */
typedef struct bwv_peerinfo {

  /** The ID of this peer */
  bgpstream_peer_id_t id;

  /** The number of v4 prefixes that this peer observed */
  uint32_t v4_pfx_cnt;

  /** The number of v6 prefixes that this peer observed */
  uint32_t v6_pfx_cnt;

  /** If set, this peer contributed to the view.  */
  // uint8_t in_use;
  
  /** State of the peer, if the peer is active */
  bgpwatcher_view_field_state_t state;
  
  /** Generic pointer to store information related to the peer */
  void *user;
  
} bwv_peerinfo_t;

KHASH_INIT(bwv_peerid_peerinfo, bgpstream_peer_id_t, bwv_peerinfo_t, 1,
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
  bgpstream_peer_sig_map_t *peersigns;

  /** Is the peersigns table shared? */
  int peersigns_shared;

  /** Table of peerid -> peerinfo */
  /** todo*/ 
  kh_bwv_peerid_peerinfo_t *peerinfo; 

  /** The number of in-use peers */
  uint32_t peerinfo_cnt;

  /** The number of times this view has been published since it was cleared */
  int pub_cnt;

  /** Pointer to a function that destroys the user structure
   *  in the bgpwatcher_view_t structure */
  bgpwatcher_view_destroy_user_t *user_destructor;

  /** Pointer to a function that destroys the user structure
   *  in the bwv_peerinfo_t structure */
  bgpwatcher_view_destroy_user_t *peer_user_destructor;

  /** Pointer to a function that destroys the user structure
   *  in the bwv_peerid_pfxinfo_t structure */
  bgpwatcher_view_destroy_user_t *pfx_user_destructor;
  
  /** Pointer to a function that destroys the user structure
   *  in the bgpwatcher_pfx_peer_info_t structure */
  bgpwatcher_view_destroy_user_t *pfx_peer_user_destructor;

  /** State of the view */
  bgpwatcher_view_field_state_t state;

  /** Generic pointer to store information related to the view */
  void *user;

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
                               bgpstream_pfx_t *prefix,
                               bgpstream_peer_id_t peerid,
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


/** Get the current pfx info (ptr) for the current v4pfx.
 *
 * @param iter          Pointer to a valid iterator structure
 * @return the prefix-peer info that the iterator's peer field is currently
 *         pointed at, NULL if the iterator is not initialized, or has reached
 *         the end of the peers for the current prefix
 *
 * @note this returns the pfxinfo for the current peer
 * (BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER) for the current prefix
 * (BGPWATCHER_VIEW_ITER_FIELD_V4PFX).
 */
bgpwatcher_pfx_peer_info_t *
bgpwatcher_view_iter_get_v4pfx_pfxinfo(bgpwatcher_view_iter_t *iter);


/** Get the current pfx info (ptr) for the current v6pfx.
 *
 * @param iter          Pointer to a valid iterator structure
 * @return the prefix-peer info that the iterator's peer field is currently
 *         pointed at, NULL if the iterator is not initialized, or has reached
 *         the end of the peers for the current prefix
 *
 * @note this returns the pfxinfo for the current peer
 * (BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER) for the current prefix
 * (BGPWATCHER_VIEW_ITER_FIELD_V6PFX).
 */
bgpwatcher_pfx_peer_info_t *
bgpwatcher_view_iter_get_v6pfx_pfxinfo(bgpwatcher_view_iter_t *iter);

#endif /* __BGPWATCHER_VIEW_INT_H */
