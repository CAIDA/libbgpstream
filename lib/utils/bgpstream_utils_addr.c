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

#include "utils.h"
#include <assert.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "khash.h"

#include "bgpstream_log.h"
#include "bgpstream_utils_addr.h"

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_ipv4_addr_hash(const bgpstream_ipv4_addr_t *addr)
{
  return __ac_Wang_hash(addr->addr.s_addr);
}

#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_ipv6_addr_hash(const bgpstream_ipv6_addr_t *addr)
{
  return __ac_Wang_hash(*((const khint64_t *)(&(addr->addr.s6_addr[0]))));
}

#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_addr_hash(bgpstream_ip_addr_t *addr)
{
  switch (addr->version) {
  case BGPSTREAM_ADDR_VERSION_IPV4:
    return bgpstream_ipv4_addr_hash(&addr->bs_ipv4);

  case BGPSTREAM_ADDR_VERSION_IPV6:
    return bgpstream_ipv6_addr_hash(&addr->bs_ipv6);

  default:
    return 0;
  }
}

int bgpstream_addr_equal(const bgpstream_ip_addr_t *addr1,
    const bgpstream_ip_addr_t *addr2)
{
  if (addr1->version != addr2->version)
    return 0;
  else if (addr1->version == BGPSTREAM_ADDR_VERSION_IPV4)
    return bgpstream_ipv4_addr_equal(&addr1->bs_ipv4, &addr2->bs_ipv4);
  else if (addr1->version == BGPSTREAM_ADDR_VERSION_IPV6)
    return bgpstream_ipv6_addr_equal(&addr1->bs_ipv6, &addr2->bs_ipv6);
  else
    return 0;
}

int bgpstream_ipv4_addr_equal(const bgpstream_ipv4_addr_t *addr1,
                              const bgpstream_ipv4_addr_t *addr2)
{
  return addr1->addr.s_addr == addr2->addr.s_addr;
}

int bgpstream_ipv6_addr_equal(const bgpstream_ipv6_addr_t *addr1,
                              const bgpstream_ipv6_addr_t *addr2)
{
  return memcmp(&(addr1->addr.s6_addr[0]), &(addr2->addr.s6_addr[0]), 16) == 0;
}

bgpstream_ip_addr_t *bgpstream_addr_mask(bgpstream_ip_addr_t *addr,
                                         uint8_t mask_len)
{
  if (addr->version == BGPSTREAM_ADDR_VERSION_IPV4) {
    return (bgpstream_ip_addr_t *)bgpstream_ipv4_addr_mask(
      &addr->bs_ipv4, mask_len);
  }
  if (addr->version == BGPSTREAM_ADDR_VERSION_IPV6) {
    return (bgpstream_ip_addr_t *)bgpstream_ipv6_addr_mask(
      &addr->bs_ipv6, mask_len);
  }
  return NULL;
}

bgpstream_ipv4_addr_t *bgpstream_ipv4_addr_mask(bgpstream_ipv4_addr_t *addr,
                                                uint8_t mask_len)
{
  if (mask_len > 32) {
    mask_len = 32;
  }

  addr->addr.s_addr &= htonl(~(((uint64_t)1 << (32 - mask_len)) - 1));
  return addr;
}

bgpstream_ipv6_addr_t *bgpstream_ipv6_addr_mask(bgpstream_ipv6_addr_t *addr,
                                                uint8_t mask_len)
{
  uint64_t *ptr;

  if (mask_len > 128) {
    mask_len = 128;
  }

  if (mask_len <= 64) {
    /* mask the bottom 64bits and zero the top 64bits */
    ptr = (uint64_t *)&(addr->addr.s6_addr[8]);
    *ptr = 0;
    ptr = (uint64_t *)&(addr->addr.s6_addr[0]);
    *ptr &= htonll((uint64_t)(~0) << (64 - mask_len));
  } else {
    /* mask the top 64 bits */
    mask_len -= 64;
    ptr = (uint64_t *)&(addr->addr.s6_addr[8]);
    *ptr &= htonll((uint64_t)(~0) << (64 - mask_len - 64));
  }

  return addr;
}

void bgpstream_addr_copy(bgpstream_ip_addr_t *dst,
    const bgpstream_ip_addr_t *src)
{
  if (src->version == BGPSTREAM_ADDR_VERSION_IPV4) {
    memcpy(dst, src, sizeof(bgpstream_ipv4_addr_t));
  } else if (src->version == BGPSTREAM_ADDR_VERSION_IPV6) {
    memcpy(dst, src, sizeof(bgpstream_ipv6_addr_t));
  }
}

bgpstream_ip_addr_t *bgpstream_str2addr(const char *addr_str,
                                        bgpstream_ip_addr_t *addr)
{
  if (addr_str == NULL || addr == NULL) {
    return NULL;
  }

  if (strchr(addr_str, ':') != NULL) {
    /* this looks like it will be an IPv6 address */
    if (inet_pton(AF_INET6, addr_str, &addr->bs_ipv6.addr) != 1) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not parse address string %s",
                    addr_str);
      return NULL;
    }
    addr->version = BGPSTREAM_ADDR_VERSION_IPV6;
  } else {
    /* probably a v4 address */
    if (inet_pton(AF_INET, addr_str, &addr->bs_ipv4.addr) != 1) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not parse address string %s",
                    addr_str);
      return NULL;
    }
    addr->version = BGPSTREAM_ADDR_VERSION_IPV4;
  }

  return addr;
}

uint8_t bgpstream_ipv2idx(bgpstream_addr_version_t v)
{
  switch (v) {
  case BGPSTREAM_ADDR_VERSION_IPV4:
    return 0;
  case BGPSTREAM_ADDR_VERSION_IPV6:
    return 1;
  default:
    assert(0);
  }
  return 255;
}

bgpstream_addr_version_t bgpstream_idx2ipv(uint8_t i)
{
  switch (i) {
  case 0:
    return BGPSTREAM_ADDR_VERSION_IPV4;
  case 1:
    return BGPSTREAM_ADDR_VERSION_IPV6;
  default:
    assert(0);
  }
  return BGPSTREAM_ADDR_VERSION_UNKNOWN;
}

uint8_t bgpstream_ipv2number(bgpstream_addr_version_t v)
{
  switch (v) {
  case BGPSTREAM_ADDR_VERSION_IPV4:
    return 4;
  case BGPSTREAM_ADDR_VERSION_IPV6:
    return 6;
  default:
    assert(0);
  }
  return 255;
}

uint8_t bgpstream_idx2number(uint8_t i)
{
  switch (i) {
  case 0:
    return 4;
  case 1:
    return 6;
  default:
    assert(0);
  }
  return 255;
}
