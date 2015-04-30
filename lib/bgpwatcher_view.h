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

/** @} */

/**
 * @name Public Constants
 *
 * @{ */

/** BGPWATCHER_VIEW_FIELD_ALL_VALID is the expression to use
 *  when we do not need to specify ACTIVE or INACTIVE states,
 *  we are looking for any VALID state */
#define BGPWATCHER_VIEW_FIELD_ALL_VALID  \
  BGPWATCHER_VIEW_FIELD_ACTIVE | BGPWATCHER_VIEW_FIELD_INACTIVE


/** if an origin AS number is within this range:
 *  [BGPWATCHER_VIEW_ASN_NOEXPORT_START,BGPWATCHER_VIEW_ASN_NOEXPORT_END]
 *  the pfx-peer info will not be exported (i.e. sent through the io channel) */
#define BGPWATCHER_VIEW_ASN_NOEXPORT_START BGPWATCHER_VIEW_ASN_NOEXPORT_END - 255
#define BGPWATCHER_VIEW_ASN_NOEXPORT_END   0xffffffff

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

/** Callback for filtering peers in a view when sending from bgpwatcher_client.
 *
 * @param iter          iterator to the peer to check
 * @return 1 to include the peer, 0 to exclude the peer, and -1 if an error
 * occured.
 *
 * @note This callback will be called for every prefix/peer combination, so it
 * should be efficient at determining if a peer is to be included.
 */
typedef int (bgpwatcher_view_filter_peer_cb_t)(bgpwatcher_view_iter_t *iter);

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

/** Garbage collect a view
 *
 * @param view          view to garbage collect on
 *
 * This function frees memory marked as unused either by the
 * bgpwatcher_view_clear or the various *_remove_* functions.
 *
 * @note at this point, any user data stored in unused portions of the view will
 * be freed using the appropriate destructor.
 */
void
bgpwatcher_view_gc(bgpwatcher_view_t *view);

/** Disable user data for a view
 *
 * @param view          view to disable user data for
 *
 * Disables the user pointer for per-prefix peer information. This can reduce
 * memory consumption for applications that do not need the pfx-peer user
 * pointer. Be careful with use of this mode.
 */
void
bgpwatcher_view_disable_user_data(bgpwatcher_view_t *view);

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

/** Get the total number of active IPv4 prefixes in the view
 *
 * @param view          pointer to a view structure
 * @param state_mask    mask of pfx states to include in the count
 *                      (i.e. active and/or inactive pfxs)
 * @return the number of IPv4 prefixes in the view
 */
uint32_t
bgpwatcher_view_v4pfx_cnt(bgpwatcher_view_t *view, uint8_t state_mask);

/** Get the total number of active IPv6 prefixes in the view
 *
 * @param view          pointer to a view structure
 * @param state_mask    mask of pfx states to include in the count
 *                      (i.e. active and/or inactive pfxs)
 * @return the number of IPv6 prefixes in the view
 */
uint32_t
bgpwatcher_view_v6pfx_cnt(bgpwatcher_view_t *view, uint8_t state_mask);

/** Get the total number of active prefixes (v4+v6) in the view
 *
 * @param view          pointer to a view structure
 * @param state_mask    mask of pfx states to include in the count
 *                      (i.e. active and/or inactive pfxs)
 * @return the number of prefixes in the view
 */
uint32_t
bgpwatcher_view_pfx_cnt(bgpwatcher_view_t *view, uint8_t state_mask);

/** Get the number of active peers in the view
 *
 * @param view          pointer to a view structure
 * @param state_mask    mask of peer states to include in the count
 *                      (i.e. active and/or inactive peers)
 * @return the number of peers in the view
 */
uint32_t
bgpwatcher_view_peer_cnt(bgpwatcher_view_t *view, uint8_t state_mask);

/** Get the BGP time that the view represents
 *
 * @param view          pointer to a view structure
 * @return the time that the view represents
 */
uint32_t
bgpwatcher_view_get_time(bgpwatcher_view_t *view);

/** Set the BGP time that the view represents
 *
 * @param view          pointer to a view structure
 * @param time          time to set
 */
void
bgpwatcher_view_set_time(bgpwatcher_view_t *view, uint32_t time);

/** Get the wall time that this view was created
 *
 * @param view          pointer to a view structure
 * @return the time that the view represents
 */
uint32_t
bgpwatcher_view_get_time_created(bgpwatcher_view_t *view);

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

/** @} */

/**
 * @name View Iterator Add/Remove Functions
 *
 * @{ */

/** Insert a new peer in the BGP Watcher view
 *
 * @param iter             pointer to a view iterator
 * @param collector_str    pointer to the peer's collector name
 * @param peer_address     pointer to a peer's ip address
 * @param peer_asnumber    peer's AS number
 * @return the peer_id that is associated with the inserted peer if successful,
 *         0 otherwise
 *
 * When a new peer is created its state is set to inactive. if the peer is
 * already present, it will not be modified.
 *
 * When this function returns successfully, the provided iterator will be
 * pointing to the inserted peer (even if it already existed).
 *
 */
bgpstream_peer_id_t
bgpwatcher_view_iter_add_peer(bgpwatcher_view_iter_t *iter,
                              char *collector_str,
                              bgpstream_ip_addr_t *peer_address,
                              uint32_t peer_asnumber);

/** Remove the current peer from the BGP Watcher view
 *
 * @param iter             pointer to a view iterator
 * @return 0 if the peer was removed successfully, -1 otherwise
 *
 * @note for efficiency, this function may not actually free the memory
 * associated with the peer. If memory should be reclaimed, run the
 * bgpwatcher_view_gc function after removals are complete.
 *
 * After removing a peer, the provided iterator will be advanced to the next
 * peer that matches the current iterator configuration, or will be invalidated
 * if there are no more peers.
 *
 * If the peer is currently active, it will be deactivated prior to removal.
 */
int
bgpwatcher_view_iter_remove_peer(bgpwatcher_view_iter_t *iter);

/** Insert a new pfx-peer information in the BGP Watcher view
 *
 * @param iter             pointer to a view iterator
 * @param pfx              pointer to the prefix
 * @param peer_id          peer identifier
 * @param origin_asn       origin AS number for the prefix as observed
 *                         by peer peer_id
 * @return 0 if the insertion was successful, <0 otherwise
 *
 * In order for the function to succeed the peer must exist (it can be either
 * active or inactive).
 * When a new pfx-peer is created its state is set to inactive.
 * When this function returns successfully, the provided iterator will be
 * pointing to the inserted prefix-peer (even if it already existed).
 */
int
bgpwatcher_view_iter_add_pfx_peer(bgpwatcher_view_iter_t *iter,
                                  bgpstream_pfx_t *pfx,
                                  bgpstream_peer_id_t peer_id,
                                  uint32_t origin_asn);

/** Remove the current pfx currently referenced by the given iterator
 *
 * @param iter             pointer to a view iterator
 * @return 0 if the pfx was removed successfully, -1 otherwise
 *
 * @note for efficiency, this function may not actually free the memory
 * associated with the pfx. If memory should be reclaimed, run the
 * bgpwatcher_view_gc function after removals are complete.
 *
 * After removing a pfx, the provided iterator will be advanced to the next pfx
 * that matches the current iterator configuration, or will be invalidated if
 * there are no more pfxs.
 *
 * If the pfx is currently active, it will be deactivated prior to removal (thus
 * deactivating and removing all associated pfx-peers).
 */
int
bgpwatcher_view_iter_remove_pfx(bgpwatcher_view_iter_t *iter);

/** Insert a new peer info into the currently iterated pfx
 *
 * @param iter             pointer to a view iterator
 * @param peer_id          peer identifier
 * @param origin_asn       origin AS number for the prefix as observed
 *                         by peer peer_id
 * @return 0 if the insertion was successful, <0 otherwise
 *
 * @note: in order for the function to succeed the peer must
 *        exist (it can be either active or inactive)
 * @note: when a new pfx-peer is created its state is set to
 *        inactive.
 */
int
bgpwatcher_view_iter_pfx_add_peer(bgpwatcher_view_iter_t *iter,
                                  bgpstream_peer_id_t peer_id,
                                  uint32_t origin_asn);

/** Remove the current peer from the current prefix currently referenced by the
 * given iterator
 *
 * @param iter             pointer to a view iterator
 * @return 0 if the pfx-peer was removed successfully, -1 otherwise
 *
 * @note for efficiency, this function may not actually free the memory
 * associated with the pfx-peer. If memory should be reclaimed, run the
 * bgpwatcher_view_gc function after removals are complete.
 *
 * After removing a pfx-peer, the provided iterator will be advanced to the next
 * pfx-peer that matches the current iterator configuration, or will be
 * invalidated if there are no more pfx-peers.
 *
 * If the pfx-peer is currently active, it will be deactivated prior to
 * removal.
 *
 * If this is the last pfx-peer for the current pfx, the pfx will also be
 * removed.
 */
int
bgpwatcher_view_iter_pfx_remove_peer(bgpwatcher_view_iter_t *iter);

/** @} */

/**
 * @name View Iterator Getter and Setter Functions
 *
 * @{ */


/** Get the current view
 *
 * @param iter          Pointer to an iterator structure
 * @return a pointer to the current view,
 *         NULL if the iterator is not initialized.
 */
bgpwatcher_view_t *
bgpwatcher_view_iter_get_view(bgpwatcher_view_iter_t *iter);

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
 * @param state_mask    mask of peer states to include in the count
 *                      (i.e. active and/or inactive peers)
 * @return the number of peers providing information for the prefix,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 */
int
bgpwatcher_view_iter_pfx_get_peer_cnt(bgpwatcher_view_iter_t *iter,
                                      uint8_t state_mask);

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
bgpwatcher_view_iter_peer_get_peer_id(bgpwatcher_view_iter_t *iter);

/** Get the peer signature for the current peer id
 *
 * @param iter          Pointer to an iterator structure
 * @return the signature associated with the peer id,
 *         NULL if the iterator is not initialized, or has reached the end of
 *         the peers.
 */
bgpstream_peer_sig_t *
bgpwatcher_view_iter_peer_get_sig(bgpwatcher_view_iter_t *iter);


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
bgpwatcher_view_iter_peer_get_pfx_cnt(bgpwatcher_view_iter_t *iter,
                                      int version,
                                      uint8_t state_mask);

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


/** Get the origin AS number for the current pfx-peer structure pointed by
 *  the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @return the origin AS number,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the peers for the given prefix.
 */
int
bgpwatcher_view_iter_pfx_peer_get_orig_asn(bgpwatcher_view_iter_t *iter);


/** Set the origin AS number for the current pfx-peer structure pointed by
 *  the given iterator
 *
 * @param iter          Pointer to an iterator structure
 * @param asn           Origin AS number
 * @return 0 if the process ends correctly, -1 if the iterator is not
 *         initialized, or has reached the end of the peers for the
 *         given prefix.
 */
int
bgpwatcher_view_iter_pfx_peer_set_orig_asn(bgpwatcher_view_iter_t *iter, uint32_t asn);


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

/**
 * @name View Iterator Activate Deactivate Functions
 *
 * @{ */

/** Activate the current peer
 *
 * @param iter          Pointer to an iterator structure
 * @return  0 if the peer was already active
 *          1 if the peer was inactive and it became active,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 */
int
bgpwatcher_view_iter_activate_peer(bgpwatcher_view_iter_t *iter);

/** De-activate the current peer
 *
 * @param iter          Pointer to an iterator structure
 * @return  0 if the peer was already inactive
 *          1 if the peer was active and it became inactive,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 * @note the function deactivates all the peer-pfxs of the bgpview
 *       associated with the same peer
 */
int
bgpwatcher_view_iter_deactivate_peer(bgpwatcher_view_iter_t *iter);

/** Activate the current prefix:
 *  a prefix is active only when there is at least one prefix
 *  peer info which is active. In order to have a coherent
 *  behavior the only way to activate a prefix is either to
 *  activate a peer-pfx or to insert/add a peer-pfx (that
 *  automatically causes the activation.
 */

/** De-activate the current prefix
 *
 * @param iter          Pointer to an iterator structure
 * @return  0 if the prefix was already inactive
 *          1 if the prefix was active and it became inactive,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the prefixes.
 * @note the function deactivates all the peer-pfxs associated with the
 *       same prefix
 */
int
bgpwatcher_view_iter_deactivate_pfx(bgpwatcher_view_iter_t *iter);

/** Activate the current pfx-peer
 *
 * @param iter          Pointer to an iterator structure
 * @return  0 if the pfx-peer was already active
 *          1 if the pfx-peer was inactive and it became active,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the peers for the given prefix.
 *
 * @note: this function will automatically activate the corresponding
 *        prefix and peer (if they are not active already)
 */
int
bgpwatcher_view_iter_pfx_activate_peer(bgpwatcher_view_iter_t *iter);

/** De-activate the current pfx-peer
 *
 * @param iter          Pointer to an iterator structure
 * @return  0 if the pfx-peer was already inactive
 *          1 if the pfx-peer was active and it became inactive,
 *         -1 if the iterator is not initialized, or has reached the end of
 *         the peers for the given prefix.
 * @note if this is the last peer active for the the given prefix, then it
 *       deactivates the prefix.
 */
int
bgpwatcher_view_iter_pfx_deactivate_peer(bgpwatcher_view_iter_t *iter);


/** @} */


#endif /* __BGPWATCHER_VIEW_H */
