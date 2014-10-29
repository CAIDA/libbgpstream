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

#include "bl_addr_set.h"
#include <stdio.h>
#include "utils.h"
#include <assert.h>

bl_addr_storage_set_t *bl_addr_storage_set_create() 
{
  bl_addr_storage_set_t *ip_address_set = NULL;
  ip_address_set = kh_init(bl_addr_storage_set);
  return ip_address_set;
}

int bl_addr_storage_set_insert(bl_addr_storage_set_t *ip_address_set, bl_addr_storage_t ip_address) 
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_addr_storage_set, ip_address_set,
		 ip_address)) == kh_end(ip_address_set))
    {
      k = kh_put(bl_addr_storage_set, ip_address_set, 
		 ip_address, &khret);
      return 1;
    }
  return 0;
}

void bl_addr_storage_set_reset(bl_addr_storage_set_t *ip_address_set) 
{
  kh_clear(bl_addr_storage_set, ip_address_set);
}

void bl_addr_storage_set_destroy(bl_addr_storage_set_t *ip_address_set) 
{
  kh_destroy(bl_addr_storage_set, ip_address_set);
}


// ipv4 specific functions

bl_ipv4_addr_set_t *bl_ipv4_addr_set_create() 
{
  bl_ipv4_addr_set_t *ip_address_set = NULL;
  ip_address_set = kh_init(bl_ipv4_addr_set);
  return ip_address_set;
}

int bl_ipv4_addr_set_insert(bl_ipv4_addr_set_t *ip_address_set, bl_ipv4_addr_t ip_address) 
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_ipv4_addr_set, ip_address_set,
		 ip_address)) == kh_end(ip_address_set))
    {
      k = kh_put(bl_ipv4_addr_set, ip_address_set, 
		 ip_address, &khret);
      return 1;
    }
  return 0;
}

void bl_ipv4_addr_set_reset(bl_ipv4_addr_set_t *ip_address_set) 
{
  kh_clear(bl_ipv4_addr_set, ip_address_set);
}

void bl_ipv4_addr_set_destroy(bl_ipv4_addr_set_t *ip_address_set) 
{
  kh_destroy(bl_ipv4_addr_set, ip_address_set);
}


// ipv6 specific functions

bl_ipv6_addr_set_t *bl_ipv6_addr_set_create() 
{
  bl_ipv6_addr_set_t *ip_address_set = NULL;
  ip_address_set = kh_init(bl_ipv6_addr_set);
  return ip_address_set;
}

int bl_ipv6_addr_set_insert(bl_ipv6_addr_set_t *ip_address_set, bl_ipv6_addr_t ip_address) 
{
  int khret;
  khiter_t k;
  if((k = kh_get(bl_ipv6_addr_set, ip_address_set,
		 ip_address)) == kh_end(ip_address_set))
    {
      k = kh_put(bl_ipv6_addr_set, ip_address_set, 
		 ip_address, &khret);
      return 1;
    }
  return 0;
}

void bl_ipv6_addr_set_reset(bl_ipv6_addr_set_t *ip_address_set) 
{
  kh_clear(bl_ipv6_addr_set, ip_address_set);
}

void bl_ipv6_addr_set_destroy(bl_ipv6_addr_set_t *ip_address_set) 
{
  kh_destroy(bl_ipv6_addr_set, ip_address_set);
}


