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


#ifndef __BGPSTREAM_UTILS_PFX_SET_H
#define __BGPSTREAM_UTILS_PFX_SET_H

#include <bgpstream_utils_pfx.h>

/** @file
 *
 * @brief Header file that exposes the public interface of the BGP Stream
 * Prefix Sets. There is one set for each bgpstream_pfx type (Storage, IPv4
 * and IPv6).
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Opaque Data Structures
 *
 * @{ */

/** Opaque structure containing an Prefix Storage set instance */
typedef struct bgpstream_pfx_storage_set bgpstream_pfx_storage_set_t;

/** Opaque structure containing an IPv4 Prefix set instance */
typedef struct bgpstream_ipv4_pfx_set bgpstream_ipv4_pfx_set_t;

/** Opaque structure containing an IPv6 Prefix set instance */
typedef struct bgpstream_ipv6_pfx_set bgpstream_ipv6_pfx_set_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/* STORAGE */

/** Create a new Prefix Storage set instance
 *
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_pfx_storage_set_t *bgpstream_pfx_storage_set_create();

/** Insert a new prefix into the given set.
 *
 * @param set           pointer to the prefix set
 * @param pfx          prefix to insert in the set
 * @return 1 if the prefix was inserted, 0 if it already existed, -1 if an
 * error occurred
 */
int bgpstream_pfx_storage_set_insert(bgpstream_pfx_storage_set_t *set,
                                      bgpstream_pfx_storage_t *pfx);

/** Get the number of prefixes in the given set
 *
 * @param set           pointer to the prefix set
 * @return the size of the prefix set
 */
int bgpstream_pfx_storage_set_size(bgpstream_pfx_storage_set_t *set);

/** Merge two prefix sets
 *
 * @param dst_set      pointer to the set to merge src into
 * @param src_set      pointer to the set to merge into dst
 * @return 0 if the sets were merged succsessfully, -1 otherwise
 */
int bgpstream_pfx_storage_set_merge(bgpstream_pfx_storage_set_t *dst_set,
                                     bgpstream_pfx_storage_set_t *src_set);

/** Destroy the given prefix set
 *
 * @param set           pointer to the prefix set to destroy
 */
void bgpstream_pfx_storage_set_destroy(bgpstream_pfx_storage_set_t *set);

/** Empty the prefix set.
 *
 * @param set           pointer to the prefix set to clear
 */
void bgpstream_pfx_storage_set_clear(bgpstream_pfx_storage_set_t *set);




/* IPv4 */

/** Create a new IPv4 Prefix set instance
 *
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_ipv4_pfx_set_t *bgpstream_ipv4_pfx_set_create();

/** Insert a new prefix into the given set.
 *
 * @param set           pointer to the prefix set
 * @param pfx          prefix to insert in the set
 * @return 1 if the prefix was inserted, 0 if it already existed, -1 if an
 * error occurred
 *
 * This function takes a copy of the prefix before it is inserted in the set.
 */
int bgpstream_ipv4_pfx_set_insert(bgpstream_ipv4_pfx_set_t *set,
                                      bgpstream_ipv4_pfx_t *pfx);

/** Get the number of prefixes in the given set
 *
 * @param set           pointer to the prefix set
 * @return the size of the prefix set
 */
int bgpstream_ipv4_pfx_set_size(bgpstream_ipv4_pfx_set_t *set);

/** Merge two prefix sets
 *
 * @param dst_set      pointer to the set to merge src into
 * @param src_set      pointer to the set to merge into dst
 * @return 0 if the sets were merged succsessfully, -1 otherwise
 */
int bgpstream_ipv4_pfx_set_merge(bgpstream_ipv4_pfx_set_t *dst_set,
                                  bgpstream_ipv4_pfx_set_t *src_set);

/** Destroy the given prefix set
 *
 * @param set           pointer to the prefix set to destroy
 */
void bgpstream_ipv4_pfx_set_destroy(bgpstream_ipv4_pfx_set_t *set);

/** Empty the prefix set.
 *
 * @param set           pointer to the prefix set to clear
 */
void bgpstream_ipv4_pfx_set_clear(bgpstream_ipv4_pfx_set_t *set);



/** Create a new IPv6 Prefix set instance
 *
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_ipv6_pfx_set_t *bgpstream_ipv6_pfx_set_create();

/** Insert a new prefix into the given set.
 *
 * @param set           pointer to the prefix set
 * @param pfx          prefix to insert in the set
 * @return 1 if the prefix was inserted, 0 if it already existed, -1 if an
 * error occurred
 */
int bgpstream_ipv6_pfx_set_insert(bgpstream_ipv6_pfx_set_t *set,
                                      bgpstream_ipv6_pfx_t *pfx);

/** Get the number of prefixes in the given set
 *
 * @param set           pointer to the prefix set
 * @return the size of the prefix set
 */
int bgpstream_ipv6_pfx_set_size(bgpstream_ipv6_pfx_set_t *set);

/** Merge two prefix sets
 *
 * @param dst_set      pointer to the set to merge src into
 * @param src_set      pointer to the set to merge into dst
 * @return 0 if the sets were merged succsessfully, -1 otherwise
 */
int bgpstream_ipv6_pfx_set_merge(bgpstream_ipv6_pfx_set_t *dst_set,
                                  bgpstream_ipv6_pfx_set_t *src_set);

/** Destroy the given prefix set
 *
 * @param set           pointer to the prefix set to destroy
 */
void bgpstream_ipv6_pfx_set_destroy(bgpstream_ipv6_pfx_set_t *set);

/** Empty the prefix set.
 *
 * @param set           pointer to the prefix set to clear
 */
void bgpstream_ipv6_pfx_set_clear(bgpstream_ipv6_pfx_set_t *set);

#endif /* __BGPSTREAM_UTILS_PFX_SET_H */


