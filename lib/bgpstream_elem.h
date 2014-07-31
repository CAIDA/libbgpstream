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
  bgpstream_address_number_t address;
  bgpstream_ip_version_t type;
} bgpstream_ip_address_t;

typedef struct struct_bgpstream_prefix_t {  
  bgpstream_ip_address_t number;
  uint8_t len; // prefix length
} bgpstream_prefix_t;


typedef enum {BST_STRING_ASPATH, BST_UINT32_ASPATH} bgpstream_aspath_type_t;

typedef struct struct_bgpstream_aspath_t {
  bgpstream_aspath_type_t type;
  uint8_t hop_count; // number of hops in the AS path
  union {
    char * str_aspath;
    uint32_t * numeric_aspath;
  };
} bgpstream_aspath_t;


typedef enum {BST_RIB, BST_ANNOUNCEMENT, BST_WITHDRAWAL, BST_STATE} bgpstream_elem_type_t;
typedef enum {BST_UNKNOWN, BST_IDLE, BST_CONNECT, BST_ACTIVE,
	      BST_OPENSENT, BST_OPENCONFIRM, BST_ESTABLISHED, 
	      BST_NULL} bgpstream_peer_state_t;


typedef struct struct_bgpstream_elem_t {

  // 0 rib, -1 withdraw, 1 announcement, 2 state
  bgpstream_elem_type_t type;

  // "time of record dump" (see -m option, default time in bgpdump.c file)
  long int timestamp;
  
  bgpstream_ip_address_t peer_address;  // peer IP address
  uint32_t peer_asnumber;           // peer AS number

  bgpstream_prefix_t prefix;
  
  bgpstream_ip_address_t nexthop;       // next hop ip address
  bgpstream_aspath_t aspath;           // aspath

  bgpstream_peer_state_t old_state;            // RIS peer status variables
  bgpstream_peer_state_t new_state;            // 6 = peer up,
  // != 6 peer is not up (either down, or waking up)

  struct struct_bgpstream_elem_t * next; 
  // forget about other attributes
} bgpstream_elem_t;


#endif /* _BGPSTREAM_ELEM_H */
