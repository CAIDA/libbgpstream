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

#include "bgpribs_prefixes_table.h"


prefixes_table_t *prefixes_table_create() 
{
  prefixes_table_t *prefixes_table;
  if((prefixes_table = malloc_zero(sizeof(prefixes_table_t))) == NULL)
    {
      return NULL;
    }
  // init ipv4 and ipv6 khashes
  prefixes_table->ipv4_prefixes_table = bl_ipv4_pfx_set_create();
  prefixes_table->ipv6_prefixes_table = bl_ipv6_pfx_set_create();
  return prefixes_table;
}


void prefixes_table_insert_ipv4(prefixes_table_t *prefixes_table, bl_ipv4_pfx_t *prefix)
{
  bl_ipv4_pfx_set_insert(prefixes_table->ipv4_prefixes_table, *prefix);  
}

void prefixes_table_insert_ipv6(prefixes_table_t *prefixes_table, bl_ipv6_pfx_t *prefix)
{
  bl_ipv6_pfx_set_insert(prefixes_table->ipv6_prefixes_table, *prefix);  
}

void prefixes_table_reset(prefixes_table_t *prefixes_table) 
{
  assert(prefixes_table);
  bl_ipv4_pfx_set_reset(prefixes_table->ipv4_prefixes_table);
  bl_ipv6_pfx_set_reset(prefixes_table->ipv6_prefixes_table);
}

void prefixes_table_destroy(prefixes_table_t *prefixes_table) 
{
  if(prefixes_table == NULL) 
    {
      bl_ipv4_pfx_set_destroy(prefixes_table->ipv4_prefixes_table);
      bl_ipv6_pfx_set_destroy(prefixes_table->ipv6_prefixes_table);
      // free prefixes_table
      free(prefixes_table);
    }
}


