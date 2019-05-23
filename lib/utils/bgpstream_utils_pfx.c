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

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "khash.h"

#include "bgpstream_utils_pfx.h"

char *bgpstream_pfx_snprintf(char *buf, size_t len, const bgpstream_pfx_t *pfx)
{
  char *p = buf;

  /* print the address */
  if (bgpstream_addr_ntop(buf, len, &(pfx->address)) == NULL) {
    return NULL;
  }

  while (*p != '\0') {
    p++;
    len--;
  }

  /* print the mask */
  snprintf(p, len, "/%" PRIu8, pfx->mask_len);

  return buf;
}

void bgpstream_pfx_copy(bgpstream_pfx_t *dst, const bgpstream_pfx_t *src)
{
  if (src->address.version == BGPSTREAM_ADDR_VERSION_IPV4) {
    memcpy(dst, src, sizeof(bgpstream_ipv4_pfx_t));
  }
  if (src->address.version == BGPSTREAM_ADDR_VERSION_IPV6) {
    memcpy(dst, src, sizeof(bgpstream_ipv6_pfx_t));
  }
}

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_ipv4_pfx_hash(const bgpstream_ipv4_pfx_t *pfx)
{
  // embed the network mask length in the 32 bits
  return __ac_Wang_hash(pfx->address.addr.s_addr | (uint32_t)pfx->mask_len);
}

#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_ipv6_pfx_hash(const bgpstream_ipv6_pfx_t *pfx)
{
  // ipv6 number - we take most significant 64 bits only
  uint64_t address = *((const uint64_t *)(&(pfx->address.addr.s6_addr[0])));
  // embed the network mask length in the 64 bits
  return __ac_Wang_hash(address | (uint64_t)pfx->mask_len);
}

#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_pfx_hash(const bgpstream_pfx_t *pfx)
{
  if (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4) {
    return bgpstream_ipv4_pfx_hash((const bgpstream_ipv4_pfx_t *)pfx);
  }
  if (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV6) {
    return bgpstream_ipv6_pfx_hash((const bgpstream_ipv6_pfx_t *)pfx);
  }
  return 0;
}

int bgpstream_ipv4_pfx_equal(const bgpstream_ipv4_pfx_t *pfx1,
                             const bgpstream_ipv4_pfx_t *pfx2)
{
  return ((pfx1->mask_len == pfx2->mask_len) &&
          bgpstream_ipv4_addr_equal(&pfx1->address, &pfx2->address));
}

int bgpstream_ipv6_pfx_equal(const bgpstream_ipv6_pfx_t *pfx1,
                             const bgpstream_ipv6_pfx_t *pfx2)
{
  // note: it could be faster to use a loop when inserting differing prefixes

  return ((pfx1->mask_len == pfx2->mask_len) &&
          bgpstream_ipv6_addr_equal(&pfx1->address, &pfx2->address));
}

int bgpstream_pfx_equal(const bgpstream_pfx_t *pfx1,
                        const bgpstream_pfx_t *pfx2)
{
  if (pfx1->mask_len == pfx2->mask_len) {
    return bgpstream_addr_equal(&pfx1->address, &pfx2->address);
  }
  return 0;
}

int bgpstream_pfx_contains(const bgpstream_pfx_t *outer, const bgpstream_pfx_t *inner)
{
  bgpstream_ip_addr_t tmp;

  if (outer->address.version != inner->address.version ||
      outer->mask_len > inner->mask_len) {
    return 0;
  }

  bgpstream_addr_copy(&tmp, &inner->address);
  bgpstream_addr_mask(&tmp, outer->mask_len);
  return bgpstream_addr_equal(&tmp, &outer->address);
}

bgpstream_pfx_t *bgpstream_str2pfx(const char *pfx_str, bgpstream_pfx_t *pfx)
{
  if (pfx_str == NULL || pfx == NULL) {
    return NULL;
  }

  char pfx_copy[INET6_ADDRSTRLEN + 3];
  char *endptr = NULL;

  /* strncpy() functions copy at most len characters from src into
   * dst.  If src is less than len characters long, the remainder of
   * dst is filled with `\0' characters.  Otherwise, dst is not
   * terminated. */
  strncpy(pfx_copy, pfx_str, INET6_ADDRSTRLEN + 3);
  if (pfx_copy[INET6_ADDRSTRLEN + 3 - 1] != '\0') {
    return NULL;
  }

  /* get pointer to ip/mask divisor */
  char *found = strchr(pfx_copy, '/');
  if (found == NULL) {
    return NULL;
  }

  *found = '\0';
  /* get the ip address */
  if (bgpstream_str2addr(pfx_copy, &pfx->address) == NULL) {
    return NULL;
  }

  /* get the mask len */
  errno = 0;
  unsigned long int r = strtoul(found + 1, &endptr, 10);
  int ret = errno;
  if (!(endptr != NULL && *endptr == '\0') || ret != 0 ||
      (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4 && r > 32) ||
      (pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV6 && r > 128)) {
    return NULL;
  }
  pfx->mask_len = (uint8_t)r;
  pfx->allowed_matches = BGPSTREAM_PREFIX_MATCH_ANY;

  bgpstream_addr_mask(&pfx->address, pfx->mask_len);

  return pfx;
}
