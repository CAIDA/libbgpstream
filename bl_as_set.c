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


bl_as_storage_set_t *bl_as_storage_set_create()
{
  bl_as_storage_set_t *as_set = NULL;
  as_set = kh_init(bl_as_storage_set);
  return as_set;
}

int bl_as_storage_set_insert(bl_as_storage_set_t *as_set, bl_as_storage_t as)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_as_storage_set, as_set,
			       as)) == kh_end(as_set))
    { 
      k = kh_put(bl_as_storage_set, as_set, as, &khret);
      return 1;
    }
  return 0;
}

void bl_as_storage_set_reset(bl_as_storage_set_t *as_set)
{
  kh_clear(bl_as_storage_set, as_set);
}

void as_set_destroy(bl_as_storage_set_t *as_set) 
{
  kh_destroy(bl_as_storage_set, as_set);
}


