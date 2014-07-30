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
 * libbgpstream is distributed in the hope that it will be usefuql,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bgpstream_elem.h"
#include<stdio.h>
#include<string.h>
#include <inttypes.h>


static void print_ip_address(bgpstream_ip_address_t * peer_address, char * peer_address_str){

  switch (peer_address->type){
  case 0:
    inet_ntop(AF_INET, &(peer_address->address.v4_addr), peer_address_str, INET6_ADDRSTRLEN);
    break;
  case 1:
    inet_ntop(AF_INET6, &(peer_address->address.v6_addr), peer_address_str, INET6_ADDRSTRLEN);
    break;
  default:
    printf("Error!\n");
  }

}

static char * print_aspath(char * ap_str, bgpstream_aspath_t aspath) {
  int i;
  char asn[16];
  if(aspath.type == BST_STRING_ASPATH) {
    ap_str = aspath.str_aspath;
  }
  else {
    // suppose 16 characters per as number + \0
    ap_str = (char *) malloc((sizeof(asn) * aspath.hop_count + 1) * sizeof(char) );
    //    memset(ap_str, 0, (sizeof(asn) * aspath.hop_count + 1));
    strcpy(ap_str,"");
    for(i = 0; i< aspath.hop_count; i++) {
      if(i == aspath.hop_count-1) {
	sprintf(asn, "%"PRIu32"", aspath.numeric_aspath[i]);      
      }
      else{
	sprintf(asn, "%"PRIu32" ", aspath.numeric_aspath[i]);      
      }
      strcat(ap_str, asn);
    }    
  }
  return ap_str;
}


/* print routing information from queue */

void print_elem_queue(bgpstream_elem_t * ri_queue) {
  bgpstream_elem_t * ri = ri_queue;
  uint32_t origin_asnumber;
  char * ap_str = NULL;
  while(ri != NULL) {
    char peer_address[INET6_ADDRSTRLEN];
    char prefix_address[INET6_ADDRSTRLEN];    
    char nexthop[INET6_ADDRSTRLEN];
    print_ip_address(&ri->peer_address, peer_address);
    if(ri->type != 2) {
      print_ip_address(&ri->prefix, prefix_address);      
      print_ip_address(&ri->nexthop, nexthop);      
    }     
    switch(ri->type) {
    case 0:
      origin_asnumber = -1;
      if(ri->aspath.type == BST_UINT32_ASPATH) {
	origin_asnumber = ri->aspath.numeric_aspath[ri->aspath.hop_count-1];
      }
      printf("RIB|%ld|%s|%u|%s/%d|%s|%"PRIu32"|%s|\n",
	     ri->timestamp, peer_address, ri->peer_asnumber, 
	     prefix_address, ri->prefix_len, 
	     print_aspath(ap_str, ri->aspath), origin_asnumber,
	     nexthop);
      if(ri->aspath.type == BST_UINT32_ASPATH) {
	free(ap_str);
      }
      break;
    case 1:
      origin_asnumber = -1;
      if(ri->aspath.type == BST_UINT32_ASPATH) {
	origin_asnumber = ri->aspath.numeric_aspath[ri->aspath.hop_count-1];
      }
      printf("ANNOUNCE|%ld|%s|%u|%s/%d|%s|%"PRIu32"|%s|\n",
	     ri->timestamp, peer_address, ri->peer_asnumber, 
	     prefix_address, ri->prefix_len, 
	     print_aspath(ap_str, ri->aspath), origin_asnumber,
	     nexthop);
      if(ri->aspath.type == BST_UINT32_ASPATH) {
	free(ap_str);
      }
      break;
    case -1:
      printf("WITHDRAWAL|%ld|%s|%u|%s/%d|\n",
	     ri->timestamp, peer_address, ri->peer_asnumber, 
	     prefix_address, ri->prefix_len);
      break;
    case 2:
      printf("STATE|%ld|%s|%u|\n", ri->timestamp, peer_address, ri->peer_asnumber);
      break;

    }
    ri = ri->next;
  }
}
