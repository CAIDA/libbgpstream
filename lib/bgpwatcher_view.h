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

  /** The current field is invalid (it could
   *  be destroyed and the status of the view
   *  would still be consistent). The number 
   *  associated with the enumerator is such 
   *  that no mask will ever iterate/seek
   *  over this field (it is exactly equivalent
   *  to a non existent field). */
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

  /** Iterate over the Peer information in the view */
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

/** Reset the prefix iterator to the first item for the given 
 *  IP version that also matches the mask
 *
 * @param iter          Pointer to an iterator structure
 * @param version       0 if the intention is to iterate over
 *                      all IP versions, BGPSTREAM_ADDR_VERSION_IPV4 or
 *                      BGPSTREAM_ADDR_VERSION_IPV6 to iterate over a 
 *                      single version
 * @param state_mask    A mask that indicates the state of the pfx
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 *
 * @note 1: the mask provided is permanent until a new first or 
 *          a new seek function is called
 */
int
bgpwatcher_view_iter_first_pfx(bgpwatcher_view_iter_t *iter,
                               int version,
                               uint8_t state_mask);

/** Advance the provided iterator to the next prefix in the given view
 *
 * @param iter          Pointer to an iterator structure
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_next_pfx(bgpwatcher_view_iter_t *iter);

/** Check if the provided iterator point at an existing prefix
 *  or the end has been reached
 *
 * @param iter          Pointer to an iterator structure
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_has_more_pfx(bgpwatcher_view_iter_t *iter);

/** Check if the provided prefix exists in the current view
 *  and its state matches the mask provided; set the provided
 *  iterator to point at the prefix (if it exists) or set it
 *  to the end of the prefix table (if it doesn't exist)
 *
 * @param iter          Pointer to an iterator structure
 * @param state_mask    A mask that indicates the state of the pfx
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 *
 * @note: the seek function sets the version to pfx->version and
 *        sets the state mask for pfx iteration
 */
int
bgpwatcher_view_iter_seek_pfx(bgpwatcher_view_iter_t *iter,
                              bgpstream_pfx_t *pfx,
                              uint8_t state_mask);

/** Reset the peer iterator to the first peer that matches 
 *  the mask
 *
 * @param iter          Pointer to an iterator structure
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_first_peer(bgpwatcher_view_iter_t *iter,                               
                                uint8_t state_mask);

/** Advance the provided iterator to the next peer in the given view
 *
 * @param iter          Pointer to an iterator structure
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_next_peer(bgpwatcher_view_iter_t *iter);

/** Check if the provided iterator point at an existing peer
 *  or the end has been reached
 *
 * @param iter          Pointer to an iterator structure
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_has_more_peer(bgpwatcher_view_iter_t *iter);

/** Check if the provided peer exists in the current view
 *  and its state matches the mask provided; set the provided
 *  iterator to point at the peer (if it exists) or set it
 *  to the end of the peer table (if it doesn't exist)
 *
 * @param iter          Pointer to an iterator structure
 * @param peerid        The peer id
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_seek_peer(bgpwatcher_view_iter_t *iter,
                               bgpstream_peer_id_t peerid,
                               uint8_t state_mask);


/** Reset the peer iterator to the first peer (of the current
 *  prefix) that matches the mask 
 *
 * @param iter          Pointer to an iterator structure
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 *
 * @note: everytime the iterator is moved to a new peer
 * the peer iterator updated accordingly (e.g. if pfx_peer
 * points at pfx1 peer1, then the pfx iterator will point
 * at pfx1, the peer iterator will point at peer 1, and the
 * pfx_point iterator at the peer1 info associated with pfx1
 */
int
bgpwatcher_view_iter_pfx_first_peer(bgpwatcher_view_iter_t *iter,                               
                                    uint8_t state_mask);

/** Advance the provided iterator to the next peer that 
 * matches the mask for the current prefix
 *
 * @param iter          Pointer to an iterator structure
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_pfx_next_peer(bgpwatcher_view_iter_t *iter);

/** Check if the provided iterator point at an existing peer
 *  for the current prefix or the end has been reached
 *
 * @param iter          Pointer to an iterator structure
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_pfx_has_more_peer(bgpwatcher_view_iter_t *iter);

/** Check if the provided peer exists for the current prefix
 *  and its state matches the mask provided; set the provided
 *  iterator to point at the peer (if it exists) or set it
 *  to the end of the prefix-peer table (if it doesn't exist)
 *
 * @param iter          Pointer to an iterator structure
 * @param peerid        The peer id
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_pfx_seek_peer(bgpwatcher_view_iter_t *iter,
                                   bgpstream_peer_id_t peerid,
                                   uint8_t state_mask);

/** Reset the peer iterator to the first peer that matches the
 *  the peer mask for the first pfx that matches the IP version
 *  and the pfx_mask
 *
 * @param iter          Pointer to an iterator structure
 * @param version       0 if the intention is to iterate over
 *                      all IP versions, BGPSTREAM_ADDR_VERSION_IPV4 or
 *                      BGPSTREAM_ADDR_VERSION_IPV6 to iterate over a 
 *                      single version
 * @param pfx_mask      A mask that indicates the state of the
 *                      prefixes we iterate through
 * @param peer_mask     A mask that indicates the state of the
 *                      peers we iterate through
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 *
 * @note: everytime the iterator is moved to a new peer
 * the peer iterator updated accordingly (e.g. if pfx_peer
 * points at pfx1 peer1, then the pfx iterator will point
 * at pfx1, the peer iterator will point at peer 1, and the
 * pfx_point iterator at the peer1 info associated with pfx1
 */
int
bgpwatcher_view_iter_first_pfx_peer(bgpwatcher_view_iter_t *iter,
                                    int version,
                                    uint8_t pfx_mask,
                                    uint8_t peer_mask);

/** Advance the provided iterator to the next peer that matches the
 *  the peer mask for the next pfx that matches the pfx_mask
 * @param iter          Pointer to an iterator structure
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_next_pfx_peer(bgpwatcher_view_iter_t *iter);

/** Check if the provided iterator point at an existing peer/prefix
 *  or the end has been reached
 *
 * @param iter          Pointer to an iterator structure
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_has_more_pfx_peer(bgpwatcher_view_iter_t *iter);

/** Check if the provided peer exists for the given prefix 
 *  and their states match the masks provided; set the provided
 *  iterator to point at the peer (if it exists) or set it
 *  to the end of the prefix/peer tables (if it doesn't exist)
 *
 * @param iter          Pointer to an iterator structure
 * @param pfx           A pointer to a prefix, the prefix version
 *                      will be extracted from this structure
 * @param pfx_mask      A mask that indicates the state of the
 *                      prefixes we iterate through
 * @param peer_mask     A mask that indicates the state of the
 *                      peers we iterate through
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 * @note: the seek function sets the version to pfx->version and
 *        sets the state mask for pfx iteration
 */
int
bgpwatcher_view_iter_seek_pfx_peer(bgpwatcher_view_iter_t *iter,
                                   bgpstream_pfx_t *pfx,
                                   bgpstream_peer_id_t peerid,                                   
                                   uint8_t pfx_mask,
                                   uint8_t peer_mask);

/**
 * @name View Iterator Getter and Setter Functions
 *
 * @{ */

/** Get the current prefix for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the prefix the pfx_iterator is currently pointing at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 */
bgpstream_pfx_t *
bgpwatcher_view_iter_pfx_get_pfx(bgpwatcher_view_iter_t *iter);

/** Get the number of peers providing information for the
 *  current prefix pointed by the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the number of peers providing information for the prefix,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 */
int
bgpwatcher_view_iter_pfx_get_peers_cnt(bgpwatcher_view_iter_t *iter);

/** Get the state of the current prefix pointed by the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the state of the prefix (either ACTIVE or INACTIVE)
 *         INVALID if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 */
bgpwatcher_view_field_state_t
bgpwatcher_view_iter_pfx_get_state(bgpwatcher_view_iter_t *iter);

/** Get the current user ptr of the current prefix
 *
 * @param iter          Pointer to an iterator structure
 * @return the user ptr that the pfx_iterator is currently pointing at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 */
void *
bgpwatcher_view_iter_pfx_get_user(bgpwatcher_view_iter_t *iter);

/** Set the user ptr for the current prefix
 *
 * @param iter          Pointer to an iterator structure
 * @param user          Pointer to a memory to borrow to the prefix 
 * @return  0 if the user pointer already pointed at the same memory location,
 *          1 if the internal user pointer has been updated,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 */
int
bgpwatcher_view_iter_pfx_set_user(bgpwatcher_view_iter_t *iter, void *user);

/** Get the current peer id for the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the peer id that is currently pointing at,
 *         0 if the iterator is not initialized, or has reached the end of
 *         the peers.
 */
bgpstream_peer_id_t 
bgpwatcher_view_iter_peer_get_peer(bgpwatcher_view_iter_t *iter);

/** Get the peer signature for the current peer id
 *
 * @param iter          Pointer to an iterator structure
 * @return the signature associated with the peer id,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the peers.
 */
bgpstream_peer_sig_t * 
bgpwatcher_view_iter_peer_get_sign(bgpwatcher_view_iter_t *iter);


/** Get the number of prefixes (ipv4, ipv6, or all) that the current
 *  peer observed
 *
 * @param iter          Pointer to an iterator structure
 * @param version       0 if the intention is to consider over
 *                      all IP versions, BGPSTREAM_ADDR_VERSION_IPV4 or
 *                      BGPSTREAM_ADDR_VERSION_IPV6 to consider a
 *                      single version
 * @return number of observed prefixes,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the peers.
 */
int
bgpwatcher_view_iter_peer_get_pfx_count(bgpwatcher_view_iter_t *iter,
                                        int version);

/** Get the state of the current peer pointed by the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the state of the peer (either ACTIVE or INACTIVE)
 *         INVALID if the iterator is not initialized, or has reached the end of
 *         the peers.
 */
bgpwatcher_view_field_state_t
bgpwatcher_view_iter_peer_get_state(bgpwatcher_view_iter_t *iter);

/** Get the current user ptr of the current peer
 *
 * @param iter          Pointer to an iterator structure
 * @return the user ptr that the peer_iterator is currently pointing at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the peers.
 */
void *
bgpwatcher_view_iter_peer_get_user(bgpwatcher_view_iter_t *iter);

/** Set the user ptr for the current peer
 *
 * @param iter          Pointer to an iterator structure
 * @param user          Pointer to a memory to borrow to the peer 
 * @return  0 if the user pointer already pointed at the same memory location,
 *          1 if the internal user pointer has been updated,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the peers.
 */
int
bgpwatcher_view_iter_peer_set_user(bgpwatcher_view_iter_t *iter, void *user);


/** Get the origin AS number for the current pfx-peer structure pointed by the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the origin AS number, 
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the peer for the given prefix.
 */
int
bgpwatcher_view_iter_pfx_peer_get_orig_asn(bgpwatcher_view_iter_t *iter);


/** Get the state of the current pfx-peer pointed by the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the state of the pfx-peer (either ACTIVE or INACTIVE)
 *         INVALID if the iterator is not initialized, or has reached the end of
 *         the peers for the given prefix.
 */
bgpwatcher_view_field_state_t
bgpwatcher_view_iter_pfx_peer_get_state(bgpwatcher_view_iter_t *iter);

/** Get the current user ptr of the current pfx-peer
 *
 * @param iter          Pointer to an iterator structure
 * @return the user ptr that the pfx-peer iterator is currently pointing at,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the peers for the given prefix.
 */
void *
bgpwatcher_view_iter_pfx_peer_get_user(bgpwatcher_view_iter_t *iter);

/** Set the user ptr for the current pfx-peer
 *
 * @param iter          Pointer to an iterator structure
 * @param user          Pointer to a memory to borrow to the pfx-peer 
 * @return  0 if the user pointer already pointed at the same memory location,
 *          1 if the internal user pointer has been updated,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the peers for the given prefix.
 */
int
bgpwatcher_view_iter_pfx_peer_set_user(bgpwatcher_view_iter_t *iter, void *user);

/** @} */

/** @} */


#endif /* __BGPWATCHER_VIEW_H */
