/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bgpribs_ases_table.h"


ases_table_wrapper_t *ases_table_create() 
{
  ases_table_wrapper_t *ases_table;
  if((ases_table = malloc_zero(sizeof(ases_table_wrapper_t))) == NULL)
    {
      return NULL;
    }
  // init khash
  ases_table->table = kh_init(ases_table_t);
  return ases_table;
}

void ases_table_insert(ases_table_wrapper_t *ases_table, uint32_t as) 
{
  assert(ases_table); 
  int khret;
  khiter_t k;
  if((k = kh_get(ases_table_t, ases_table->table,
			       as)) == kh_end(ases_table->table))
    {
      k = kh_put(ases_table_t, ases_table->table, 
		       as, &khret);
    }
}

void ases_table_reset(ases_table_wrapper_t *ases_table) 
{
  assert(ases_table); 
  kh_clear(ases_table_t, ases_table->table);
}

void ases_table_destroy(ases_table_wrapper_t *ases_table) 
{
  if(ases_table != NULL) 
    {
      kh_destroy(ases_table_t, ases_table->table);
      // free prefixes_table
      free(ases_table);
    }
}

