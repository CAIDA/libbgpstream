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

#include <bgpstream_utils_pfx_set_int.h>

/* STORAGE */

bgpstream_pfx_storage_set_t *bgpstream_pfx_storage_set_create()
{
  bgpstream_pfx_storage_set_t *set;

  if((set =
      (bgpstream_pfx_storage_set_t*)
      malloc(sizeof(bgpstream_pfx_storage_set_t))) == NULL)
    {
      return NULL;
    }

  if((set->hash = kh_init(bgpstream_pfx_storage_set)) == NULL)
    {
      bgpstream_pfx_storage_set_destroy(set);
      return NULL;
    }

  return set;
}

int bgpstream_pfx_storage_set_insert(bgpstream_pfx_storage_set_t *set,
                                      bgpstream_pfx_storage_t *pfx)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_pfx_storage_set, set->hash, *pfx)) ==
     kh_end(set->hash))
    {
      k = kh_put(bgpstream_pfx_storage_set, set->hash, *pfx, &khret);
      return 1;
    }
  return 0;
}

int bgpstream_pfx_storage_set_size(bgpstream_pfx_storage_set_t *set)
{
  return kh_size(set->hash);
}

int bgpstream_pfx_storage_set_merge(bgpstream_pfx_storage_set_t *dst_set,
                                     bgpstream_pfx_storage_set_t *src_set)
{
  khiter_t k;
  for(k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k)
    {
      if(kh_exist(src_set->hash, k))
	{
	  if(bgpstream_pfx_storage_set_insert(dst_set,
                                               &(kh_key(src_set->hash, k))) <0)
            {
              return -1;
            }
	}
    }
  return 0;
}

void bgpstream_pfx_storage_set_destroy(bgpstream_pfx_storage_set_t *set)
{
  kh_destroy(bgpstream_pfx_storage_set, set->hash);
  free(set);
}

void bgpstream_pfx_storage_set_clear(bgpstream_pfx_storage_set_t *set)
{
  kh_clear(bgpstream_pfx_storage_set, set->hash);
}



/* IPv4 */

bgpstream_ipv4_pfx_set_t *bgpstream_ipv4_pfx_set_create()
{
  bgpstream_ipv4_pfx_set_t *set;

  if((set =
      (bgpstream_ipv4_pfx_set_t*)
      malloc(sizeof(bgpstream_ipv4_pfx_set_t))) == NULL)
    {
      return NULL;
    }

  if((set->hash = kh_init(bgpstream_ipv4_pfx_set)) == NULL)
    {
      bgpstream_ipv4_pfx_set_destroy(set);
      return NULL;
    }

  return set;
}

int bgpstream_ipv4_pfx_set_insert(bgpstream_ipv4_pfx_set_t *set,
                                      bgpstream_ipv4_pfx_t *pfx)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_ipv4_pfx_set, set->hash, *pfx)) ==
     kh_end(set->hash))
    {
      k = kh_put(bgpstream_ipv4_pfx_set, set->hash, *pfx, &khret);
      return 1;
    }
  return 0;
}

int bgpstream_ipv4_pfx_set_size(bgpstream_ipv4_pfx_set_t *set)
{
  return kh_size(set->hash);
}

int bgpstream_ipv4_pfx_set_merge(bgpstream_ipv4_pfx_set_t *dst_set,
                                     bgpstream_ipv4_pfx_set_t *src_set)
{
  khiter_t k;
  for(k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k)
    {
      if(kh_exist(src_set->hash, k))
	{
	  if(bgpstream_ipv4_pfx_set_insert(dst_set,
                                               &(kh_key(src_set->hash, k))) <0)
            {
              return -1;
            }
	}
    }
  return 0;
}

void bgpstream_ipv4_pfx_set_destroy(bgpstream_ipv4_pfx_set_t *set)
{
  kh_destroy(bgpstream_ipv4_pfx_set, set->hash);
  free(set);
}

void bgpstream_ipv4_pfx_set_clear(bgpstream_ipv4_pfx_set_t *set)
{
  kh_clear(bgpstream_ipv4_pfx_set, set->hash);
}



/* IPv6 */

bgpstream_ipv6_pfx_set_t *bgpstream_ipv6_pfx_set_create()
{
  bgpstream_ipv6_pfx_set_t *set;

  if((set =
      (bgpstream_ipv6_pfx_set_t*)
      malloc(sizeof(bgpstream_ipv6_pfx_set_t))) == NULL)
    {
      return NULL;
    }

  if((set->hash = kh_init(bgpstream_ipv6_pfx_set)) == NULL)
    {
      bgpstream_ipv6_pfx_set_destroy(set);
      return NULL;
    }

  return set;
}

int bgpstream_ipv6_pfx_set_insert(bgpstream_ipv6_pfx_set_t *set,
                                      bgpstream_ipv6_pfx_t *pfx)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bgpstream_ipv6_pfx_set, set->hash, *pfx)) ==
     kh_end(set->hash))
    {
      k = kh_put(bgpstream_ipv6_pfx_set, set->hash, *pfx, &khret);
      return 1;
    }
  return 0;
}

int bgpstream_ipv6_pfx_set_size(bgpstream_ipv6_pfx_set_t *set)
{
  return kh_size(set->hash);
}

int bgpstream_ipv6_pfx_set_merge(bgpstream_ipv6_pfx_set_t *dst_set,
                                     bgpstream_ipv6_pfx_set_t *src_set)
{
  khiter_t k;
  for(k = kh_begin(src_set->hash); k != kh_end(src_set->hash); ++k)
    {
      if(kh_exist(src_set->hash, k))
	{
	  if(bgpstream_ipv6_pfx_set_insert(dst_set,
                                               &(kh_key(src_set->hash, k))) <0)
            {
              return -1;
            }
	}
    }
  return 0;
}

void bgpstream_ipv6_pfx_set_destroy(bgpstream_ipv6_pfx_set_t *set)
{
  kh_destroy(bgpstream_ipv6_pfx_set, set->hash);
  free(set);
}

void bgpstream_ipv6_pfx_set_clear(bgpstream_ipv6_pfx_set_t *set)
{
  kh_clear(bgpstream_ipv6_pfx_set, set->hash);
}
