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

#include "bgpribs_common.h"


void graphite_safe(char *p)
{
  if(p == NULL)
    {
      return;
    }

  while(*p != '\0')
    {
      if(*p == '.')
	{
	  *p = '_';
	}
      if(*p == '*')
	{
	  *p = '-';
	}
      p++;
    }
}


aggregated_bgp_stats_t *aggregated_bgp_stats_create()
{
  aggregated_bgp_stats_t *aggr = NULL;

  if((aggr = (aggregated_bgp_stats_t *) malloc_zero(sizeof(aggregated_bgp_stats_t)) ) == NULL)
    {
      goto err;
    }

  aggr->unique_ipv4_prefixes = NULL;
  aggr->unique_ipv6_prefixes = NULL;
  aggr->unique_origin_ases = NULL;
  aggr->affected_ipv4_prefixes = NULL;
  aggr->affected_ipv6_prefixes = NULL;
  aggr->announcing_origin_ases = NULL;

  if((aggr->unique_ipv4_prefixes = bl_ipv4_pfx_set_create()) == NULL)
    {
      goto err;
    }

  if((aggr->unique_ipv6_prefixes = bl_ipv6_pfx_set_create()) == NULL)
    {
      goto err;
    }

  if((aggr->unique_origin_ases = bl_id_set_create()) == NULL)
    {
      goto err;
    }
    
  if((aggr->affected_ipv4_prefixes = bl_ipv4_pfx_set_create()) == NULL)
    {
      goto err;
    }

  if((aggr->affected_ipv6_prefixes = bl_ipv6_pfx_set_create()) == NULL)
    {
      goto err;
    }
  
  if((aggr->announcing_origin_ases = bl_id_set_create()) == NULL)
    {
      goto err;
    }
  
  return aggr;
  
 err:
  aggregated_bgp_stats_destroy(aggr);
  fprintf(stderr, "Error: can't allocate memory for aggregated statistics\n");
  return NULL;
}


void aggregated_bgp_stats_destroy(aggregated_bgp_stats_t *aggr)
{
  if(aggr != NULL)
    {
      if(aggr->unique_ipv4_prefixes != NULL)
        {
          bl_ipv4_pfx_set_destroy(aggr->unique_ipv4_prefixes);
          aggr->unique_ipv4_prefixes = NULL;
        }
      if(aggr->unique_ipv6_prefixes != NULL)
        {
          bl_ipv6_pfx_set_destroy(aggr->unique_ipv6_prefixes);
          aggr->unique_ipv6_prefixes = NULL;
        }
      if(aggr->unique_origin_ases != NULL)
        {
          bl_id_set_destroy(aggr->unique_origin_ases);
          aggr->unique_origin_ases = NULL;
        }     
      if(aggr->affected_ipv4_prefixes != NULL)
        {
          bl_ipv4_pfx_set_destroy(aggr->affected_ipv4_prefixes);
          aggr->affected_ipv4_prefixes = NULL;
        }
      if(aggr->affected_ipv6_prefixes != NULL)
        {
          bl_ipv6_pfx_set_destroy(aggr->affected_ipv6_prefixes);
          aggr->affected_ipv6_prefixes = NULL;
        }
      if(aggr->announcing_origin_ases != NULL)
        {
          bl_id_set_destroy(aggr->announcing_origin_ases);
          aggr->announcing_origin_ases = NULL;
        }
      free(aggr);
      aggr = NULL;
    }
}
  
