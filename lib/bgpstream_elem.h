/*
 * libbgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of libbgpstream.
 *
 * libbgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libbgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef _BGPSTREAM_ELEM_H
#define _BGPSTREAM_ELEM_H


#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>


typedef union union_bgpstream_address_number_t {
    struct in_addr	v4_addr;
    struct in6_addr	v6_addr;
} bgpstream_address_number_t;

typedef enum {BST_IPV4, BST_IPV6} bgpstream_ip_version_t;

typedef struct struct_bgpstream_ip_address_t {
  bgpstream_address_number_t address;  // ip address
  bgpstream_ip_version_t type;         // ipv4 or ipv6
} bgpstream_ip_address_t;

typedef struct struct_bgpstream_prefix_t {  
  bgpstream_ip_address_t number;  // first ip address of prefix
  uint8_t len;                    // prefix length
} bgpstream_prefix_t;


typedef enum {BST_STRING_ASPATH, // AS PATH is stored as a string (as it contains AS confederations or AS sets)
	      BST_UINT32_ASPATH  // AS PATH is stored as an array of uint32 
} bgpstream_aspath_type_t;

typedef struct struct_bgpstream_aspath_t {
  bgpstream_aspath_type_t type;
  uint8_t hop_count; // number of hops in the AS path
  union {
    // if the path contains sets or confederations
    // we maintain the string structure
    char * str_aspath; 
    // otherwise we maintain the as path as a vector of uint_32
    uint32_t * numeric_aspath;
  };
} bgpstream_aspath_t;


typedef enum {BST_RIB = 0,
	      BST_ANNOUNCEMENT =1,
	      BST_WITHDRAWAL = 2,
	      BST_STATE = 3
} bgpstream_elem_type_t;

#define BGPSTREAM_ELEM_TYPE_MAX 4

typedef enum {BST_UNKNOWN, BST_IDLE, BST_CONNECT, BST_ACTIVE,
	      BST_OPENSENT, BST_OPENCONFIRM, BST_ESTABLISHED, 
	      BST_NULL} bgpstream_peer_state_t;

typedef struct struct_bgpstream_elem_t {
  bgpstream_elem_type_t type;
  long int timestamp;  // "time of record dump" (see -m option, default time in bgpdump.c file)
  bgpstream_ip_address_t peer_address;  // peer IP address
  uint32_t peer_asnumber;           // peer AS number
  bgpstream_prefix_t prefix;        // IP prefix
  bgpstream_ip_address_t nexthop;   // next hop ip address
  bgpstream_aspath_t aspath;        // aspath
  bgpstream_peer_state_t old_state; // RIS peer status variables
  bgpstream_peer_state_t new_state; // 6 = peer up,
  // != 6 peer is not up (either down, or waking up)
  struct struct_bgpstream_elem_t * next; 
  // TODO: translate other attributes
} bgpstream_elem_t;


#endif /* _BGPSTREAM_ELEM_H */
