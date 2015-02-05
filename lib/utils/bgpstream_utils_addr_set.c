/*
 * bgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpstream.
 *
 * bgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <assert.h>
#include <stdio.h>

#include "khash.h"
#include "utils.h"

#include "bgpstream_utils_addr_set.h"

#define STORAGE_HASH_VAL(arg) bgpstream_addr_storage_hash(&(arg))
#define STORAGE_EQUAL_VAL(arg1, arg2) \
  bgpstream_addr_storage_equal(&(arg1), &(arg2))

#define V4_HASH_VAL(arg) bgpstream_ipv4_addr_hash(&(arg))
#define V4_EQUAL_VAL(arg1, arg2) \
  bgpstream_ipv4_addr_equal(&(arg1), &(arg2))

#define V6_HASH_VAL(arg) bgpstream_ipv6_addr_hash(&(arg))
#define V6_EQUAL_VAL(arg1, arg2) \
  bgpstream_ipv6_addr_equal(&(arg1), &(arg2))

/** set of unique IP addresses
 *  this structure maintains a set of unique
 *  addresses (ipv4 and ipv6 addresses, both hashed
 *  using a int64 type)
 */
KHASH_INIT(bgpstream_addr_storage_set /* name */,
	   bgpstream_addr_storage_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   STORAGE_HASH_VAL /*__hash_func */,
	   STORAGE_EQUAL_VAL /* __hash_equal */);


struct bgpstream_addr_storage_set_t {
  khash_t(bgpstream_addr_storage_set) *hash;
};


bgpstream_addr_storage_set_t *bgpstream_addr_storage_set_create()
{
  bgpstream_addr_storage_set_t *ip_address_set = (bgpstream_addr_storage_set_t *) malloc(sizeof(bgpstream_addr_storage_set_t));
  ip_address_set->hash = kh_init(bgpstream_addr_storage_set);
  return ip_address_set;
}

int bgpstream_addr_storage_set_insert(bgpstream_addr_storage_set_t *ip_address_set, bgpstream_addr_storage_t ip_address)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_addr_storage_set, ip_address_set->hash,
		 ip_address)) == kh_end(ip_address_set->hash))
    {
      k = kh_put(bgpstream_addr_storage_set, ip_address_set->hash,
		 ip_address, &khret);
      return 1;
    }
  return 0;
}

void bgpstream_addr_storage_set_reset(bgpstream_addr_storage_set_t *ip_address_set)
{
  kh_clear(bgpstream_addr_storage_set, ip_address_set->hash);
}

int bgpstream_addr_storage_set_size(bgpstream_addr_storage_set_t *ip_address_set)
{
  return kh_size(ip_address_set->hash);
}

void bgpstream_addr_storage_set_merge(bgpstream_addr_storage_set_t *union_set, bgpstream_addr_storage_set_t *part_set)
{
  bgpstream_addr_storage_t *id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = &(kh_key(part_set->hash, k));
	  bgpstream_addr_storage_set_insert(union_set, *id);
	}
    }
}

void bgpstream_addr_storage_set_destroy(bgpstream_addr_storage_set_t *ip_address_set)
{
  kh_destroy(bgpstream_addr_storage_set, ip_address_set->hash);
  free(ip_address_set);
}



// ipv4 specific functions


// same functions, ipv4 specific

KHASH_INIT(bgpstream_ipv4_addr_set /* name */,
	   bgpstream_ipv4_addr_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   V4_HASH_VAL /*__hash_func */,
	   V4_EQUAL_VAL /* __hash_equal */);

struct bgpstream_ipv4_addr_set_t {
  khash_t(bgpstream_ipv4_addr_set) *hash;
};


bgpstream_ipv4_addr_set_t *bgpstream_ipv4_addr_set_create()
{
  bgpstream_ipv4_addr_set_t *ip_address_set = (bgpstream_ipv4_addr_set_t *) malloc(sizeof(bgpstream_ipv4_addr_set_t));
  ip_address_set->hash = kh_init(bgpstream_ipv4_addr_set);
  return ip_address_set;
}

int bgpstream_ipv4_addr_set_insert(bgpstream_ipv4_addr_set_t *ip_address_set, bgpstream_ipv4_addr_t ip_address)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_ipv4_addr_set, ip_address_set->hash,
		 ip_address)) == kh_end(ip_address_set->hash))
    {
      k = kh_put(bgpstream_ipv4_addr_set, ip_address_set->hash,
		 ip_address, &khret);
      return 1;
    }
  return 0;
}

void bgpstream_ipv4_addr_set_reset(bgpstream_ipv4_addr_set_t *ip_address_set)
{
  kh_clear(bgpstream_ipv4_addr_set, ip_address_set->hash);
}

int bgpstream_ipv4_addr_set_size(bgpstream_ipv4_addr_set_t *ip_address_set)
{
  return kh_size(ip_address_set->hash);
}

void bgpstream_ipv4_addr_set_merge(bgpstream_ipv4_addr_set_t *union_set, bgpstream_ipv4_addr_set_t *part_set)
{
  bgpstream_ipv4_addr_t *id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = &(kh_key(part_set->hash, k));
	  bgpstream_ipv4_addr_set_insert(union_set, *id);
	}
    }
}

void bgpstream_ipv4_addr_set_destroy(bgpstream_ipv4_addr_set_t *ip_address_set)
{
  kh_destroy(bgpstream_ipv4_addr_set, ip_address_set->hash);
  free(ip_address_set);
}


// ipv6 specific functions

KHASH_INIT(bgpstream_ipv6_addr_set /* name */,
	   bgpstream_ipv6_addr_t /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   V6_HASH_VAL /*__hash_func */,
	   V6_EQUAL_VAL /* __hash_equal */);

struct bgpstream_ipv6_addr_set_t {
  khash_t(bgpstream_ipv6_addr_set) *hash;
};


bgpstream_ipv6_addr_set_t *bgpstream_ipv6_addr_set_create()
{
  bgpstream_ipv6_addr_set_t *ip_address_set = (bgpstream_ipv6_addr_set_t *) malloc(sizeof(bgpstream_ipv6_addr_set_t));
  ip_address_set->hash = kh_init(bgpstream_ipv6_addr_set);
  return ip_address_set;
}

int bgpstream_ipv6_addr_set_insert(bgpstream_ipv6_addr_set_t *ip_address_set, bgpstream_ipv6_addr_t ip_address)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_ipv6_addr_set, ip_address_set->hash,
		 ip_address)) == kh_end(ip_address_set->hash))
    {
      k = kh_put(bgpstream_ipv6_addr_set, ip_address_set->hash,
		 ip_address, &khret);
      return 1;
    }
  return 0;
}

void bgpstream_ipv6_addr_set_reset(bgpstream_ipv6_addr_set_t *ip_address_set)
{
  kh_clear(bgpstream_ipv6_addr_set, ip_address_set->hash);
}

int bgpstream_ipv6_addr_set_size(bgpstream_ipv6_addr_set_t *ip_address_set)
{
  return kh_size(ip_address_set->hash);
}

void bgpstream_ipv6_addr_set_merge(bgpstream_ipv6_addr_set_t *union_set, bgpstream_ipv6_addr_set_t *part_set)
{
  bgpstream_ipv6_addr_t *id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = &(kh_key(part_set->hash, k));
	  bgpstream_ipv6_addr_set_insert(union_set, *id);
	}
    }
}

void bgpstream_ipv6_addr_set_destroy(bgpstream_ipv6_addr_set_t *ip_address_set)
{
  kh_destroy(bgpstream_ipv6_addr_set, ip_address_set->hash);
  free(ip_address_set);
}
