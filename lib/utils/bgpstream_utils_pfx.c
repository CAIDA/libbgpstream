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

#include <inttypes.h>
#include <stdio.h>

#include "khash.h"

#include <bgpstream_utils_pfx.h>

char *bgpstream_pfx_snprintf(char *buf, size_t len, bgpstream_pfx_t *pfx)
{
  char *p = buf;

  /* print the address */
  if(bgpstream_addr_ntop(buf, len, &(pfx->address)) == NULL)
    {
      return NULL;
    }

  while(*p != '\0')
    {
      p++;
      len--;
    }

  /* print the mask */
  snprintf(p, len, "/%"PRIu8, pfx->mask_len);

  return buf;
}


#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_ipv4_pfx_hash(bgpstream_ipv4_pfx_t *pfx)
{
  // embed the network mask length in the 32 bits
  return __ac_Wang_hash(pfx->address.ipv4.s_addr | (uint32_t)pfx->mask_len);
}


#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_ipv6_pfx_hash(bgpstream_ipv6_pfx_t *pfx)
{
  // ipv6 number - we take most significant 64 bits only
  unsigned char *s6 =  &(pfx->address.ipv6.s6_addr[0]);
  uint64_t address = *((uint64_t *) s6);
  // embed the network mask length in the 64 bits
  return __ac_Wang_hash(address | (uint64_t)pfx->mask_len);
}


#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_pfx_storage_hash(bgpstream_pfx_storage_t *pfx)
{
  if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4)
    {
      return bgpstream_ipv4_pfx_hash((bgpstream_ipv4_pfx_t *)pfx);
    }
  if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV6)
    {
      return bgpstream_ipv6_pfx_hash((bgpstream_ipv6_pfx_t *)pfx);
    }
  return 0;
}


int bgpstream_ipv4_pfx_equal(bgpstream_ipv4_pfx_t *pfx1,
                             bgpstream_ipv4_pfx_t *pfx2)
{
  return ((pfx1->mask_len == pfx2->mask_len) &&
          bgpstream_ipv4_addr_equal(&pfx1->address, &pfx2->address));
}


int bgpstream_ipv6_pfx_equal(bgpstream_ipv6_pfx_t *pfx1,
                             bgpstream_ipv6_pfx_t *pfx2)
{
  // note: it could be faster to use a loop when inserting differing prefixes

  return ((pfx1->mask_len == pfx2->mask_len) &&
          bgpstream_ipv6_addr_equal(&pfx1->address, &pfx2->address));
}


int bgpstream_pfx_storage_equal(bgpstream_pfx_storage_t *pfx1,
                                bgpstream_pfx_storage_t *pfx2)
{
  if(pfx1->mask_len == pfx2->mask_len)
    {
      return bgpstream_addr_storage_equal(&pfx1->address, &pfx2->address);
    }
  return 0;
}
