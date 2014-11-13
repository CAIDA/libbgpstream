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


#ifndef _BL_BGP_UTILS_H
#define _BL_BGP_UTILS_H

#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "khash.h"


#define BGPCOMMON_COLLECTOR_NAME_LEN 128


typedef enum {BL_UNKNOWN_DUMP = 0,
              BL_RIB_DUMP     = 1,
	      BL_UPDATE_DUMP  = 2
} bl_dump_type_t;

#define BL_DUMP_TYPE_MAX 3


typedef enum {BL_UNKNOWN_ELEM      = 0,
	      BL_RIB_ELEM          = 1,
	      BL_ANNOUNCEMENT_ELEM = 2,
	      BL_WITHDRAWAL_ELEM   = 3,
	      BL_PEERSTATE_ELEM    = 4
} bl_elem_type_t;

#define BL_ELEM_TYPE_MAX 4


typedef enum {BL_PEERSTATE_UNKNOWN     = 0,
	      BL_PEERSTATE_IDLE        = 1,
	      BL_PEERSTATE_CONNECT     = 2,
	      BL_PEERSTATE_ACTIVE      = 3,
	      BL_PEERSTATE_OPENSENT    = 4,
	      BL_PEERSTATE_OPENCONFIRM = 5,
	      BL_PEERSTATE_ESTABLISHED = 6, 
	      BL_PEERSTATE_NULL        = 7 
} bl_peerstate_type_t;

#define BL_PEERSTATE_TYPE_MAX 8

typedef enum {BL_ADDR_TYPE_UNKNOWN  = 0,
              BL_ADDR_IPV4          = AF_INET,
	      BL_ADDR_IPV6          = AF_INET6
} bl_addr_type_t;

#define BL_ADDR_TYPE_MAX 3

typedef struct in_addr  bl_ipv4_addr_t;
typedef struct in6_addr bl_ipv6_addr_t;

char *print_ipv4_addr(bl_ipv4_addr_t* addr);
char *print_ipv6_addr(bl_ipv6_addr_t* addr);

typedef struct struct_bl_addr_storage_t {
  /** ip version (v4 o v6) */
  bl_addr_type_t version;
  /** ip address */
  union {
    bl_ipv4_addr_t ipv4;
    bl_ipv6_addr_t ipv6;
  };
} bl_addr_storage_t;

char *print_addr_storage(bl_addr_storage_t* addr);


typedef struct struct_bl_ipv4_pfx_t {
  /** the address */
  bl_ipv4_addr_t address;
  /** length of the prefix mask */
  uint8_t mask_len;
} bl_ipv4_pfx_t;

char *print_ipv4_pfx(bl_ipv4_pfx_t* pfx);

typedef struct struct_bl_ipv6_pfx_t {
  /** the address */
  bl_ipv6_addr_t address;
  /** length of the prefix mask */
  uint8_t mask_len;
} bl_ipv6_pfx_t;

char *print_ipv6_pfx(bl_ipv6_pfx_t* pfx);

typedef struct struct_bl_pfx_storage_t {
  /** the address */
  bl_addr_storage_t address;
  /** length of the prefix mask */
  uint8_t mask_len;
} bl_pfx_storage_t;

char *print_pfx_storage(bl_pfx_storage_t* pfx);


typedef enum {BL_AS_TYPE_UNKNOWN = 0,
	      BL_AS_NUMERIC      = 1,
	      BL_AS_STRING       = 2
} bl_as_type_t;

#define BL_AS_TYPE_MAX 3

typedef struct struct_bl_aspath_storage_t {
  /** aspath type (numeric or string) */
  bl_as_type_t type;
  /** number of hops in the AS path */
  uint8_t hop_count; // number of hops in the AS path
  /** aspath */
  union {
    // if the path contains sets or confederations
    // we maintain the string structure
    char * str_aspath; 
    // otherwise we maintain the as path as a vector of uint_32
    uint32_t * numeric_aspath;
  };
} bl_aspath_storage_t;


/** generic as representation 
 *  actually, generic aspath hop
 *  description */
typedef struct struct_bl_as_storage_t {
  /** as type (numeric or string) */
  bl_as_type_t type;
  /** as number */
  union {
    // it could be a confederation or a set
    // so we cannot set a length
    char    *as_string; 
    uint32_t as_number;
  };
} bl_as_storage_t;


typedef struct struct_bl_elem_t {
  
  /** type of bgp elem */
  bl_elem_type_t type;
  /** epoch time that refers to when this
   *  elem was generated on the peer */
  uint32_t timestamp;  
  /** peer IP address */
  bl_addr_storage_t peer_address;  
  /** peer AS number */
  uint32_t peer_asnumber;
  
  /** type-dependent fields */
  /** IP prefix */
  bl_pfx_storage_t prefix;
  /** next hop */
  bl_addr_storage_t nexthop;  
  /** AS path */
  bl_aspath_storage_t aspath;
  /** old state of the peer */
  bl_peerstate_type_t old_state;
  /** new state of the peer */
  bl_peerstate_type_t new_state;

} bl_elem_t;



/** Utility functions (conversion between address types) */


bl_ipv4_pfx_t bl_pfx_storage2ipv4(bl_pfx_storage_t *prefix);
bl_ipv6_pfx_t bl_pfx_storage2ipv6(bl_pfx_storage_t *prefix);

bl_pfx_storage_t bl_pfx_ipv42storage(bl_ipv4_pfx_t *prefix);
bl_pfx_storage_t bl_pfx_ipv62storage(bl_ipv6_pfx_t *prefix);

/** as-path utility functions */

bl_as_storage_t bl_get_origin_as(bl_aspath_storage_t *aspath);


/** khash utility functions */

/** addresses */
khint64_t bl_addr_storage_hash_func(bl_addr_storage_t ip);
int bl_addr_storage_hash_equal(bl_addr_storage_t ip1, bl_addr_storage_t ip2);

khint32_t bl_ipv4_addr_hash_func(bl_ipv4_addr_t ip);
int bl_ipv4_addr_hash_equal(bl_ipv4_addr_t ip1, bl_ipv4_addr_t ip2);

khint64_t bl_ipv6_addr_hash_func(bl_ipv6_addr_t ip);
int bl_ipv6_addr_hash_equal(bl_ipv6_addr_t ip1, bl_ipv6_addr_t ip2);

/** prefixes */
khint64_t bl_pfx_storage_hash_func(bl_pfx_storage_t prefix);
int bl_pfx_storage_hash_equal(bl_pfx_storage_t prefix1, bl_pfx_storage_t prefix2);

khint32_t bl_ipv4_pfx_hash_func(bl_ipv4_pfx_t prefix);
int bl_ipv4_pfx_hash_equal(bl_ipv4_pfx_t prefix1, bl_ipv4_pfx_t prefix2);

khint64_t bl_ipv6_pfx_hash_func(bl_ipv6_pfx_t prefix);
int bl_ipv6_pfx_hash_equal(bl_ipv6_pfx_t prefix1, bl_ipv6_pfx_t prefix2);


/** as numbers */
khint32_t bl_as_storage_hash_func(bl_as_storage_t as);
int bl_as_storage_hash_equal(bl_as_storage_t as1, bl_as_storage_t as2);



#endif /* _BL_BGP_UTILS_H */

