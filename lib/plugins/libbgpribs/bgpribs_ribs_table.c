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

#include "bgpribs_ribs_table.h"

ribs_table_t *ribs_table_create() 
{
  ribs_table_t *ribs_table;
  if((ribs_table = malloc_zero(sizeof(ribs_table_t))) == NULL)
    {
      return NULL;
    }
  // init ipv4 and ipv6 khashes
  ribs_table->ipv4_rib = kh_init(ipv4_rib_t);
  ribs_table->ipv6_rib = kh_init(ipv6_rib_t);
  return ribs_table;
}

void ribs_table_apply_elem(ribs_table_t *ribs_table, bgpstream_elem_t *bs_elem)
{
  khiter_t k;
  int khret;

  // prepare pd in case of insert
  prefixdata_t pd;
  pd.origin_as = 0;
  pd.is_active = 0; // if it is a withdrawal it will remain 0
  pd.ts = bs_elem->timestamp;
  prefixdata_t current_pd;

  if(bs_elem->type == BST_ANNOUNCEMENT || bs_elem->type == BST_RIB)
    {
      pd.is_active = 1;
      pd.aspath = bs_elem->aspath;
      // compute origin_as
      if(bs_elem->aspath.hop_count > 0 && 
	 bs_elem->aspath.type == BST_UINT32_ASPATH ) 
	{
	  pd.origin_as = bs_elem->aspath.numeric_aspath[(bs_elem->aspath.hop_count-1)];
	}  
    } 

  if(bs_elem->prefix.number.type == BST_IPV4) 
    { // ipv4 prefix
      k = kh_get(ipv4_rib_t, ribs_table->ipv4_rib,
		 bs_elem->prefix);
      // if it doesn't exist
      if(k == kh_end(ribs_table->ipv4_rib))
	{
	  k = kh_put(ipv4_rib_t, ribs_table->ipv4_rib, 
		     bs_elem->prefix, &khret);
	  kh_value(ribs_table->ipv4_rib, k) = pd;
	}
      else
	{
	  // updating the value only if the new timestamp is >= than the current one
	  // (if it is equal we assume data arrives in order and we apply it)
	  current_pd = kh_value(ribs_table->ipv4_rib, k);
	  if(pd.ts >= current_pd.ts)
	    {
	      kh_value(ribs_table->ipv4_rib, k) = pd;
	    }
	}
    }
  else
    { // ipv6 prefix  // assert(bs_elem->prefix.number.type == BST_IPV6) 
      k = kh_get(ipv6_rib_t, ribs_table->ipv6_rib,
		 bs_elem->prefix);
      // if it doesn't exist
      if(k == kh_end(ribs_table->ipv6_rib))
	{
	  k = kh_put(ipv6_rib_t, ribs_table->ipv6_rib, 
		     bs_elem->prefix, &khret);
	  kh_value(ribs_table->ipv6_rib, k) = pd;
	}
      else
	{
	  // updating the value only if the new timestamp is >= than the current one
	  // (if it is equal we assume data arrives in order and we apply it)
	  current_pd = kh_value(ribs_table->ipv6_rib, k);
	  if(pd.ts >= current_pd.ts)
	    {
	      kh_value(ribs_table->ipv6_rib, k) = pd;
	    }
	}
    }
  return;	
}


void ribs_table_reset(ribs_table_t *ribs_table)
{
  ribs_table->reference_rib_start = 0;
  ribs_table->reference_rib_end = 0;
  ribs_table->reference_dump_time = 0;
  // ipv4_rib_t has static keys and values
  kh_clear(ipv4_rib_t, ribs_table->ipv4_rib);
  // ipv6_rib_t has static keys and values
  kh_clear(ipv6_rib_t,ribs_table->ipv6_rib);    
}


void ribs_table_destroy(ribs_table_t *ribs_table) 
{
  if(ribs_table != NULL) 
    {
      kh_destroy(ipv4_rib_t, ribs_table->ipv4_rib);
      kh_destroy(ipv6_rib_t, ribs_table->ipv6_rib);
      // free ribs_table
      free(ribs_table);
    }
}

