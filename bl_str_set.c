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

#include "bl_str_set.h"
#include <stdio.h>
#include "utils.h"
#include <assert.h>



bl_string_set_t *bl_string_set_create()
{
  bl_string_set_t *string_set = NULL;
  string_set = kh_init(bl_string_set);
  return string_set;
}


int bl_string_set_insert(bl_string_set_t *string_set, char * string_val)
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_string_set, string_set, string_val)) == kh_end(string_set))
    {
      k = kh_put(bl_string_set, string_set, strdup(string_val), &khret);
      return 1;
    }
  return 0;
}


int bl_string_set_remove(bl_string_set_t *string_set, char * string_val)
{
  khiter_t k;
  if((k = kh_get(bl_string_set, string_set, string_val)) != kh_end(string_set))
    {
      // free memory allocated for the key (string)
      free(kh_key(string_set,k));
      // delete entry
      kh_del(bl_string_set, string_set, k);
      return 1;
    }
  return 0;
}


int bl_string_set_exists(bl_string_set_t *string_set, char * string_val)
{
  khiter_t k;
  if((k = kh_get(bl_string_set, string_set, string_val)) == kh_end(string_set))
    {
      return 0;
    }
  return 1;
}


int bl_string_set_size(bl_string_set_t *string_set)
{
  return kh_size(string_set);
}


void bl_string_set_reset(bl_string_set_t *string_set)
{
  khiter_t k;
  for (k = kh_begin(string_set); k != kh_end(string_set); ++k)
    {
      if (kh_exist(string_set, k))
	{
	  free(kh_key(string_set,k));
	}
    }
  kh_clear(bl_string_set, string_set);
}


void bl_string_set_destroy(bl_string_set_t *string_set)
{
  khiter_t k;
  for (k = kh_begin(string_set); k != kh_end(string_set); ++k)
    {
      if (kh_exist(string_set, k))
	{
	  free(kh_key(string_set,k));
	}
    }
  kh_destroy(bl_string_set, string_set);
}

