/*
 * Copyright (C) 2015 The Regents of the University of California.
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
 *
 * Authors:
 *   Chiara Orsini
 *   Alistair King
 *   Shane Alcock <salcock@waikato.ac.nz>
 */

#ifndef __BGPSTREAM_UTILS_PFX_H
#define __BGPSTREAM_UTILS_PFX_H

#include "bgpstream_utils_addr.h"

#define BGPSTREAM_PREFIX_MATCH_ANY 0
#define BGPSTREAM_PREFIX_MATCH_EXACT 1
#define BGPSTREAM_PREFIX_MATCH_MORE 2
#define BGPSTREAM_PREFIX_MATCH_LESS 3

/** @file
 *
 * @brief Header file that exposes the public interface of BGP Stream Prefix
 * objects
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Public Data Structures
 *
 * @{ */

/** An IPv4 BGP Stream Prefix */
typedef struct struct_bgpstream_ipv4_pfx_t {

  /** Length of the prefix mask */
  uint8_t mask_len;

  /** Indicates what type of matches are allowed with this prefix.
   *  For filtering purposes only.
   */
  uint8_t allowed_matches;

  union {
    /** The address */
    bgpstream_ipv4_addr_t address;
    max_align_t _dummy; // force alignment
  };

} bgpstream_ipv4_pfx_t;

/** An IPv6 BGP Stream Prefix */
typedef struct struct_bgpstream_ipv6_pfx_t {

  /** Length of the prefix mask */
  uint8_t mask_len;

  /** Indicates what type of matches are allowed with this prefix.
   *  For filtering purposes only.
   */
  uint8_t allowed_matches;

  union {
    /** The address */
    bgpstream_ipv6_addr_t address;
    max_align_t _dummy; // force alignment
  };

} bgpstream_ipv6_pfx_t;

/** Generic BGP Stream Prefix
 *
 * Holds any type of bgpstream prefix.  Specific types can be accessed via a
 * convenient union member instead of a cumbersome cast, e.g.
 * `pfx_ptr->bs_ipv4` instead of `(bgpstream_ipv4_pfx_t *)pfx_ptr`
 * or `(const bgpstream_ipv4_pfx_t *)pfx_ptr`.
 */
typedef union union_bgpstream_pfx_t {

  /// Generic variant
  struct {
    /** Length of the prefix mask */
    uint8_t mask_len;

    /** Indicates what type of matches are allowed with this prefix.
     *  For filtering purposes only.
     */
    uint8_t allowed_matches;

    union {
      /** The address */
      bgpstream_ip_addr_t address;
      max_align_t _dummy; // force alignment
    };
  };

  /// IPv4 variant, iff address.version == BGPSTREAM_ADDR_VERSION_IPV4
  bgpstream_ipv4_pfx_t bs_ipv4;
  /// IPv6 variant, iff address.version == BGPSTREAM_ADDR_VERSION_IPV6
  bgpstream_ipv6_pfx_t bs_ipv6;

} bgpstream_pfx_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Write the string representation of the given prefix into the given
 * character buffer.
 *
 * @param buf           pointer to a character buffer at least len bytes long
 * @param len           length of the given character buffer
 * @param pfx           pointer to the bgpstream pfx to convert to string
 * @param returns a pointer to buf if successful, NULL otherwise
 *
 * You will likely want to use INET_ADDRSTRLEN+3 or INET6_ADDRSTRLEN+3 to
 * dimension the buffer.
 */
char *bgpstream_pfx_snprintf(char *buf, size_t len, const bgpstream_pfx_t *pfx);

/** Copy one prefix into another
 *
 * @param dst          pointer to the destination prefix
 * @param src          pointer to the source prefix
 *
 * The destination prefix structure **must** be large enough to hold the source
 * prefix type (e.g., if src points to a generic prefix structure, it may be
 * copied into a destination v4 structure **iff** the src version is v4)
 */
void bgpstream_pfx_copy(bgpstream_pfx_t *dst, const bgpstream_pfx_t *src);

/** Hash the given IPv4 Prefix into a 32bit number
 *
 * @param pfx          pointer to the IPv4 prefix to hash
 * @return 32bit hash of the prefix
 */
#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_ipv4_pfx_hash(const bgpstream_ipv4_pfx_t *pfx);

/** Hash the given IPv6 prefix into a 64bit number
 *
 * @param pfx          pointer to the IPv6 prefix to hash
 * @return 64bit hash of the prefix
 */
#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_ipv6_pfx_hash(const bgpstream_ipv6_pfx_t *pfx);

/** Hash the given prefix into a 64bit number
 *
 * @param pfx          pointer to the prefix to hash
 * @return 64bit hash of the prefix
 */
#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_pfx_hash(const bgpstream_pfx_t *pfx);

/** Compare two prefixes for equality
 *
 * @param pfx1          pointer to the first prefix to compare
 * @param pfx2          pointer to the first prefix to compare
 * @return 0 if the prefixes are not equal, non-zero if they are equal
 */
int bgpstream_pfx_equal(const bgpstream_pfx_t *pfx1,
                        const bgpstream_pfx_t *pfx2);

/** Compare two IPv4 prefixes for equality
 *
 * @param pfx1         pointer to the first prefix to compare
 * @param pfx2         pointer to the second prefix to compare
 * @return 0 if the prefixes are not equal, non-zero if they are equal
 */
int bgpstream_ipv4_pfx_equal(const bgpstream_ipv4_pfx_t *pfx1,
                             const bgpstream_ipv4_pfx_t *pfx2);

/** Compare two IPv6 prefixes for equality
 *
 * @param pfx1         pointer to the first prefix to compare
 * @param pfx2         pointer to the second prefix to compare
 * @return 0 if the prefixes are not equal, non-zero if they are equal
 */
int bgpstream_ipv6_pfx_equal(const bgpstream_ipv6_pfx_t *pfx1,
                             const bgpstream_ipv6_pfx_t *pfx2);

/** Check if one prefix contains another
 *
 * @param outer          pointer to the outer prefix
 * @param inner          pointer to the inner prefix to check
 * @return non-zero if inner is a more-specific prefix of outer, 0 if not
 */
int bgpstream_pfx_contains(const bgpstream_pfx_t *outer,
                           const bgpstream_pfx_t *inner);

/** Utility macros used to pass khashes objects by reference
 *  instead of copying them */

#define bgpstream_pfx_hash_val(a)          bgpstream_pfx_hash(&(a))
#define bgpstream_pfx_equal_val(a, b)      bgpstream_pfx_equal(&(a), &(b))

#define bgpstream_ipv4_pfx_hash_val(a)     bgpstream_ipv4_pfx_hash(&(a))
#define bgpstream_ipv4_pfx_equal_val(a, b) bgpstream_ipv4_pfx_equal(&(a), &(b))

#define bgpstream_ipv6_pfx_hash_val(a)     bgpstream_ipv6_pfx_hash(&(a))
#define bgpstream_ipv6_pfx_equal_val(a, b) bgpstream_ipv6_pfx_equal(&(a), &(b))

/** Convert a string into a prefix
 *
 * @param pfx_str     pointer a string representing an IP prefix
 * @param pfx         pointer to a prefix
 * @return the pointer to an initialized pfx, NULL if the
 *         prefix is not valid
 */
bgpstream_pfx_t *bgpstream_str2pfx(const char *pfx_str, bgpstream_pfx_t *pfx);

/** @} */

#endif /* __BGPSTREAM_UTILS_PFX_H */
