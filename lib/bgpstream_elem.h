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

typedef struct struct_bgpstream_ip_address_t {
  bgpstream_address_number_t address;
  uint8_t type; // 0 ipv4, 1 ipv6
} bgpstream_ip_address_t;



/* bgp_state_name  */
/* 0	"Unknown", */
/* 1	"Idle", */
/* 2	"Connect", */
/* 3	"Active", */
/* 4	"Opensent", */
/* 5	"Openconfirm", */
/* 6	"Established", */
/* 7	NULL */


typedef struct struct_bgpstream_elem_t {

  // 0 rib, -1 withdraw, 1 announcement, 2 state
  int type;

  // "time of record dump" (see -m option, default time in bgpdump.c file)
  long int timestamp;
  
  bgpstream_ip_address_t peer_address;  // peer IP address
  uint32_t peer_asnumber;           // peer AS number

  bgpstream_ip_address_t prefix;
  uint8_t prefix_len;     // prefix length
  
  bgpstream_ip_address_t nexthop;       // next hop ip address

  //  char aspath[1024];        // as path 
  // char origin_asnumber[1024]; // first prepended AS, i.e. AS announcing the prefix 

  char * aspath;          // as path 
  char * origin_asnumber; // first prepended AS, i.e. AS announcing the prefix 

  int old_state;            // RIS peer status variables
  int new_state;            // 6 = peer up, != 6 peer is not up (either down, or waking up)

  struct struct_bgpstream_elem_t * next; 
  // forget about other attributes
} bgpstream_elem_t;


#endif /* _BGPSTREAM_ELEM_H */
