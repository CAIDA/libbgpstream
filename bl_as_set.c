/*
 * bgp-common
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgp-common.
 *
 * bgp-common is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgp-common is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgp-common.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bl_as_set.h"
#include <stdio.h>
#include "utils.h"
#include <assert.h>
#include "khash.h"

/** set of unique ASes
 *  this structure maintains a set of unique
 *  AS numbers (16/32 bits AS numbers are hashed
 *  using a uint32 type) - ASes could be represented
 *  as an as set or an as confederation.
 */
KHASH_INIT(bl_as_storage_set /* name */, 
	   bl_as_storage_t  /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   bl_as_storage_hash_func /*__hash_func */,  
	   bl_as_storage_hash_equal /* __hash_equal */);

struct bl_as_storage_set_t {
  khash_t(bl_as_storage_set) *hash;
};


bl_as_storage_set_t *bl_as_storage_set_create()
{
  bl_as_storage_set_t *as_set = (bl_as_storage_set_t *) malloc(sizeof(bl_as_storage_set_t));
  as_set->hash = kh_init(bl_as_storage_set);
  return as_set;
}

int bl_as_storage_set_insert(bl_as_storage_set_t *as_set, bl_as_storage_t as)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_as_storage_set, as_set->hash,
			       as)) == kh_end(as_set->hash))
    { 
      k = kh_put(bl_as_storage_set, as_set->hash, as, &khret);
      return 1;
    }
  return 0;
}

void bl_as_storage_set_reset(bl_as_storage_set_t *as_set)
{
  kh_clear(bl_as_storage_set, as_set->hash);
}

int bl_as_storage_set_size(bl_as_storage_set_t *as_set)
{
  return kh_size(as_set->hash);
}

void bl_as_storage_set_merge(bl_as_storage_set_t *union_set, bl_as_storage_set_t *part_set)
{
  bl_as_storage_t *id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = &(kh_key(part_set->hash, k));
	  bl_as_storage_set_insert(union_set, *id);
	}
    }
}

void as_set_destroy(bl_as_storage_set_t *as_set) 
{
  kh_destroy(bl_as_storage_set, as_set->hash);
  free(as_set);
}


