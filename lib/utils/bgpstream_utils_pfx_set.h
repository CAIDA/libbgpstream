/*
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BGPSTREAM_UTILS_PFX_SET_H
#define __BGPSTREAM_UTILS_PFX_SET_H

#include "bgpstream_utils_pfx.h"

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
typedef struct bgpstream_pfx_set bgpstream_pfx_set_t;

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
bgpstream_pfx_set_t *bgpstream_pfx_set_create(void);

/** Insert a new prefix into the given set.
 *
 * @param set           pointer to the prefix set
 * @param pfx           pointer to the prefix to insert in the set
 * @return 1 if the prefix was inserted, 0 if it already existed, -1 if an
 * error occurred
 *
 * This function inserts a copy of the prefix (not the pointer).
 * It is also safe to pass a pointer to a bgpstream_ipv4_pfx_t or
 * bgpstream_ipv6_pfx_t (cast to bgpstream_pfx_t *) in the pfx parameter.
 */
int bgpstream_pfx_set_insert(bgpstream_pfx_set_t *set,
                             bgpstream_pfx_t *pfx);

/** Check whether a prefix exists in the set
 *
 * @param set           pointer to the prefix set
 * @param pfx           pointer to the prefix to look up
 * @return 0 if the prefix is not in the set, 1 if it is in the set
 *
 * It is also safe to pass a pointer to a bgpstream_ipv4_pfx_t or
 * bgpstream_ipv6_pfx_t (cast to bgpstream_pfx_t *) in the pfx parameter.
 */
int bgpstream_pfx_set_exists(bgpstream_pfx_set_t *set,
                             bgpstream_pfx_t *pfx);

/** Get the number of prefixes in the given set
 *
 * @param set           pointer to the prefix set
 * @return the size of the prefix set
 */
int bgpstream_pfx_set_size(bgpstream_pfx_set_t *set);

/** Get the number of IPv<v> prefixes in the given set
 *
 * @param set           pointer to the prefix set
 * @param v          IP version
 * @return the size of the prefix set
 */
int bgpstream_pfx_set_version_size(bgpstream_pfx_set_t *set,
                                   bgpstream_addr_version_t v);

/** Merge two prefix sets
 *
 * @param dst_set      pointer to the set to merge src into
 * @param src_set      pointer to the set to merge into dst
 * @return 0 if the sets were merged succsessfully, -1 otherwise
 */
int bgpstream_pfx_set_merge(bgpstream_pfx_set_t *dst_set,
                            bgpstream_pfx_set_t *src_set);

/** Destroy the given prefix set
 *
 * @param set           pointer to the prefix set to destroy
 */
void bgpstream_pfx_set_destroy(bgpstream_pfx_set_t *set);

/** Empty the prefix set.
 *
 * @param set           pointer to the prefix set to clear
 */
void bgpstream_pfx_set_clear(bgpstream_pfx_set_t *set);

/* IPv4 */

/** Create a new IPv4 Prefix set instance
 *
 * @return a pointer to the structure, or NULL if an error occurred
 */
bgpstream_ipv4_pfx_set_t *bgpstream_ipv4_pfx_set_create(void);

/** Insert a new prefix into the given set.
 *
 * @param set           pointer to the prefix set
 * @param pfx           pointer to the prefix to insert in the set
 * @return 1 if the prefix was inserted, 0 if it already existed, -1 if an
 * error occurred
 *
 * This function inserts a copy of the prefix (not the pointer).
 */
int bgpstream_ipv4_pfx_set_insert(bgpstream_ipv4_pfx_set_t *set,
                                  bgpstream_ipv4_pfx_t *pfx);

/** Check whether a prefix exists in the set
 *
 * @param set           pointer to the prefix set
 * @param pfx           pointer to the prefix to look up
 * @return 0 if the prefix is not in the set, 1 if it is in the set
 */
int bgpstream_ipv4_pfx_set_exists(bgpstream_ipv4_pfx_set_t *set,
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
bgpstream_ipv6_pfx_set_t *bgpstream_ipv6_pfx_set_create(void);

/** Insert a new prefix into the given set.
 *
 * @param set           pointer to the prefix set
 * @param pfx           pointer to the prefix to insert in the set
 * @return 1 if the prefix was inserted, 0 if it already existed, -1 if an
 * error occurred
 *
 * This function inserts a copy of the prefix (not the pointer).
 */
int bgpstream_ipv6_pfx_set_insert(bgpstream_ipv6_pfx_set_t *set,
                                  bgpstream_ipv6_pfx_t *pfx);

/** Check whether a prefix exists in the set
 *
 * @param set           pointer to the prefix set
 * @param pfx           pointer to the prefix to look up
 * @return 0 if the prefix is not in the set, 1 if it is in the set
 */
int bgpstream_ipv6_pfx_set_exists(bgpstream_ipv6_pfx_set_t *set,
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
