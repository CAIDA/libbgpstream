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



/** ######################### new iterators API  ######################### **/


/** Reset the prefix iterator to the first item for the given 
 *  IP version that also matches the mask
 *
 * @param iter          Pointer to an iterator structure
 * @param version       IP version (we may want to reset to
 *                      the first IPv4 or IPv6 prefix)
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @param all_versions  1 if the intention is to iterate over
 *                      all IP versions, 0 if the intention
 *                      is to iterate over 'version' prefixes
 *                      only
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 *
 * @note 1: if the intention is to iterate over all versions
 * of prefixes, then the version type has to be 
 * BGPSTREAM_ADDR_VERSION_IPV4
 *
 * @note 2: if the intention is to iterate over all versions
 * of prefixes, then it is possible that, at the end of the
 * function, the iterator will point at a prefix version
 * which appears in the table after the provided 'version'
 */
int
bgpwatcher_view_iter_first_pfx(bgpwatcher_view_iter_t *iter,
                               bgpstream_addr_version_t version,
                               uint8_t state_mask,
                               uint8_t all_versions);

/** Advance the provided iterator to the next prefix in the given view
 *
 * @param iter          Pointer to an iterator structure
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @param all_versions  1 if the intention is to iterate over
 *                      all IP versions, 0 if the intention
 *                      is to iterate over the current prefix
 *                      version
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_next_pfx(bgpwatcher_view_iter_t *iter,
                              uint8_t state_mask, 
                              uint8_t all_versions);

/** Check if the provided iterator point at an existing prefix
 *  or the end has been reached
 *
 * @param iter          Pointer to an iterator structure
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @param all_versions  1 if the intention is to iterate over
 *                      all IP versions, 0 if the intention
 *                      is to iterate over the current prefix
 *                      version
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_has_more_pfx(bgpwatcher_view_iter_t *iter,
                                  uint8_t state_mask, 
                                  uint8_t all_versions);

/** Check if the provided prefix exists in the current view
 *  and its state matches the mask provided; set the provided
 *  iterator to point at the prefix (if it exists) or set it
 *  to the end of the prefix table (if it doesn't exist)
 *
 * @param iter          Pointer to an iterator structure
 * @param pfx           A pointer to a prefix, the prefix version
 *                      will be extracted from this structure
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing prefix,
 *         0 if the end has been reached
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
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_next_peer(bgpwatcher_view_iter_t *iter,
                               uint8_t state_mask);

/** Check if the provided iterator point at an existing peer
 *  or the end has been reached
 *
 * @param iter          Pointer to an iterator structure
 * @param state_mask    A mask that indicates the state of the
 *                      fields we iterate through
 * @return 1 if the iterator points at an existing peer,
 *         0 if the end has been reached
 */
int
bgpwatcher_view_iter_has_more_peer(bgpwatcher_view_iter_t *iter,
                                   uint8_t state_mask);

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


/** ######################### old iterators API  ######################### **/


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

/** Get the iterator to the peerid in the given view (if it exists and it matches
 *  the mask)
 *
 * @param iter          Pointer to an iterator structure (this iterator will be 
 *                      modified and returned)
 * @param peerid        The peer id
 * @param state_mask    A mask that indicates the state of the field we iterate
 *                      through
 *
 * @return a pointer to the iterator structure that points to the given
 *         peerid if it exists, otherwise the bgpwatcher_view_iter_is_end(it) will be
 *         positive
 */
bgpwatcher_view_iter_t *
bgpwatcher_view_iter_seek_peerid(bgpwatcher_view_iter_t *iter,
                                 bgpstream_peer_id_t peerid,
                                 uint8_t state_mask); 

/** Get the iterator to the v4pfx in the given view (if it exists and it matches
 *  the mask)
 *
 * @param iter          Pointer to an iterator structure (this iterator will be 
 *                      modified and returned)
 * @param v4pfx         IPv4 prefix
 * @param state_mask    A mask that indicates the state of the field we iterate
 *                      through
 *
 * @return a pointer to the iterator structure that points to the given
 *         prefix if it exists, otherwise the bgpwatcher_view_iter_is_end(it) will be
 *         positive
 */
bgpwatcher_view_iter_t *
bgpwatcher_view_iter_seek_v4pfx(bgpwatcher_view_iter_t *iter,
                                bgpstream_ipv4_pfx_t *v4pfx,
                                uint8_t state_mask); 

/** Get the iterator to the v6pfx in the given view (if it exists and it matches
 *  the mask)
 *
 * @param iter          Pointer to an iterator structure (this iterator will be 
 *                      modified and returned)
 * @param v6pfx         IPv6 prefix
 * @param state_mask    A mask that indicates the state of the field we iterate
 *                      through
 *
 * @return a pointer to the iterator structure that points to the given
 *         prefix if it exists, otherwise the bgpwatcher_view_iter_is_end(it) will be
 *         positive
 */
bgpwatcher_view_iter_t *
bgpwatcher_view_iter_seek_v6pfx(bgpwatcher_view_iter_t *iter,
                                bgpstream_ipv6_pfx_t *v6pfx,
                                uint8_t state_mask); 


/** Get the iterator to the peerid for the currently pointed ipv4 prefix in the given
 *  view (if it exists and it matches the mask)
 *
 * @param iter          Pointer to an iterator structure (this iterator will be 
 *                      modified and returned)
 * @param peerid        The peer id
 * @param state_mask    A mask that indicates the state of the field we iterate
 *                      through
 *
 * @return a pointer to the iterator structure that points to the given
 *         peerid if it exists, otherwise the bgpwatcher_view_iter_is_end(it) will be
 *         positive
 */
bgpwatcher_view_iter_t *
bgpwatcher_view_iter_seek_v4pfx_peerid(bgpwatcher_view_iter_t *iter,
                                       bgpstream_peer_id_t peerid,
                                       uint8_t state_mask);

/** Get the iterator to the peerid for the currently pointed ipv6 prefix in the given
 *  view (if it exists and it matches the mask)
 *
 * @param iter          Pointer to an iterator structure (this iterator will be 
 *                      modified and returned)
 * @param peerid        The peer id
 * @param state_mask    A mask that indicates the state of the field we iterate
 *                      through
 *
 * @return a pointer to the iterator structure that points to the given
 *         peerid if it exists, otherwise the bgpwatcher_view_iter_is_end(it) will be
 *         positive
 */
bgpwatcher_view_iter_t *
bgpwatcher_view_iter_seek_v6pfx_peerid(bgpwatcher_view_iter_t *iter,
                                       bgpstream_peer_id_t peerid,
                                       uint8_t state_mask);

/** Get the iterator to the peerid for the specified ipv4 prefix
 *  view (if both exist and both match the relative masks)
 *
 * @param iter          Pointer to an iterator structure (this iterator will be 
 *                      modified and returned)
 * @param v4pfx         IPv4 prefix
 * @param peerid        The peer id
 * @param state_mask    A mask that indicates the state of the field we iterate
 *                      through
 *
 * @return a pointer to the iterator structure that points to the given prefix and
 *         peerid if they exist, otherwise the bgpwatcher_view_iter_is_end(it) will be
 *         positive
 */
bgpwatcher_view_iter_t *
bgpwatcher_view_iter_seek_v4pfx_peerid_pair(bgpwatcher_view_iter_t *iter,
                                            bgpstream_ipv4_pfx_t *v4pfx,
                                            bgpstream_peer_id_t peerid,
                                            uint8_t pfx_state_mask,
                                            uint8_t peer_state_mask);

/** Get the iterator to the peerid for the specified ipv6 prefix
 *  view (if both exist and both match the relative masks)
 *
 * @param iter          Pointer to an iterator structure (this iterator will be 
 *                      modified and returned)
 * @param v6pfx         IPv6 prefix
 * @param peerid        The peer id
 * @param state_mask    A mask that indicates the state of the field we iterate
 *                      through
 *
 * @return a pointer to the iterator structure that points to the given prefix and
 *         peerid if they exist, otherwise the bgpwatcher_view_iter_is_end(it) will be
 *         positive
 */
bgpwatcher_view_iter_t *
bgpwatcher_view_iter_seek_v6pfx_peerid_pair(bgpwatcher_view_iter_t *iter,
                                            bgpstream_ipv6_pfx_t *v6pfx,
                                            bgpstream_peer_id_t peerid,
                                            uint8_t pfx_state_mask,
                                            uint8_t peer_state_mask);

/**
 * @name View Iterator Getter and Setter Functions
 *
 * @{ */

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

/** Get the current origin AS number for the current v4pfx.
 *
 * @param iter          Pointer to a valid iterator structure
 * @return the origin AS number fore the current prefix as observed
 *         from the current peer, -1 if the iterator is not initialized,
 *         or has reached the end of the peers for the current prefix
 *
 * @note this returns the pfxinfo for the current peer
 * (BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER) for the current prefix
 * (BGPWATCHER_VIEW_ITER_FIELD_V4PFX).
 */
int
bgpwatcher_view_iter_get_v4pfx_orig_asn(bgpwatcher_view_iter_t *iter);

/** Get the current origin AS number for the current v6pfx.
 *
 * @param iter          Pointer to a valid iterator structure
 * @return the origin AS number fore the current prefix as observed
 *         from the current peer, -1 if the iterator is not initialized,
 *         or has reached the end of the peers for the current prefix
 *
 * @note this returns the pfxinfo for the current peer
 * (BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER) for the current prefix
 * (BGPWATCHER_VIEW_ITER_FIELD_V6PFX).
 */
int
bgpwatcher_view_iter_get_v6pfx_orig_asn(bgpwatcher_view_iter_t *iter);

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

/** @} */

#endif /* __BGPWATCHER_VIEW_H */
