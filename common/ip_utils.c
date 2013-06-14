/* 
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Much of this code has been adapted from the iprange.c code available at
 *   http://www.cs.colostate.edu/~somlo/iprange.c
 * 
 * Copyright (C) 2012 The Regents of the University of California.
 * 
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ip_utils.h"

/**
 * Recursively compute network addresses to cover range lo-hi
 *
 * Note: Worst case scenario is when lo=0.0.0.1 and hi=255.255.255.254
 *       We then have 62 CIDR bloks to cover this interval, and 125
 *       calls to split_range();
 *       The maximum possible recursion depth is 32.
 */
static int split_range(uint32_t addr, int masklen, 
		       uint32_t lo, uint32_t hi,
		       ip_prefix_list_t **pfx_list) 
{
  uint32_t bc, lower_half, upper_half;
  
  ip_prefix_list_t *new_node = NULL;

  if ((masklen < 0) || (masklen > 32) || pfx_list == NULL) 
    {
      return -1;
    }

  bc = ip_broadcast_addr(addr, masklen);

  if ((lo < addr) || (hi > bc)) 
    {
      return -1;
    }

  if ( (lo == addr) && (hi == bc) ) 
    {
      /* add this (addr, masklen) to the list */
      if((new_node = malloc(sizeof(ip_prefix_list_t))) == NULL)
	{
	  return -1;
	}
      new_node->prefix.addr = addr;
      new_node->prefix.masklen = masklen;
      
      /* this will work even for the first element in the list
       as pfx_list will be null */
      new_node->next = *pfx_list;
      *pfx_list = new_node;
      
      return 0;
    }

  masklen++;
  lower_half = addr;
  upper_half = ip_set_bit(addr, masklen, 1);
  
  if (hi < upper_half) 
    {
      return split_range(lower_half, masklen, lo, hi, pfx_list);
    } 
  else if (lo >= upper_half) 
    {
      return split_range(upper_half, masklen, lo, hi, pfx_list);
    } 
  else 
    {
      if(split_range(lower_half, masklen, lo, 
		     ip_broadcast_addr(lower_half, masklen), pfx_list) != 0)
      {
	return -1;
      }
      return split_range(upper_half, masklen, upper_half, hi, pfx_list);
    }

  assert(0);
}

/**
 * Set a bit to a given value (0 or 1); MSB is bit 1, LSB is bit 32
 */
uint32_t ip_set_bit(uint32_t addr, int bitno, int val) 
{
  if (val)
    return(addr | (1 << (32 - bitno)));
  else
    return(addr & ~(1 << (32 - bitno)));
}

/**
 * Compute netmask address given a prefix bit length
 */
uint32_t ip_netmask(int masklen) 
{
  if ( masklen == 0 )
    return( ~((uint32_t) -1) );
  else
    return( ~((1 << (32 - masklen)) - 1) );
}

/**
 * Compute broadcast address given address and prefix
 */
uint32_t ip_broadcast_addr(uint32_t addr, int masklen) 
{
  return(addr | ~ip_netmask(masklen));
}

/**
 * Compute network address given address and prefix
 */
uint32_t ip_network_addr(uint32_t addr, int masklen) 
{
  return(addr & ip_netmask(masklen));
}

int ip_range_to_prefix(ip_prefix_t lower, ip_prefix_t upper, 
		       ip_prefix_list_t **pfx_list)
{
  /* get the first address of the lower prefix */
  uint32_t lo = ip_network_addr(lower.addr, lower.masklen);
  /* get the last address of the upper prefix */
  uint32_t hi = ip_broadcast_addr(upper.addr, upper.masklen);
  
  *pfx_list = NULL;
  return split_range(0, 0, lo, hi, pfx_list);
}

void ip_prefix_list_free(ip_prefix_list_t *pfx_list)
{
  ip_prefix_list_t *temp = pfx_list;

  while(temp != NULL)
    {
      temp = pfx_list->next;
      pfx_list->next = NULL;
      free(pfx_list);
      pfx_list = temp;
    }
}
