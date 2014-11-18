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

#include "bl_bgp_utils.h"
#include <stdio.h>
#include "utils.h"
#include <inttypes.h>
#include <assert.h>


/** Print functions */

char *bl_print_elemtype(bl_elem_type_t type)
{
  switch(type)
    {
    case BL_RIB_ELEM:
      return strdup("R");
    case BL_ANNOUNCEMENT_ELEM:
      return strdup("A");
    case BL_WITHDRAWAL_ELEM:
      return strdup("W");
    case BL_PEERSTATE_ELEM:
      return strdup("S");
    default:
      // do nothing
      break;
    }
  return strdup("");
}


char *bl_print_ipv4_addr(bl_ipv4_addr_t* addr)
{
  char addr_str[INET_ADDRSTRLEN];
  addr_str[0] ='\0';
  inet_ntop(AF_INET, addr.ipv4, addr_str, INET_ADDRSTRLEN);
  return strdup(addr_str);
}

char *bl_print_ipv6_addr(bl_ipv6_addr_t* addr)
{
  char addr_str[INET6_ADDRSTRLEN];
  addr_str[0] ='\0';
  inet_ntop(AF_INET6, addr.ipv6, addr_str, INET6_ADDRSTRLEN);
  return strdup(addr_str);
}

char *bl_print_addr_storage(bl_addr_storage_t* addr)
{
  if(addr->version == BL_ADDR_IPV4)
    {
      return bl_print_ipv4_addr((bl_ipv4_addr_t*)addr);
    }
  if(addr->version == BL_ADDR_IPV6)
    {
      return bl_print_ipv6_addr((bl_ipv6_addr_t*)addr);
    }
  return NULL;
}


char *bl_print_ipv4_pfx(bl_ipv4_pfx_t* pfx)
{
  char pfx_str[24];
  pfx_str[0] ='\0';
  char *addr_str = bl_print_ipv4_addr(&(pfx->address));
  sprintf(pfx_str, "%s/%"PRIu8, addr_str, pfx->mask_len);
  if(addr_str != NULL)
    {
      free(addr_str);
    }       
  return strdup(pfx_str);
}

char *bl_print_ipv6_pfx(bl_ipv6_pfx_t* pfx)
{
  char pfx_str[64];
  pfx_str[0] ='\0';
  char *addr_str = bl_print_ipv6_addr(&(pfx->address));
  sprintf(pfx_str, "%s/%"PRIu8, addr_str, pfx->mask_len);
  if(addr_str != NULL)
    {
      free(addr_str);
    }       
  return strdup(pfx_str);
}

char *bl_print_pfx_storage(bl_pfx_storage_t* pfx)
{
  char pfx_str[64];
  pfx_str[0] ='\0';
  char *addr_str = bl_print_addr_storage(&(pfx->address));
  sprintf(pfx_str, "%s/%"PRIu8, addr_str, pfx->mask_len);
  if(addr_str != NULL)
    {
      free(addr_str);
    }       
  return strdup(pfx_str);
}

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
    return "";
}


char *bl_print_peerstate(bl_peerstate_type_t state)
{
  switch(state)
    {
    case BL_PEERSTATE_IDLE:
      return strdup("IDLE");
    case BL_PEERSTATE_CONNECT:
      return strdup("CONNECT");
    case BL_PEERSTATE_ACTIVE:
      return strdup("ACTIVE");
    case BL_PEERSTATE_OPENSENT:
      return strdup("OPENSENT");
    case BL_PEERSTATE_OPENCONFIRM:
      return strdup("OPENCONFIRM");
    case BL_PEERSTATE_ESTABLISHED:
      return strdup("ESTABLISHED");
    default:
      // do nothing
      break;
    }
  return strdup("");
}


char *bl_print_elem(bl_elem_t *elem)
{
  assert(elem);

  char elem_str[4096];
  elem_str[0] = '\0';

  char partial[4096];
  partial[0] = '\0';

  char *pa = NULL;
  char *et = NULL;
  char *pr = NULL;
  char *nh = NULL;
  char *ap = NULL;
  char *ao = NULL;
  char *os = NULL;
  char *ns = NULL;
  bl_as_storage_t a;
  
  // timestamp|peer_ip|peer_asn|message_type|

  sprintf(partial, "%"PRIu32"|%s|%"PRIu32"|%s|",
	  elem->timestamp,
	  (pa = bl_print_addr_storage(&elem->peer_address)),
	  elem->peer_asnumber,
	  (et = bl_print_elemtype(elem->type)) 
	  );

  strcat(elem_str, partial);
  // reset partial string
  partial[0] = '\0';

  switch(elem->type)
    {
    case BL_RIB_ELEM:
    case BL_ANNOUNCEMENT_ELEM:
      a = bl_get_origin_as(&elem->aspath);
      sprintf(partial, "%s|%s|%s|%s|",
	      (pr = bl_print_pfx_storage(&(elem->prefix))),
	      (nh = bl_print_addr_storage(&(elem->nexthop))),
	      (ap = bl_print_aspath(&(elem->aspath))),
	      (ao = bl_print_as(&a)));
      break;
    case BL_WITHDRAWAL_ELEM:
      sprintf(partial, "%s|", (pr = bl_print_pfx_storage(&(elem->prefix))));
      break;
    case BL_PEERSTATE_ELEM:
      sprintf(partial, "%s|%s|",
	      (os = bl_print_peerstate(elem->old_state)),
	      (ns = bl_print_peerstate(elem->new_state)));
      break;
    default:
      fprintf(stderr, "Error during elem processing\n");
    }

  strcat(elem_str, partial);

  // free all temporary strings  
  if(pa == NULL) free(pa);
  if(et == NULL) free(et);
  if(pr == NULL) free(pr);
  if(nh == NULL) free(nh);
  if(ap == NULL) free(ap);
  if(ao == NULL) free(ao);
  if(os == NULL) free(os);
  if(ns == NULL) free(ns);
    
  return strdup(elem_str);

}



/** Utility functions (conversion between address types) */

bl_ipv4_addr_t *bl_addr_storage2ipv4(bl_addr_storage_t *address)
{
  assert(address->version == BL_ADDR_IPV4);
  return (bl_ipv4_addr_t *) address;
}

bl_ipv6_addr_t *bl_addr_storage2ipv6(bl_addr_storage_t *address)
{
  assert(address->version == BL_ADDR_IPV6);
  return (bl_ipv6_addr_t *) address;
}

bl_ipv4_pfx_t *bl_pfx_storage2ipv4(bl_pfx_storage_t *prefix)
{
  assert(prefix->address.version == BL_ADDR_IPV4);
  return (bl_ipv4_pfx_t *) prefix;
}

bl_ipv6_pfx_t *bl_pfx_storage2ipv6(bl_pfx_storage_t *prefix)
{
  assert(prefix->address.version == BL_ADDR_IPV6);
  return (bl_ipv6_pfx_t *) prefix;
}

bl_addr_storage_t *bl_addr_ipv42storage(bl_ipv4_addr_t *address)
{
  return (bl_addr_storage_t *)address;
}

bl_addr_storage_t *bl_addr_ipv62storage(bl_ipv6_addr_t *address)
{
  return (bl_addr_storage_t *)address;
}

bl_pfx_storage_t *bl_pfx_ipv42storage(bl_ipv4_pfx_t *prefix)
{
  return (bl_pfx_storage_t *) prefix;
}

bl_pfx_storage_t *bl_pfx_ipv62storage(bl_ipv6_pfx_t *prefix)
{
  return (bl_pfx_storage_t *) prefix;
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


/* khash utility functions 
 * Note:
 * __ac_Wang_hash(h) decreases the
 * chances of collisions
 */

/* addresses */
khint64_t bl_addr_storage_hash_func(bl_addr_storage_t ip)
{
  khint64_t h = 0;
  if(ip.version == BL_ADDR_IPV4)
    {
      h = bl_ipv4_addr_hash_func(*((bl_ipv4_addr_t *)&ip));
    }
  if(ip.version == BL_ADDR_IPV6)
    {
      h = bl_ipv6_addr_hash_func(*((bl_ipv6_addr_t *)&ip));
    }
  return h;
}

int bl_addr_storage_hash_equal(bl_addr_storage_t ip1, bl_addr_storage_t ip2)
{
  if(ip1.version == BL_ADDR_IPV4 && ip2.version == BL_ADDR_IPV4)
    {
      return bl_ipv4_addr_hash_equal(*((bl_ipv4_addr_t *)&ip1),*((bl_ipv4_addr_t *)&ip2));
    }
  if(ip1.version == BL_ADDR_IPV6 && ip2.version == BL_ADDR_IPV6)
    {
      return bl_ipv6_addr_hash_equal(*((bl_ipv6_addr_t *)&ip1),*((bl_ipv6_addr_t *)&ip2));
    }
  return 0;
}

khint32_t bl_ipv4_addr_hash_func(bl_ipv4_addr_t ip)
{
  khint32_t h = ip.ipv4.s_addr;  
  return __ac_Wang_hash(h);
}

int bl_ipv4_addr_hash_equal(bl_ipv4_addr_t ip1, bl_ipv4_addr_t ip2)
{
  return (ip1.ipv4.s_addr == ip2.ipv4.s_addr);
}

khint64_t bl_ipv6_addr_hash_func(bl_ipv6_addr_t ip)
{
  unsigned char *s6 =  &(ip.ipv6.s6_addr[0]);
  khint64_t h = *((khint64_t *) s6);
  return __ac_Wang_hash(h);
}

int bl_ipv6_addr_hash_equal(bl_ipv6_addr_t ip1, bl_ipv6_addr_t ip2)
{
  return ( (memcmp(&(ip1.ipv6.s6_addr[0]), &(ip2.ipv6.s6_addr[0]), sizeof(uint64_t)) == 0) &&
	   (memcmp(&(ip1.ipv6.s6_addr[8]), &(ip2.ipv6.s6_addr[8]), sizeof(uint64_t)) == 0) );
}


/** prefixes */
khint64_t bl_pfx_storage_hash_func(bl_pfx_storage_t prefix)
{
  khint64_t h;
  uint64_t address = 0;
  unsigned char *s6 = NULL;
  if(prefix.address.version == BL_ADDR_IPV4)
    {
      address = ntohl(prefix.address.ipv4.s_addr);
    }
  if(prefix.address.version == BL_ADDR_IPV6)
    {
      s6 =  &(prefix.address.ipv6.s6_addr[0]);
      address = *((uint64_t *) s6);
      address = ntohll(address);
    }
  h = address | (uint64_t) prefix.mask_len;
  return __ac_Wang_hash(h);
}

int bl_pfx_storage_hash_equal(bl_pfx_storage_t prefix1,
			      bl_pfx_storage_t prefix2)
{
  if(prefix1.mask_len == prefix2.mask_len)
    {
      return bl_addr_storage_hash_equal(prefix1.address, prefix2.address);
    }
  return 0;
}

khint32_t bl_ipv4_pfx_hash_func(bl_ipv4_pfx_t prefix)
{
  // convert network byte order to host byte order
  // ipv4 32 bits number (in host order)
  uint32_t address = ntohl(prefix.address.ipv4.s_addr);  
  // embed the network mask length in the 32 bits
  khint32_t h = address | (uint32_t) prefix.mask_len;
  return __ac_Wang_hash(h);
}

int bl_ipv4_pfx_hash_equal(bl_ipv4_pfx_t prefix1, bl_ipv4_pfx_t prefix2)
{
  return ( (prefix1.address.ipv4.s_addr == prefix2.address.ipv4.s_addr) &&
	   (prefix1.mask_len == prefix2.mask_len));
}

khint64_t bl_ipv6_pfx_hash_func(bl_ipv6_pfx_t prefix)
{
  // ipv6 number - we take most significative 64 bits only (in host order)
  unsigned char *s6 =  &(prefix.address.ipv6.s6_addr[0]);
  uint64_t address = *((uint64_t *) s6);
  address = ntohll(address);
  // embed the network mask length in the 64 bits
  khint64_t h = address | (uint64_t) prefix.mask_len;
  return __ac_Wang_hash(h);
}

int bl_ipv6_pfx_hash_equal(bl_ipv6_pfx_t prefix1, bl_ipv6_pfx_t prefix2)
{

  return ( (memcmp(&(prefix1.address.ipv6.s6_addr[0]), &(prefix2.address.ipv6.s6_addr[0]), sizeof(uint64_t)) == 0) &&
	   (memcmp(&(prefix1.address.ipv6.s6_addr[8]), &(prefix2.address.ipv6.s6_addr[8]), sizeof(uint64_t)) == 0) &&
	   prefix1.mask_len == prefix2.mask_len );

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


