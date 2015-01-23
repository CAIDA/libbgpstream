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

#include "bl_id_set.h"
#include <stdio.h>
#include "utils.h"
#include <assert.h>
#include "khash.h"


/** set of unique ids
 *  this structure maintains a set of unique
 *  ids (using a uint32 type)
 */
KHASH_INIT(bl_id_set /* name */, 
	   uint32_t  /* khkey_t */, 
	   char /* khval_t */, 
	   0  /* kh_is_set */, 
	   kh_int_hash_func /*__hash_func */,  
	   kh_int_hash_equal /* __hash_equal */);


struct bl_id_set_t {
  khash_t(bl_id_set) *hash;
};

			   
bl_id_set_t *bl_id_set_create()
{
  bl_id_set_t *id_set =  (bl_id_set_t *) malloc(sizeof(bl_id_set_t));
  id_set->hash = kh_init(bl_id_set);
  return id_set;
}

int bl_id_set_insert(bl_id_set_t *id_set,  uint32_t id)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_id_set, id_set->hash,
			       id)) == kh_end(id_set->hash))
    { 
      k = kh_put(bl_id_set, id_set->hash, id, &khret);
      return 1;
    }
  return 0;
}

int bl_id_set_exists(bl_id_set_t *id_set,  uint32_t id)
{
  khiter_t k;
  if((k = kh_get(bl_id_set, id_set->hash,
			       id)) == kh_end(id_set->hash))
    { 
      return 0;
    }
  return 1;
}

void bl_id_set_reset(bl_id_set_t *id_set)
{
  kh_clear(bl_id_set, id_set->hash);
}


void bl_id_set_merge(bl_id_set_t *union_set, bl_id_set_t *part_set)
{
  uint32_t id;
  khiter_t k;
  for(k = kh_begin(part_set->hash);
      k != kh_end(part_set->hash); ++k)
    {
      if (kh_exist(part_set->hash, k))
	{
	  id = kh_key(part_set->hash, k);
	  bl_id_set_insert(union_set, id);
	}
    }
}

int bl_id_set_size(bl_id_set_t *id_set)
{
  return kh_size(id_set->hash);
}

void bl_id_set_destroy(bl_id_set_t *id_set) 
{
  kh_destroy(bl_id_set, id_set->hash);
  free(id_set);
}


