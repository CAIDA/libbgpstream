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

#include <limits.h>
#include <stdint.h>

#include "khash.h"

#include "bgpstream_utils_addr.h"

#if UINT_MAX == 0xffffffffu
unsigned int
#elif ULONG_MAX == 0xffffffffu
unsigned long
#endif
bgpstream_ipv4_addr_hash(bgpstream_ipv4_addr_t *addr)
{
  return __ac_Wang_hash(addr->ipv4.s_addr);
}


#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_ipv6_addr_hash(bgpstream_ipv6_addr_t *addr)
{
  unsigned char *s6 =  &(addr->ipv6.s6_addr[0]);
  return __ac_Wang_hash(*((khint64_t *) s6));
}


#if ULONG_MAX == ULLONG_MAX
unsigned long
#else
unsigned long long
#endif
bgpstream_addr_storage_hash(bgpstream_addr_storage_t *addr)
{
  switch(addr->version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      return bgpstream_ipv4_addr_hash((bgpstream_ipv4_addr_t*)addr);
      break;

    case BGPSTREAM_ADDR_VERSION_IPV6:
      return bgpstream_ipv6_addr_hash((bgpstream_ipv6_addr_t*)addr);
      break;

    default:
      return 0;
      break;
    }
}


int bgpstream_addr_storage_equal(bgpstream_addr_storage_t *addr1,
                                 bgpstream_addr_storage_t *addr2)
{
  if(addr1->version == BGPSTREAM_ADDR_VERSION_IPV4 &&
     addr2->version == BGPSTREAM_ADDR_VERSION_IPV4)
    {
      return bgpstream_ipv4_addr_equal((bgpstream_ipv4_addr_t *)addr1,
                                       (bgpstream_ipv4_addr_t *)addr2);
    }
  if(addr1->version == BGPSTREAM_ADDR_VERSION_IPV6 &&
     addr2->version == BGPSTREAM_ADDR_VERSION_IPV6)
    {
      return bgpstream_ipv6_addr_equal((bgpstream_ipv6_addr_t *)addr1,
                                       (bgpstream_ipv6_addr_t *)addr2);
    }
  return 0;
}


int bgpstream_ipv4_addr_equal(bgpstream_ipv4_addr_t *addr1,
                              bgpstream_ipv4_addr_t *addr2)
{
  return addr1->ipv4.s_addr == addr2->ipv4.s_addr;
}


int bgpstream_ipv6_addr_equal(bgpstream_ipv6_addr_t *addr1,
                              bgpstream_ipv6_addr_t *addr2)
{
  return ((memcmp(&(addr1->ipv6.s6_addr[0]),
                  &(addr2->ipv6.s6_addr[0]),
                  sizeof(uint64_t)) == 0) &&
          (memcmp(&(addr1->ipv6.s6_addr[8]),
                  &(addr2->ipv6.s6_addr[8]),
                  sizeof(uint64_t)) == 0));
}

uint8_t
bgpstream_ipv2idx(bgpstream_addr_version_t v)
{
  switch(v)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      return 0;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      return 1;
    default:
      assert(0);
    }
  return 255;
}

bgpstream_addr_version_t
bgpstream_idx2ipv(uint8_t i)
{
  switch(i)
    {
    case 0:
      return BGPSTREAM_ADDR_VERSION_IPV4;
    case 1:
      return BGPSTREAM_ADDR_VERSION_IPV6;
    default:
      assert(0);
    }
  return BGPSTREAM_ADDR_VERSION_UNKNOWN;

}


uint8_t
bgpstream_ipv2number(bgpstream_addr_version_t v)
{
  switch(v)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      return 4;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      return 6;
    default:
      assert(0);
    }
  return 255;
}


uint8_t
bgpstream_idx2number(uint8_t i)
{
  switch(i)
    {
    case 0:
      return 4;
    case 1:
      return 6;
    default:
      assert(0);
    }
  return 255;
}
