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

#include "bgpstream_utils_str_set.h"

/** set of unique strings
 *  this structure maintains a set of strings
 */

KHASH_INIT(bl_string_set, char*, char, 0,
	   kh_str_hash_func, kh_str_hash_equal);


struct bl_string_set_t {
  khash_t(bl_string_set) *hash;
};


bl_string_set_t *bl_string_set_create()
{
  bl_string_set_t *string_set = (bl_string_set_t *) malloc(sizeof(bl_string_set_t));
  string_set->hash = kh_init(bl_string_set);
  return string_set;
}


int bl_string_set_insert(bl_string_set_t *string_set, char * string_val)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_string_set, string_set->hash, string_val)) == kh_end(string_set->hash))
    {
      k = kh_put(bl_string_set, string_set->hash, strdup(string_val), &khret);
      return 1;
    }
  return 0;
}


int bl_string_set_remove(bl_string_set_t *string_set, char * string_val)
{
  khiter_t k;
  if((k = kh_get(bl_string_set, string_set->hash, string_val)) != kh_end(string_set->hash))
    {
      // free memory allocated for the key (string)
      free(kh_key(string_set->hash,k));
      // delete entry
      kh_del(bl_string_set, string_set->hash, k);
      return 1;
    }
  return 0;
}


int bl_string_set_exists(bl_string_set_t *string_set, char * string_val)
{
  khiter_t k;
  if((k = kh_get(bl_string_set, string_set->hash, string_val)) == kh_end(string_set->hash))
    {
      return 0;
    }
  return 1;
}


int bl_string_set_size(bl_string_set_t *string_set)
{
  return kh_size(string_set->hash);
}

void bl_string_set_merge(bl_string_set_t *union_set, bl_string_set_t *part_set)
{
  char *id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = kh_key(part_set->hash, k);
	  bl_string_set_insert(union_set, id);
	}
    }
}


void bl_string_set_reset(bl_string_set_t *string_set)
{
  khiter_t k;
  for (k = kh_begin(string_set->hash); k != kh_end(string_set->hash); ++k)
    {
      if (kh_exist(string_set->hash, k))
	{
	  free(kh_key(string_set->hash,k));
	}
    }
  kh_clear(bl_string_set, string_set->hash);
}


void bl_string_set_destroy(bl_string_set_t *string_set)
{
  khiter_t k;
  for (k = kh_begin(string_set->hash); k != kh_end(string_set->hash); ++k)
    {
      if (kh_exist(string_set->hash, k))
	{
	  free(kh_key(string_set->hash,k));
	}
    }
  kh_destroy(bl_string_set, string_set->hash);
  free(string_set);
}

