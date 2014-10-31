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


bl_id_set_t *bl_id_set_create()
{
  bl_id_set_t *id_set = NULL;
  id_set = kh_init(bl_id_set);
  return id_set;
}

int bl_id_set_insert(bl_id_set_t *id_set,  uint32_t id)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_id_set, id_set,
			       id)) == kh_end(id_set))
    { 
      k = kh_put(bl_id_set, id_set, id, &khret);
      return 1;
    }
  return 0;
}

int bl_id_set_exists(bl_id_set_t *id_set,  uint32_t id)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_id_set, id_set,
			       id)) == kh_end(id_set))
    { 
      return 0;
    }
  return 1;
}

void bl_id_set_reset(bl_id_set_t *id_set)
{
  kh_clear(bl_id_set, id_set);
}

void bl_id_set_destroy(bl_id_set_t *id_set) 
{
  kh_destroy(bl_id_set, id_set);
}


