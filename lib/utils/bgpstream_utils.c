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
#include <inttypes.h>
#include <stdio.h>


#include "khash.h"
#include "utils.h"

#include "bgpstream_utils.h"

/** Print functions */

char *bl_print_as(bl_as_storage_t *as)
{
  if(as->type == BL_AS_NUMERIC)
    {
      char as_str[16];
      sprintf(as_str, "%"PRIu32, as->as_number);
      return strdup(as_str);
    }
  if(as->type == BL_AS_STRING)
    {
      return strdup(as->as_string);
    }
    return "";
}


char *bl_print_aspath(bl_aspath_storage_t *aspath)
{
    if(aspath->type == BL_AS_NUMERIC && aspath->hop_count > 0)
      {
	char *as_path_str = NULL;
	char as[10];
	int i;
	// assuming 10 char per as number
	as_path_str = (char *)malloc_zero(sizeof(char) * (aspath->hop_count * 10 + 1));
	as_path_str[0] = '\0';
	sprintf(as, "%"PRIu32, aspath->numeric_aspath[0]);
	strcat(as_path_str, as);	
	for(i = 1; i < aspath->hop_count; i++)
	  {
	    sprintf(as, " %"PRIu32, aspath->numeric_aspath[i]);
	    strcat(as_path_str, as);
	  }
	return as_path_str;
      }    
    if(aspath->type == BL_AS_STRING)
      {
	return strdup(aspath->str_aspath);	      
      }
    return strdup("");
}

/* as-path utility functions */

bl_as_storage_t bl_get_origin_as(bl_aspath_storage_t *aspath)
{
  bl_as_storage_t origin_as;
  origin_as.type = BL_AS_NUMERIC;
  origin_as.as_number = 0;
  char *path_copy;
  char *as_hop;
  if(aspath->hop_count > 0)
    {
      if(aspath->type == BL_AS_NUMERIC)
	{
	  origin_as.as_number = aspath->numeric_aspath[aspath->hop_count-1];	
	}
      if(aspath->type == BL_AS_STRING)
	{ 
	  origin_as.type = BL_AS_STRING;
	  origin_as.as_string = strdup(aspath->str_aspath);
	  path_copy = strdup(aspath->str_aspath);
	  while((as_hop = strsep(&path_copy, " ")) != NULL) {    
	    free(origin_as.as_string);
	    origin_as.as_string = strdup(as_hop);
	  }
	}
    }
  return origin_as;
}


bl_as_storage_t bl_copy_origin_as(bl_as_storage_t *as)
{
  bl_as_storage_t copy;
  copy.type = as->type;
  if(copy.type == BL_AS_NUMERIC)
    {
      copy.as_number = as->as_number;
    }
  if(copy.type == BL_AS_STRING)
    {
      copy.as_string = strdup(as->as_string);
    }
  return copy;
}

void bl_origin_as_freedynmem(bl_as_storage_t *as)
{
  if(as->type == BL_AS_STRING)
    {
      free(as->as_string);
      as->as_string = NULL;
      as->type = BL_AS_TYPE_UNKNOWN;
    }
}


bl_aspath_storage_t bl_copy_aspath(bl_aspath_storage_t *aspath)
{
  bl_aspath_storage_t copy;
  copy.type = aspath->type;
  copy.hop_count = aspath->hop_count;
  if(copy.type == BL_AS_NUMERIC && copy.hop_count > 0)
    {
      int i;
      copy.numeric_aspath = (uint32_t *)malloc(copy.hop_count * sizeof(uint32_t));
      for(i=0; i < copy.hop_count; i++)
	{
	  copy.numeric_aspath[i] = aspath->numeric_aspath[i];
	}
    }
  if(copy.type == BL_AS_STRING && copy.hop_count > 0)
    {
      copy.str_aspath = strdup(aspath->str_aspath);
    }  
  return copy;
}


void bl_aspath_freedynmem(bl_aspath_storage_t *aspath)
{
  if(aspath->type == BL_AS_STRING)
    {
      free(aspath->str_aspath);
      aspath->str_aspath = NULL;
      aspath->type = BL_AS_TYPE_UNKNOWN;
    }
  if(aspath->type == BL_AS_NUMERIC)
    {
      free(aspath->numeric_aspath);
      aspath->numeric_aspath = NULL;
      aspath->type = BL_AS_TYPE_UNKNOWN;
    }
}

/** as numbers */
khint32_t bl_as_storage_hash_func(bl_as_storage_t as)
{
  khint32_t h = 0;
  if(as.type == BL_AS_NUMERIC)
    {
      h = as.as_number;
    }
  if(as.type == BL_AS_STRING)
    {
      // if the string is at least 32 bits
      // then consider the first 32 bits as
      // the hash
      if(strlen(as.as_string) >= 4)
	{
	  h = * ((khint32_t *) as.as_string);
	}
      else
	{
	  // TODO: this could originate a lot of collisions
	  // otherwise 0
	  h = 0; 
	}
    }
  return __ac_Wang_hash(h);
}


int bl_as_storage_hash_equal(bl_as_storage_t as1, bl_as_storage_t as2)
{
  if(as1.type == BL_AS_NUMERIC && as2.type == BL_AS_NUMERIC)
    {
      return (as1.as_number == as2.as_number);
    }
  if(as1.type == BL_AS_STRING && as2.type == BL_AS_STRING)
    {
      return (strcmp(as1.as_string, as2.as_string) == 0);
    }
  return 0;
}


