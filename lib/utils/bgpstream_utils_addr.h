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
 */

#ifndef __BGPSTREAM_UTILS_ADDR_H
#define __BGPSTREAM_UTILS_ADDR_H

#include <stddef.h>
#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>

/** @file
 *
 * @brief Header file that exposes the public interface of BGP Stream Address
 * objects
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Public Enums
 *
 * @{ */

/** Version of a BGP Stream IP Address
 *
 * These share values with AF_INET and AF_INET6 so that the version field of a
 * BGP Stream address can be used directly with standard address manipulation
 * functions
 */
typedef enum {
  BGPSTREAM_ADDR_VERSION_UNKNOWN = 0,    ///< Address type unknown
  BGPSTREAM_ADDR_VERSION_IPV4 = AF_INET, ///< Address type IPv4
  BGPSTREAM_ADDR_VERSION_IPV6 = AF_INET6 ///< Address type IPv6
} bgpstream_addr_version_t;

/** Maximum number of IP versions */
#define BGPSTREAM_MAX_IP_VERSION_IDX 2

#define BS_ADDR4_OFFSET \
  offsetof(struct {bgpstream_addr_version_t v; struct in_addr a4;}, a4)
#define BS_ADDR6_OFFSET \
  offsetof(struct {bgpstream_addr_version_t v; struct in6_addr a6;}, a6)
#define BS_ADDR_OFFSET \
  ((BS_ADDR4_OFFSET > BS_ADDR6_OFFSET) ? BS_ADDR4_OFFSET : BS_ADDR6_OFFSET)

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** An IPv4 BGP Stream IP address */
typedef struct struct_bgpstream_ipv4_addr_t {

  union { // anonymous
    /** Version of the IP address (must always be BGPSTREAM_ADDR_VERSION_IPV4) */
    bgpstream_addr_version_t version;
    uint8_t _dummy[BS_ADDR_OFFSET]; // force alignment of addr
  };
  struct in_addr addr; ///< IPv4 Address

} bgpstream_ipv4_addr_t;

/** An IPv6 BGP Stream IP address */
typedef struct struct_bgpstream_ipv6_addr_t {

  union { // anonymous
    /** Version of the IP address (must always be BGPSTREAM_ADDR_VERSION_IPV5) */
    bgpstream_addr_version_t version;
    uint8_t _dummy[BS_ADDR_OFFSET]; // force alignment of addr
  };
  struct in6_addr addr; ///< IPv6 Address

} bgpstream_ipv6_addr_t;

/** Generic BGP Stream IP address.
 *
 * Holds any type of bgpstream address.  Specific types can be accessed via
 * a convenient union member instead of a cumbersome cast, e.g.
 * `addr_ptr->bs_ipv4` instead of `(bgpstream_ipv4_addr_t *)addr_ptr`
 * or `(const bgpstream_ipv4_addr_t *)addr_ptr`.
 */
typedef union union_bgpstream_ip_addr_t {
  struct { // anonymous
    union { // anonymous
      /// Version of the IP address
      bgpstream_addr_version_t version;
      uint8_t _dummy[BS_ADDR_OFFSET]; // force alignment of addr
    };
    uint8_t addr[1]; ///< raw bytes of the address
  };
  /// IPv4 variant of this struct, iff version == BGPSTREAM_ADDR_VERSION_IPV4
  bgpstream_ipv4_addr_t bs_ipv4;
  /// IPv6 variant of this struct, iff version == BGPSTREAM_ADDR_VERSION_IPV6
  bgpstream_ipv6_addr_t bs_ipv6;
} bgpstream_ip_addr_t;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Write the string representation of the given IP address into the given
 * character buffer.
 *
 * @param buf           pointer to a character buffer at least len bytes long
 * @param len           length of the given character buffer
 * @param bsaddr        pointer to the bgpstream addr to convert to string
 *
 * You will likely want to use INET_ADDRSTRLEN or INET6_ADDRSTRLEN to dimension
 * the buffer.
 */
#define bgpstream_addr_ntop(buf, len, bsaddr)                                  \
  inet_ntop((bsaddr)->version, &(bsaddr)->addr, buf, len)

/** Hash the given IPv4 address into a 32bit number
 *
 * @param addr          pointer to the IPv4 address to hash
 * @return 32bit hash of the address
 */
uint32_t
bgpstream_ipv4_addr_hash(const bgpstream_ipv4_addr_t *addr);

/** Hash the given IPv6 address into a 64bit number
 *
 * @param addr          pointer to the IPv6 address to hash
 * @return 64bit hash of the address
 */
uint64_t
bgpstream_ipv6_addr_hash(const bgpstream_ipv6_addr_t *addr);

/** Hash the given address into a 64bit number
 *
 * @param addr          pointer to the address to hash
 * @return 64bit hash of the address
 */
uint64_t
bgpstream_addr_hash(bgpstream_ip_addr_t *addr);

/** Compare two addresses for equality
 *
 * @param addr1         pointer to the first address to compare
 * @param addr2         pointer to the second address to compare
 * @return 0 if the addresses are not equal, non-zero if they are equal
 */
int bgpstream_addr_equal(const bgpstream_ip_addr_t *addr1,
                         const bgpstream_ip_addr_t *addr2);

/** Compare two IPv4 addresses for equality
 *
 * @param addr1         pointer to the first address to compare
 * @param addr2         pointer to the second address to compare
 * @return 0 if the addresses are not equal, non-zero if they are equal
 */
int bgpstream_ipv4_addr_equal(const bgpstream_ipv4_addr_t *addr1,
                              const bgpstream_ipv4_addr_t *addr2);

/** Compare two IPv6 addresses for equality
 *
 * @param addr1         pointer to the first address to compare
 * @param addr2         pointer to the second address to compare
 * @return 0 if the addresses are not equal, non-zero if they are equal
 */
int bgpstream_ipv6_addr_equal(const bgpstream_ipv6_addr_t *addr1,
                              const bgpstream_ipv6_addr_t *addr2);

/** Apply a mask to the given IP address
 *
 * @param addr          pointer to the address to mask
 * @param mask_len      number of bits to mask
 * @return pointer to the address masked
 *
 * If the mask length is longer than the address length (32 for IPv4, 128 for
 * IPv6), then the address will be left unaffected.
 */
bgpstream_ip_addr_t *bgpstream_addr_mask(bgpstream_ip_addr_t *addr,
                                        uint8_t mask_len);

/** Apply a mask to the given IPv4 address
 *
 * @param addr          pointer to the address to mask
 * @param mask_len      number of bits to mask
 * @return pointer to the address masked
 *
 * If the mask length is longer than 31 then the address will be left
 * unaffected.
 */
bgpstream_ipv4_addr_t *bgpstream_ipv4_addr_mask(bgpstream_ipv4_addr_t *addr,
                                                uint8_t mask_len);

/** Apply a mask to the given IPv6 address
 *
 * @param addr          pointer to the address to mask
 * @param mask_len      number of bits to mask
 * @return pointer to the address masked
 *
 * If the mask length is longer than 127 then the address will be left
 * unaffected.
 */
bgpstream_ipv6_addr_t *bgpstream_ipv6_addr_mask(bgpstream_ipv6_addr_t *addr,
                                                uint8_t mask_len);

/** Copy one address into another
 *
 * @param dst          pointer to the destination address
 * @param src          pointer to the source address
 *
 * The destination address structure **must** be large enough to hold the source
 * address type (e.g., if src points to an address structure, it may be
 * copied into a destination v4 structure **iff** the src version is v4)
 */
void bgpstream_addr_copy(bgpstream_ip_addr_t *dst,
    const bgpstream_ip_addr_t *src);

/** Initialize an IPv4 bgpstream address from an in_addr
 *
 * @param dst          pointer to the bgpstream destination address
 * @param src          pointer to the in_addr source address
 */
static inline void bgpstream_ipv4_addr_init(bgpstream_ip_addr_t *dst,
    const void *src)
{
  dst->version = BGPSTREAM_ADDR_VERSION_IPV4;
  memcpy(&dst->bs_ipv4.addr, src, 4);
}

/** Initialize an IPv6 bgpstream address from an in6_addr
 *
 * @param dst          pointer to the bgpstream destination address
 * @param src          pointer to the in6_addr source address
 */
static inline void bgpstream_ipv6_addr_init(bgpstream_ip_addr_t *dst,
    const void *src)
{
  dst->version = BGPSTREAM_ADDR_VERSION_IPV6;
  memcpy(&dst->bs_ipv6.addr, src, 16);
}

/** Convert a string into an address
 *
 * @param addr_str     pointer a string representing an IP address
 * @param addr         pointer to a bgpstream address
 * @return the pointer to an initialized bgpstream address, or NULL if the
 *         address is not valid
 */
bgpstream_ip_addr_t *bgpstream_str2addr(const char *addr_str,
                                       bgpstream_ip_addr_t *addr);

/** Returns the index associated to an IP version
 * @param v             enum rapresenting the IP address version
 * @return the index associated with the IP version, 255 if
 * there is an error in the translation
 */
uint8_t bgpstream_ipv2idx(bgpstream_addr_version_t v);

/** Returns the IP version associated with an index
 * @param i             index associated to the IP address version
 * @return the IP version associated with an index
 */
bgpstream_addr_version_t bgpstream_idx2ipv(uint8_t i);

/** Returns the number associated to an IP version
 * @param v             enum rapresenting the IP address version
 * @return the IP version number, 255 if there is an error in the
 * translation
 */
uint8_t bgpstream_ipv2number(bgpstream_addr_version_t v);

/** Returns the number associated to the index (associated to an IP version)
 * @param i             index associated to the IP address version
 * @return the index number, 255 if there is an error in the
 * translation
 */
uint8_t bgpstream_idx2number(uint8_t i);

/** @} */

#endif /* __BGPSTREAM_UTILS_ADDR_H */
