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

#ifndef __BGPWATCHER_VIEW_H
#define __BGPWATCHER_VIEW_H

#include "bgpstream_utils_peer_sig_map.h"

// #include "bgpwatcher_common.h" /* < pfx_peer_info_t */

/** @file
 *
 * @brief Header file that exposes the public interface of bgpwatcher view.
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque handle to an instance of a BGP View table.
 *
 * All interaction with a view must be done through the view API exposed in
 * bgpwatcher_view.h
 */
typedef struct bgpwatcher_view bgpwatcher_view_t;

/** Opaque handle for iterating over fields of a BGP View table. */
typedef struct bgpwatcher_view_iter bgpwatcher_view_iter_t;

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

typedef enum {

  /** The current field is invalid (never initialized) */
  BGPWATCHER_VIEW_FIELD_INVALID   = 0b000,

  /** The current field is active */
  BGPWATCHER_VIEW_FIELD_ACTIVE    = 0b001,

  /** The current field is inactive */
  BGPWATCHER_VIEW_FIELD_INACTIVE  = 0b010,

} bgpwatcher_view_field_state_t;

typedef enum {

  /** Iterate over the IPv4 prefixes in the view */
  BGPWATCHER_VIEW_ITER_FIELD_V4PFX      = 1,

  /** Iterate over the IPv6 prefixes in the view */
  BGPWATCHER_VIEW_ITER_FIELD_V6PFX      = 2,

  /** Iterate over the Peer information (peerid=>(collector,IP)) in the view */
  BGPWATCHER_VIEW_ITER_FIELD_PEER       = 3,

  /** Iterate over the peers for the current v4 pfx */
  BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER = 4,

  /** Iterate over the peers for the current v6 pfx */
  BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER = 5,

} bgpwatcher_view_iter_field_t;

/** @} */

/**
 * @name Public Constants
 *
 * @{ */

/** BGPWATCHER_VIEW_FIELD_ALL_VALID is the expression to use
 *  when we do not need to specify ACTIVE or INACTIVE states,
 *  we are looking for any VALID state */
#define BGPWATCHER_VIEW_FIELD_ALL_VALID  BGPWATCHER_VIEW_FIELD_ACTIVE | BGPWATCHER_VIEW_FIELD_INACTIVE

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** @todo: remove from here and make it private */

/** Information about a prefix from a peer */
typedef struct bgpwatcher_pfx_peer_info {

  /** Origin ASN */
  uint32_t orig_asn;

  /** @todo add other pfx info fields here (AS path, etc) */

  /** If set, this prefix is seen by this peer.
   *
   * @note this is also used by the store to track which peers are active for a
   * prefix
   */
  // uint8_t in_use;

  /** State of the per-pfx per-peer data */
  bgpwatcher_view_field_state_t state;

  /** Generic pointer to store per-pfx-per-peer information */
  void *user;

} __attribute__((packed)) bgpwatcher_pfx_peer_info_t;

/** Callback for destroying a custom user structure associated with bgpwatcher
 *  view or one of its substructures
 * @param user    user pointer to destroy
 */
typedef void (bgpwatcher_view_destroy_user_t) (void* user);


/** @} */

/** Create a new BGP View
 *
 * A BGP View holds a snapshot of the aggregated prefix information.
 * Basically, it maps from prefix -> peers -> prefix info
 * 
 * @param bwv_user_destructor           a function that destroys the user structure 
 *                                      in the bgpwatcher_view_t structure
 * @param bwv_peer_user_destructor      a function that destroys the user structure 
 *                                      used in each bwv_peerinfo_t structure
 * @param bwv_pfx_user_destructor       a function that destroys the user structure 
 *                                      used in each bwv_peerid_pfxinfo_t structure
 * @param bwv_pfx_peer_user_destructor  a function that destroys the user structure 
 *                                      used in each bgpwatcher_pfx_peer_info_t structure
 *
 * @return a pointer to the view if successful, NULL otherwise
 *
 * The destroy functions passed are called everytime the associated user pointer
 * is set to a new value or when the corresponding structure are destroyed.
 * If a NULL parameter is passed, then when a the user pointer is set to a new
 * value (or when the structure containing such a user pointer is deallocated)
 * no destructor is called. In other words, when the destroy callback is set to
 * NULL the programmer is responsible for deallocating the user memory outside
 * the bgpwatcher API.
 */
bgpwatcher_view_t *
bgpwatcher_view_create(bgpwatcher_view_destroy_user_t *bwv_user_destructor,
                       bgpwatcher_view_destroy_user_t *bwv_peer_user_destructor,
                       bgpwatcher_view_destroy_user_t *bwv_pfx_user_destructor,
                       bgpwatcher_view_destroy_user_t *bwv_pfx_peer_user_destructor);

/** Create a new BGP View, reusing an existing peersigns table
 *
 * @param peersigns     pointer to a peersigns map to share
 *
 * A BGP View holds a snapshot of the aggregated prefix information.
 * Basically, it maps from prefix -> peers -> prefix info
 *
 * @param bwv_user_destructor           a function that destroys the user structure 
 *                                      in the bgpwatcher_view_t structure
 * @param bwv_peer_user_destructor      a function that destroys the user structure 
 *                                      used in each bwv_peerinfo_t structure
 * @param bwv_pfx_user_destructor       a function that destroys the user structure 
 *                                      used in each bwv_peerid_pfxinfo_t structure
 * @param bwv_pfx_peer_user_destructor  a function that destroys the user structure 
 *                                      used in each bgpwatcher_pfx_peer_info_t structure
 *
 * @return a pointer to the view if successful, NULL otherwise
 */
bgpwatcher_view_t *
bgpwatcher_view_create_shared(bgpstream_peer_sig_map_t *peersigns,
                              bgpwatcher_view_destroy_user_t *bwv_user_destructor,
                              bgpwatcher_view_destroy_user_t *bwv_peer_user_destructor,
                              bgpwatcher_view_destroy_user_t *bwv_pfx_user_destructor,
                              bgpwatcher_view_destroy_user_t *bwv_pfx_peer_user_destructor);

/** @todo create a nice high-level api for accessing information in the view */

/** Destroy the given BGP Watcher View
 *
 * @param view          pointer to the view to destroy
 */
void
bgpwatcher_view_destroy(bgpwatcher_view_t *view);

/** Empty a view
 *
 * @param view          view to clear
 *
 * This does not actually free any memory, it just marks prefix and peers as
 * dirty so that future inserts can re-use the memory allocation. It does *not*
 * clear the peersigns table.
 */
void
bgpwatcher_view_clear(bgpwatcher_view_t *view);

/** WARNING!!!!! change name here, not to be confused with user
 * pointer in the bgpview!
 * Destroy all the per-pfx user data
 *
 * @param view          view to destroy user data for
 * @param call_back     function that actually destroy the specific user structure
 *                      used in this view
 */
// void bgpwatcher_view_destroy_user(bgpwatcher_view_t *view,
//				  bgpwatcher_view_destroy_user_cb *call_back);

/** Dump the given BGP View to stdout
 *
 * @param view        pointer to a view structure
 */
void
bgpwatcher_view_dump(bgpwatcher_view_t *view);

/**
 * @name Simple Accessor Functions
 *
 * @{ */

/** Get the total number of IPv4 prefixes in the view
 *
 * @param view          pointer to a view structure
 * @return the number of IPv4 prefixes in the view
 */
uint32_t
bgpwatcher_view_v4pfx_size(bgpwatcher_view_t *view);

/** Get the total number of IPv6 prefixes in the view
 *
 * @param view          pointer to a view structure
 * @return the number of IPv6 prefixes in the view
 */
uint32_t
bgpwatcher_view_v6pfx_size(bgpwatcher_view_t *view);

/** Get the total number of prefixes (v4+v6) in the view
 *
 * @param view          pointer to a view structure
 * @return the number of prefixes in the view
 */
uint32_t
bgpwatcher_view_pfx_size(bgpwatcher_view_t *view);

/** Get the number of peers in the view
 *
 * @param view          pointer to a view structure
 * @return the number of peers in the view
 */
uint32_t
bgpwatcher_view_peer_size(bgpwatcher_view_t *view);

/** Get the BGP time that the view represents
 *
 * @param view          pointer to a view structure
 * @return the time that the view represents
 */
uint32_t
bgpwatcher_view_time(bgpwatcher_view_t *view);

/** Get the user pointer associated with the view
 *
 * @param view          pointer to a view structure
 * @return the user pointer associated with the view
 */
void *
bgpwatcher_view_get_user(bgpwatcher_view_t *view);

/** Set the user pointer associated with the view
 *
 * @param view          pointer to a view structure
 * @param user          user pointer to associate with the view structure
 * @return 1 if a new user pointer is set, 0 if the user pointer was already
 *         set to the address provided.
 */
int
bgpwatcher_view_set_user(bgpwatcher_view_t *view, void *user);

/** Set the user destructor function. If the function is set to NULL,
 *  then no the user pointer will not be destroyed by the bgpwatcher
 *  functions, the programmer is responsible for that in its own program.
 *
 * @param view                  pointer to a view structure
 * @param bwv_user_destructor   function that destroys the view user memory
 */
void
bgpwatcher_view_set_user_destructor(bgpwatcher_view_t *view,
                                    bgpwatcher_view_destroy_user_t *bwv_user_destructor);

/** @} */

/**
 * @name View Iterator Functions
 *
 * @{ */

/** Create a new view iterator
 *
 * @param               Pointer to the view to create iterator for
 * @return pointer to an iterator if successful, NULL otherwise
 */
bgpwatcher_view_iter_t *
bgpwatcher_view_iter_create(bgpwatcher_view_t *view);

/** Destroy the given iterator
 *
 * @param               Pointer to the iterator to destroy
 */
void
bgpwatcher_view_iter_destroy(bgpwatcher_view_iter_t *iter);

/** Reset the given iterator to the first item for the given field
 *
 * @param iter          Pointer to an iterator structure
 * @param field         The iterator field to reset
 * @param state_mask    A mask that indicates the state of the field we iterate through
 */
void
bgpwatcher_view_iter_first(bgpwatcher_view_iter_t *iter,
                           bgpwatcher_view_iter_field_t field,
                           uint8_t state_mask);

/** Check if the given iterator has reached the end of the items for the given
 * field.
 *
 * @param iter          Pointer to an iterator structure
 * @param field         The iterator field to check
 * @param state_mask    A mask that indicates the state of the field we iterate through
 * @return 0 if the iterator is pointing to a valid item, 1 if all items have
 * been iterated over.
 *
 * @note the various `get` functions will return invalid results when this
 * function returns 1
 */
int
bgpwatcher_view_iter_is_end(bgpwatcher_view_iter_t *iter,
                            bgpwatcher_view_iter_field_t field,
                            uint8_t state_mask);

/** Advance the provided iterator to the next prefix in the given view
 *
 * @param iter          Pointer to an iterator structure
 * @param field         The iterator field to advance
 * @param state_mask    A mask that indicates the state of the field we iterate through
 *
 * @note this function will have no effect if bgpwatcher_view_iter_is_end
 * returns non-zero for the given field.
 */
void
bgpwatcher_view_iter_next(bgpwatcher_view_iter_t *iter,
                          bgpwatcher_view_iter_field_t field,
                          uint8_t state_mask);

/** Get the total number of items in the iterator for the given field
 *
 * @param iter          Pointer to an iterator structure
 * @param field         The iterator field to get the size of
 * @param state_mask    A mask that indicates the state of the field we iterate through
 * @return the number of items for the given field
 *
 * @note this function can be called at any time using a valid iterator for the
 * v4pfxs, v6pfxs, and peers fields, but must only be called while the
 * corresponding top-level (v4pfx or v6pfx) iterator is valid to get the size
 * for the v4pfx_peers and v6pfx_peers fields respectively.
 */
uint64_t
bgpwatcher_view_iter_size(bgpwatcher_view_iter_t *iter,
                          bgpwatcher_view_iter_field_t field,
                          uint8_t state_mask);

/** Advance the provided iterator to the next prefix in the given view
 *
 * @param iter          Pointer to an iterator structure
 * @param field         The iterator field to advance
 *
 * @note this function will have no effect if bgpwatcher_view_iter_is_end
 * returns non-zero for the given field.
 */
/* void */
/* bgpwatcher_view_iter_seek_peer(bgpwatcher_view_iter_t *iter, */
/*                                ********HERE************* */
/*                                bgpwatcher_view_iter_field_t field); */



/** Get the current v4 prefix for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the prefix that the iterator's v4pfx field is currently pointed at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the v4 prefixes.
 */
bgpstream_ipv4_pfx_t *
bgpwatcher_view_iter_get_v4pfx(bgpwatcher_view_iter_t *iter);

/** Get the current v6 prefix for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the prefix that the iterator's v6pfx field is currently pointed at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the v6 prefixes.
 */
bgpstream_ipv6_pfx_t *
bgpwatcher_view_iter_get_v6pfx(bgpwatcher_view_iter_t *iter);

/** Get the current v4 prefix user pointer for the given iterator
 *
 * @param iter      Pointer to an iterator structure
 * @return the user pointer that the iterator's v4pfx field is currently pointed at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the v4 prefixes.
 */
void *
bgpwatcher_view_iter_get_v4pfx_user(bgpwatcher_view_iter_t *iter);

/** Get the current v6 prefix user pointer for the given iterator
 *
 * @param iter      Pointer to an iterator structure
 * @return the user pointer that the iterator's v4pfx field is currently pointed at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the v6 prefixes.
 */
void *
bgpwatcher_view_iter_get_v6pfx_user(bgpwatcher_view_iter_t *iter);

/** Set the current v4 prefix user pointer for the given iterator
 *
 * @param iter      Pointer to an iterator structure
 * @param user      Pointer to store per-pfx information on consumers
 * @return 1 if a new user pointer is set, 0 if the user pointer
 *           provided was already set, -1 if the iterator is not initialized,
 *           or has reached the end of the peers.
 */
int
bgpwatcher_view_iter_set_v4pfx_user(bgpwatcher_view_iter_t *iter, void *user);

/** Set the current v6 prefix user pointer for the given iterator
 *
 * @param iter      Pointer to an iterator structure
 * @param user      Pointer to store per-pfx information on consumers
 * @return 1 if a new user pointer is set, 0 if the user pointer
 *           provided was already set, -1 if the iterator is not initialized,
 *           or has reached the end of the peers.
 */
int
bgpwatcher_view_iter_set_v6pfx_user(bgpwatcher_view_iter_t *iter, void *user);

/** Set the pfx user destructor function. If the function is set to NULL,
 *  then no the user pointer will not be destroyed by the bgpwatcher
 *  functions, the programmer is responsible for that in its own program.
 *
 * @param view                       pointer to a view structure
 * @param bwv_pfx_user_destructor    function that destroys the per-pfx user memory
 */
void
bgpwatcher_view_set_pfx_user_destructor(bgpwatcher_view_t *view,
                                        bgpwatcher_view_destroy_user_t *bwv_pfx_user_destructor);

/** Get the current peer ID for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the peer ID that the iterator's peer field is currently pointed at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the peers
 */
bgpstream_peer_id_t
bgpwatcher_view_iter_get_peerid(bgpwatcher_view_iter_t *iter);

/** Get the current peer signature for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the peer signature that the iterator's peer field is currently
 *         pointed at, NULL if the iterator is not initialized, or has reached
 *         the end of the peers
 */
bgpstream_peer_sig_t *
bgpwatcher_view_iter_get_peersig(bgpwatcher_view_iter_t *iter);

/** Get the current peer's IPv4 prefix count for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the number of prefixes observed by the peer currently pointed at by
 *         the iterator's peer field. -1 if the iterator is not initialized, or
 *         has reached the end of the peers.
 */
int
bgpwatcher_view_iter_get_peer_v4pfx_cnt(bgpwatcher_view_iter_t *iter);

/** Get the current peer's IPv6 prefix count for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the number of prefixes observed by the peer currently pointed at by
 *         the iterator's peer field. -1 if the iterator is not initialized, or
 *         has reached the end of the peers.
 */
int
bgpwatcher_view_iter_get_peer_v6pfx_cnt(bgpwatcher_view_iter_t *iter);

/** Get the current peer's user pointer for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the user pointer associated witht the peer currently pointed at by 
 *         the iterator's peer field. NULL if the iterator is not initialized, or
 *         has reached the end of the peers.
 */
void *
bgpwatcher_view_iter_get_peer_user(bgpwatcher_view_iter_t *iter);

/** Set the current peer's user pointer for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @param user          Pointer to a user structure
 * @return 1 if a new user pointer is set, 0 if the user pointer
 *           provided was already set, -1 if the iterator is not initialized,
 *           or has reached the end of the peers.
 */
int
bgpwatcher_view_iter_set_peer_user(bgpwatcher_view_iter_t *iter, void *user);

/** Set the peer user destructor function. If the function is set to NULL,
 *  then no the user pointer will not be destroyed by the bgpwatcher
 *  functions, the programmer is responsible for that in its own program.
 *
 * @param view                       pointer to a view structure
 * @param bwv_peer_user_destructor   function that destroys the per-peer view user memory
 */
void
bgpwatcher_view_set_peer_user_destructor(bgpwatcher_view_t *view,
                                              bgpwatcher_view_destroy_user_t *bwv_peer_user_destructor);

/** Get the current peer ID (key) for the current v4pfx.
 *
 * @param iter          Pointer to a valid iterator structure
 * @return the peer ID that the iterator's v4pfx_peer field is currently pointed
 *         at, NULL if the iterator is not initialized, or has reached the end
 *         of the peers for the current v4 prefix
 *
 * @note this returns the current peer (BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER)
 * for the current prefix (BGPWATCHER_VIEW_ITER_FIELD_V4PFX).
 *
 * @note the peer ID is only meaningful *within* a view.
 */
bgpstream_peer_id_t
bgpwatcher_view_iter_get_v4pfx_peerid(bgpwatcher_view_iter_t *iter);

/** Get the current peer ID (key) for the current v6pfx.
 *
 * @param iter          Pointer to a valid iterator structure
 * @return the peer ID that the iterator's v6pfx_peer field is currently pointed
 *         at, NULL if the iterator is not initialized, or has reached the end
 *         of the peers for the current v6 prefix
 *
 * @note this returns the current peer (BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER)
 * for the current prefix (BGPWATCHER_VIEW_ITER_FIELD_V6PFX).
 *
 * @note the peer ID is only meaningful *within* a view.
 */
bgpstream_peer_id_t
bgpwatcher_view_iter_get_v6pfx_peerid(bgpwatcher_view_iter_t *iter);

/** Get the current peer signature (key) for the current v4pfx.
 *
 * @param iter          Pointer to a valid iterator structure
 * @return the peer signature info that the iterator's v4pfx_peer field is
 *         currently pointed at, NULL if the iterator is not initialized, or has
 *         reached the end of the peers for the current v4 prefix
 *
 * @note this returns the current peer (BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER)
 * for the current prefix (BGPWATCHER_VIEW_ITER_FIELD_V4PFX).
 */
bgpstream_peer_sig_t *
bgpwatcher_view_iter_get_v4pfx_peersig(bgpwatcher_view_iter_t *iter);

/** Get the current peer signature (key) for the current v6pfx.
 *
 * @param iter          Pointer to a valid iterator structure
 * @return the peer signature info that the iterator's v6pfx_peer field is
 *         currently pointed at, NULL if the iterator is not initialized, or has
 *         reached the end of the peers for the current v6 prefix
 *
 * @note this returns the current peer (BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER)
 * for the current prefix (BGPWATCHER_VIEW_ITER_FIELD_V6PFX).
 */
bgpstream_peer_sig_t *
bgpwatcher_view_iter_get_v6pfx_peersig(bgpwatcher_view_iter_t *iter);

/** Get the current pfx info (value) for the current v4pfx.
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

/** Get the current pfx info (value) for the current v6pfx.
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



/* ######## ######## ######## ######## ######## ######## ######## 
 * TODO: write an accessor function for each field in 
 *  bgpwatcher_pfx_peer_info_t (so far we explicitely take care
 *  of the user pointer, nothing more)
 * ######## ######## ######## ######## ######## ######## ######## */


/** Get the current v4 prefix-peer's user pointer for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the user pointer associated with the pfx-peer currently pointed at by 
 *         the iterator's pfx-peer field. NULL if the iterator is not initialized, or
 *         has reached the end of the pfx-peers.
 */
void *
bgpwatcher_view_iter_get_v4pfx_pfxinfo_user(bgpwatcher_view_iter_t *iter);

/** Get the current v6 prefix-peer's user pointer for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the user pointer associated with the pfx-peer currently pointed at by 
 *         the iterator's pfx-peer field. NULL if the iterator is not initialized, or
 *         has reached the end of the pfx-peers.
 */
void *
bgpwatcher_view_iter_get_v6pfx_pfxinfo_user(bgpwatcher_view_iter_t *iter);

/** Set the current v4 prefix-peer user pointer for the given iterator
 *
 * @param iter      Pointer to an iterator structure
 * @param user      Pointer to store per-pfx per-peer information on consumers
 * @return 1 if a new user pointer is set, 0 if the user pointer
 *           provided was already set, -1 if the iterator is not initialized,
 *           or has reached the end of the peers.
 */
int
bgpwatcher_view_iter_set_v4pfx_pfxinfo_user(bgpwatcher_view_iter_t *iter, void *user);

/** Set the current v6 prefix-peer user pointer for the given iterator
 *
 * @param iter      Pointer to an iterator structure
 * @param user      Pointer to store per-pfx per-peer information on consumers
 * @return 1 if a new user pointer is set, 0 if the user pointer
 *           provided was already set, -1 if the iterator is not initialized,
 *           or has reached the end of the peers.
 */
int
bgpwatcher_view_iter_set_v6pfx_pfxinfo_user(bgpwatcher_view_iter_t *iter, void *user);

/** Set the pfx-peer user destructor function. If the function is set to NULL,
 *  then no the user pointer will not be destroyed by the bgpwatcher
 *  functions, the programmer is responsible for that in its own program.
 *
 * @param view                            pointer to a view structure
 * @param bwv_pfx_peer_user_destructor    function that destroys the per-pfx per-peer user memory
 */
void
bgpwatcher_view_set_pfx_peer_user_destructor(bgpwatcher_view_t *view,
                                               bgpwatcher_view_destroy_user_t *bwv_pfx_peer_user_destructor);

/** @} */

#endif /* __BGPWATCHER_VIEW_H */
