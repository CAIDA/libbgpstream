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

#include <stdio.h>
#include <assert.h>

#include "khash.h"
#include "utils.h"

#include "bgpstream_utils_pfx_set.h"
#include "bgpstream_utils_pfx_set_int.h"

bgpstream_pfx_storage_set_t *bgpstream_pfx_storage_set_create() 
{
  bgpstream_pfx_storage_set_t *ip_prefix_set =  (bgpstream_pfx_storage_set_t *) malloc(sizeof(bgpstream_pfx_storage_set_t));
  ip_prefix_set->hash = kh_init(bgpstream_pfx_storage_set);
  return ip_prefix_set;
}

int bgpstream_pfx_storage_set_insert(bgpstream_pfx_storage_set_t *ip_prefix_set, bgpstream_pfx_storage_t prefix) 
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_pfx_storage_set, ip_prefix_set->hash,
			       prefix)) == kh_end(ip_prefix_set->hash))
    {
      k = kh_put(bgpstream_pfx_storage_set, ip_prefix_set->hash, 
		       prefix, &khret);
      return 1;
    }
  return 0;
}

void bgpstream_pfx_storage_set_reset(bgpstream_pfx_storage_set_t *ip_prefix_set)
{
  kh_clear(bgpstream_pfx_storage_set, ip_prefix_set->hash);
}

int bgpstream_pfx_storage_set_size(bgpstream_pfx_storage_set_t *ip_prefix_set)
{
  return kh_size(ip_prefix_set->hash);
}

void bgpstream_pfx_storage_set_merge(bgpstream_pfx_storage_set_t *union_set, bgpstream_pfx_storage_set_t *part_set)
{
  bgpstream_pfx_storage_t *id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = &(kh_key(part_set->hash, k));
	  bgpstream_pfx_storage_set_insert(union_set, *id);
	}
    }
}

void bgpstream_pfx_storage_set_destroy(bgpstream_pfx_storage_set_t *ip_prefix_set)
{
  kh_destroy(bgpstream_pfx_storage_set, ip_prefix_set->hash);
  free(ip_prefix_set);
}


/* ipv4 specific set */

bgpstream_ipv4_pfx_set_t *bgpstream_ipv4_pfx_set_create() 
{
  bgpstream_ipv4_pfx_set_t *ip_prefix_set =  (bgpstream_ipv4_pfx_set_t *) malloc(sizeof(bgpstream_ipv4_pfx_set_t));
  ip_prefix_set->hash = kh_init(bgpstream_ipv4_pfx_set);
  return ip_prefix_set;
}

int bgpstream_ipv4_pfx_set_insert(bgpstream_ipv4_pfx_set_t *ip_prefix_set, bgpstream_ipv4_pfx_t prefix) 
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_ipv4_pfx_set, ip_prefix_set->hash,
			       prefix)) == kh_end(ip_prefix_set->hash))
    {
      k = kh_put(bgpstream_ipv4_pfx_set, ip_prefix_set->hash, 
		       prefix, &khret);
      return 1;
    }
  return 0;
}

void bgpstream_ipv4_pfx_set_reset(bgpstream_ipv4_pfx_set_t *ip_prefix_set)
{
  kh_clear(bgpstream_ipv4_pfx_set, ip_prefix_set->hash);
}

int bgpstream_ipv4_pfx_set_size(bgpstream_ipv4_pfx_set_t *ip_prefix_set)
{
  return kh_size(ip_prefix_set->hash);
}

void bgpstream_ipv4_pfx_set_merge(bgpstream_ipv4_pfx_set_t *union_set, bgpstream_ipv4_pfx_set_t *part_set)
{
  bgpstream_ipv4_pfx_t *id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = &(kh_key(part_set->hash, k));
	  bgpstream_ipv4_pfx_set_insert(union_set, *id);
	}
    }
}


void bgpstream_ipv4_pfx_set_destroy(bgpstream_ipv4_pfx_set_t *ip_prefix_set)
{
  kh_destroy(bgpstream_ipv4_pfx_set, ip_prefix_set->hash);
  free(ip_prefix_set);
}


/* ipv6 specific set */


bgpstream_ipv6_pfx_set_t *bgpstream_ipv6_pfx_set_create() 
{
  bgpstream_ipv6_pfx_set_t *ip_prefix_set =  (bgpstream_ipv6_pfx_set_t *) malloc(sizeof(bgpstream_ipv6_pfx_set_t));
  ip_prefix_set->hash = kh_init(bgpstream_ipv6_pfx_set);
  return ip_prefix_set;
}

int bgpstream_ipv6_pfx_set_insert(bgpstream_ipv6_pfx_set_t *ip_prefix_set, bgpstream_ipv6_pfx_t prefix) 
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_ipv6_pfx_set, ip_prefix_set->hash,
			       prefix)) == kh_end(ip_prefix_set->hash))
    {
      k = kh_put(bgpstream_ipv6_pfx_set, ip_prefix_set->hash, 
		       prefix, &khret);
      return 1;
    }
  return 0;
}

void bgpstream_ipv6_pfx_set_reset(bgpstream_ipv6_pfx_set_t *ip_prefix_set)
{
  kh_clear(bgpstream_ipv6_pfx_set, ip_prefix_set->hash);
}

int bgpstream_ipv6_pfx_set_size(bgpstream_ipv6_pfx_set_t *ip_prefix_set)
{
  return kh_size(ip_prefix_set->hash);
}

void bgpstream_ipv6_pfx_set_merge(bgpstream_ipv6_pfx_set_t *union_set, bgpstream_ipv6_pfx_set_t *part_set)
{
  bgpstream_ipv6_pfx_t *id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = &(kh_key(part_set->hash, k));
	  bgpstream_ipv6_pfx_set_insert(union_set, *id);
	}
    }
}

void bgpstream_ipv6_pfx_set_destroy(bgpstream_ipv6_pfx_set_t *ip_prefix_set)
{
  kh_destroy(bgpstream_ipv6_pfx_set, ip_prefix_set->hash);
  free(ip_prefix_set);
}


