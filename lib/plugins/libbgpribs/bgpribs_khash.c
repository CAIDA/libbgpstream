/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#include "bgpribs_khash.h"

#include "khash.h"
#include "bgpstream_lib.h"
#include "utils.h"
#include <assert.h>


/** ---------------- ipv4 address hashing and comparison ----------------*/

khint32_t bgpstream_ipv4_address_hash_func(bgpstream_ip_address_t ip)
{
  assert(ip.type == BST_IPV4); // check type is ipv4
  khint32_t h = ip.address.v4_addr.s_addr;  
  return __ac_Wang_hash(h);  // decreases the chances of collisions
}

int bgpstream_ipv4_address_hash_equal(bgpstream_ip_address_t ip1,
					     bgpstream_ip_address_t ip2)
{
  // assert(ip1.type == BST_IPV4); // check type is ipv4
  // assert(ip2.type == BST_IPV4); // check type is ipv4
  // we cannot use memcmp as these are unions and ipv4 is not the
  // largest data structure that can fit in the union, ipv6 is
  return (ip1.address.v4_addr.s_addr == ip2.address.v4_addr.s_addr);
}



/** ---------------- ipv6 address hashing and comparison ----------------*/

khint64_t bgpstream_ipv6_address_hash_func(bgpstream_ip_address_t ip) 
{
  assert(ip.type == BST_IPV6); // check type is ipv6
  khint64_t h = *((khint64_t *) &(ip.address.v6_addr.s6_addr[0]));
  return __ac_Wang_hash(h);  // decreases the chances of collisions
}

int bgpstream_ipv6_address_hash_equal(bgpstream_ip_address_t ip1,
				      bgpstream_ip_address_t ip2) 
{
  //assert(ip1.type == BST_IPV6); // check type is ipv6
  //assert(ip2.type == BST_IPV6); // check type is ipv6
  return ( (memcmp(&(ip1.address.v6_addr.s6_addr[0]),
		   &(ip2.address.v6_addr.s6_addr[0]),
		   sizeof(uint64_t)) == 0) &&
	   (memcmp(&(ip1.address.v6_addr.s6_addr[8]),
		   &(ip2.address.v6_addr.s6_addr[8]),
		   sizeof(uint64_t)) == 0) );
}



/** ---------------- ipv4 prefix hashing and comparison ----------------*/

khint32_t bgpstream_prefix_ipv4_hash_func(bgpstream_prefix_t prefix)
{
  assert(prefix.number.type == BST_IPV4);
  assert(prefix.len >= 0);
  assert(prefix.len <= 32);
  khint32_t h = 0;
  // convert network byte order to host byte order
  // ipv4 32 bits number (in host order)
  uint32_t address = ntohl(prefix.number.address.v4_addr.s_addr);  
  // embed the network mask length in the 32 bits
  h = address | (uint32_t) prefix.len;
  return __ac_Wang_hash(h); // decreases the chances of collisions
}

int bgpstream_prefix_ipv4_hash_equal(bgpstream_prefix_t prefix1,
				     bgpstream_prefix_t prefix2)
{
  // assert(prefix1.number.type == BST_IPV4); // check type is ipv4
  // assert(prefix2.number.type == BST_IPV4); // check type is ipv4
  return (prefix1.number.address.v4_addr.s_addr == prefix2.number.address.v4_addr.s_addr) &&
    (prefix1.len == prefix2.len);
}



/** ---------------- ipv6 address hashing and comparison ----------------*/

khint64_t bgpstream_prefix_ipv6_hash_func(bgpstream_prefix_t prefix) 
{
  assert(prefix.number.type == BST_IPV6);
  assert(prefix.len >= 0);
  khint64_t h = 0;
  // ipv6 number - we take most significative 64 bits only (in host order)
  uint64_t address = *((uint64_t *) &(prefix.number.address.v6_addr.s6_addr[0]));
  address = ntohll(address);
  // embed the network mask length in the 64 bits
  h = address | (uint64_t) prefix.len;
  return __ac_Wang_hash(h);  // decreases the chances of collisions
}


int bgpstream_prefix_ipv6_hash_equal(bgpstream_prefix_t prefix1,
				     bgpstream_prefix_t prefix2) 
{
  // assert(prefix1.number.type == BST_IPV6); // check type is ipv6
  // assert(prefix2.number.type == BST_IPV6); // check type is ipv6
  return ( (memcmp(&(prefix1.number.address.v6_addr.s6_addr[0]),
		   &(prefix2.number.address.v6_addr.s6_addr[0]),
		   sizeof(uint64_t)) == 0) &&
	   (prefix1.len == prefix2.len) && // this should be enough
	   (memcmp(&(prefix1.number.address.v6_addr.s6_addr[8]),
		   &(prefix2.number.address.v6_addr.s6_addr[8]),
		   sizeof(uint64_t)) == 0)	    
	   /* (  *((uint64_t *) &(prefix1.number.address.v6_addr.s6_addr[0])) == */
	   /*    *((uint64_t *) &(prefix2.number.address.v6_addr.s6_addr[0])) ) && */
	   /* (  *((uint64_t *) &(prefix1.number.address.v6_addr.s6_addr[8])) == */
	   /*    *((uint64_t *) &(prefix2.number.address.v6_addr.s6_addr[8])) ) */
	   );
}


