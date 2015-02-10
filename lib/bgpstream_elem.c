/*
 Copyright (c) 2007 - 2010 RIPE NCC - All Rights Reserved

 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted, provided
 that the above copyright notice appear in all copies and that both that
 copyright notice and this permission notice appear in supporting
 documentation, and that the name of the author not be used in advertising or
 publicity pertaining to distribution of the software without specific,
 written prior permission.

 THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS; IN NO EVENT SHALL
 AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

Parts of this code have been engineered after analiyzing GNU Zebra's
source code and therefore might contain declarations/code from GNU
Zebra, Copyright (C) 1999 Kunihiro Ishiguro. Zebra is a free routing
software, distributed under the GNU General Public License. A copy of
this license is included with libbgpdump.
Original Author: Shufu Mao(msf98@mails.tsinghua.edu.cn)
*/

/*
 * file further modified by
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

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "bgpdump_lib.h"
#include "utils.h"

#include "bgpstream_utils.h"

#include "bgpstream_debug.h"
#include "bgpstream_record.h"

#include "bgpstream_elem.h"

/* route info create and destroy methods */
static bgpstream_elem_t *bd2bi_create_route_info() {
  // allocate memory for new element
  bgpstream_elem_t * ri =
    (bgpstream_elem_t *) malloc_zero(sizeof(bgpstream_elem_t));
  // all fields are initialized to zero
  ri->next = NULL;
  return ri;
}

static bgpstream_elem_t *bd2bi_add_new_route_info(bgpstream_elem_t **lifo_queue) {
  bgpstream_elem_t * ri = bd2bi_create_route_info();
  if(ri == NULL) {
    return NULL;
  }
  ri->next = *lifo_queue;
  *lifo_queue = ri;
  return ri;
}

static void bd2bi_destroy_route_info(bgpstream_elem_t *ri) {
  if(ri != NULL) {
    if(ri->aspath.type == BL_AS_NUMERIC)
      {
	if(ri->aspath.numeric_aspath != NULL)
	  {
	    free(ri->aspath.numeric_aspath);
	  }
      }
    if(ri->aspath.type == BL_AS_STRING)
      {
	if(ri->aspath.str_aspath != NULL)
	  {
	    free(ri->aspath.str_aspath);
	  }
      }
    free(ri);
  }
}

static void bd2bi_destroy_route_info_queue(bgpstream_elem_t * lifo_queue) {
  if(lifo_queue == NULL) {
    return;
  }
  bgpstream_elem_t * ri = lifo_queue;
  while(lifo_queue != NULL) {
    ri = lifo_queue;
    lifo_queue = ri->next;
    bd2bi_destroy_route_info(ri);
  }
}

static void get_aspath_struct(struct aspath *ap,
                              bl_aspath_storage_t *ap_struct)
{
  const char *invalid_characters = "([{}])";
  // char origin_copy[16];
  uint8_t it;
  char * tok = NULL;
  char *c = ap->str;
  char *next;
  ap_struct->hop_count = ap->count;
  ap_struct->type = BL_AS_TYPE_UNKNOWN;

  if(ap->str == NULL || ap_struct->hop_count == 0) {
    // aspath is empty, if it is an internal AS bgp info that is fine
    return;
  }
  // check if there are sets or confederations
  while (*c) {
    if(strchr(invalid_characters, *c)) {
      ap_struct->type = BL_AS_STRING;
      ap_struct->str_aspath = NULL;
      break;
    }
    c++;
  }
  /* if sets or confederations are present, then
   * the AS_PATH is of type STRING */
  if(ap_struct->type == BL_AS_STRING) {
    /* if the type is STR then we allocate the memory
     * required for the path - we do not copy ap->str
     * it is a fixed length array which is unreasonably
     long (8000)*/
    ap_struct->str_aspath = strdup(ap->str);
  }

  /* if type has not been changed, then it is  numeric, then  */

  if(ap_struct->type == BL_AS_TYPE_UNKNOWN)
    {
      ap_struct->type =  BL_AS_NUMERIC;
      it = 0;
      ap_struct->numeric_aspath = (uint32_t *)malloc(ap_struct->hop_count * sizeof(uint32_t));
      if(ap_struct->numeric_aspath == NULL) {
	bgpstream_log_err("get_aspath_struct: can't malloc aspath numeric array");
	return;
      }

      next = c = strdup(ap->str);
      while((tok = strsep(&next, " ")) != NULL) {
	// strcpy(origin_copy, tok);
	ap_struct->numeric_aspath[it] = strtoul(tok, NULL, 10);
	it++;
      }
      free(c);
    }

}

/* ribs related functions */
static bgpstream_elem_t *table_line_mrtd_route(BGPDUMP_ENTRY *entry) {
  bgpstream_elem_t * ri  =  bd2bi_create_route_info();
  if (ri == NULL) {
    return NULL;
  }
  // general
  ri->type = BGPSTREAM_ELEM_TYPE_RIB;
  ri->timestamp = entry->time;

  // peer and prefix
#ifdef BGPDUMP_HAVE_IPV6
  if (entry->subtype == AFI_IP6) {
    ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->peer_address.ipv6 = entry->body.mrtd_table_dump.peer_ip.v6_addr;
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->prefix.address.ipv6 = entry->body.mrtd_table_dump.prefix.v6_addr;
  }
  else
#endif
    {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.mrtd_table_dump.peer_ip.v4_addr;
      ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->prefix.address.ipv4 = entry->body.mrtd_table_dump.prefix.v4_addr;
    }
  ri->prefix.mask_len = entry->body.mrtd_table_dump.mask;
  ri->peer_asnumber = entry->body.mrtd_table_dump.peer_as;

  // as path
  if(entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
     entry->attr->aspath && entry->attr->aspath->str) {
    get_aspath_struct(entry->attr->aspath, &ri->aspath);
  }

  // nextop
#ifdef BGPDUMP_HAVE_IPV6
    if ((entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI)) &&
	entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]) {
      ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->nexthop.ipv6 = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop.v6_addr;
    }
    else
#endif
      {
	ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV4;
	ri->nexthop.ipv4 = entry->attr->nexthop;
      }
  return ri;
}

static bgpstream_elem_t *table_line_dump_v2_prefix(BGPDUMP_ENTRY *entry) {
  bgpstream_elem_t *ri_queue = NULL;
  bgpstream_elem_t *ri;
  BGPDUMP_TABLE_DUMP_V2_PREFIX *e = &(entry->body.mrtd_table_dump_v2_prefix);
  int i;

  for(i = 0; i < e->entry_count; i++) {
    attributes_t *attr = e->entries[i].attr;
    if(! attr)
      continue;

    ri = bd2bi_add_new_route_info(&ri_queue);
    if(ri == NULL) {
      // warning
      return ri_queue;
    }

    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_RIB;
    ri->timestamp = entry->time;

    // peer
    if(e->entries[i].peer.afi == AFI_IP){
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = e->entries[i].peer.peer_ip.v4_addr;
    }
#ifdef BGPDUMP_HAVE_IPV6
    else {
      if(e->entries[i].peer.afi == AFI_IP6){
	ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
	ri->peer_address.ipv6 = e->entries[i].peer.peer_ip.v6_addr;
      }
      else {
	printf("ERROR____bgpstream-elem__________\n");
      }
    }
#endif

    ri->peer_asnumber = e->entries[i].peer.peer_as;

    // prefix
    if(e->afi == AFI_IP) {
      ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->prefix.address.ipv4 = e->prefix.v4_addr;
    }

#ifdef BGPDUMP_HAVE_IPV6
    else{
      if(e->afi == AFI_IP6) {
	ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV6;
	ri->prefix.address.ipv6 = e->prefix.v6_addr;
      }
    }
#endif
    ri->prefix.mask_len = e->prefix_length;
    // as path
    if (attr->aspath) {
      get_aspath_struct(attr->aspath, &ri->aspath);
    }
    // next hop
 #ifdef BGPDUMP_HAVE_IPV6
    if ((attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI)) &&
	attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]) {
      ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->nexthop.ipv6 = attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop.v6_addr;
    }
    else
#endif
      {
      ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->nexthop.ipv4 = attr->nexthop;
      }

  }
  return ri_queue;
}

static void concatenate_queues(bgpstream_elem_t **total,
                               bgpstream_elem_t **new) {
  bgpstream_elem_t * r;
  r = *total;
  if(*total == NULL) {
    *total = *new;
  }
  else {
    while(r->next != NULL) {
      r = r->next;
    }
    r->next = *new;
  }
}

static bgpstream_elem_t *table_line_announce(struct prefix *prefix, int count,
                                             BGPDUMP_ENTRY *entry){
  bgpstream_elem_t * ri_queue = NULL;
  bgpstream_elem_t * ri;
  int idx;
  for (idx=0;idx<count;idx++) {
    ri  =  bd2bi_add_new_route_info(&ri_queue);
    if(ri == NULL) {
      // warning
      return ri_queue;
    }
    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT;
    ri->timestamp = entry->time;
    // peer
#ifdef BGPDUMP_HAVE_IPV6
    if(entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
#endif
    if(entry->body.zebra_message.address_family == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.zebra_message.source_ip.v4_addr;
    }
    ri->peer_asnumber = entry->body.zebra_message.source_as;
    // prefix (ipv4)
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->prefix.address.ipv4 = prefix[idx].address.v4_addr;
    ri->prefix.mask_len = prefix[idx].len;
    // nexthop (ipv4)
    ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->nexthop.ipv4 = entry->attr->nexthop;
    // as path
    if(entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
       entry->attr->aspath && entry->attr->aspath->str) {
      get_aspath_struct(entry->attr->aspath, &ri->aspath);
    }
  }
  return ri_queue;
}

static bgpstream_elem_t *table_line_announce_1(struct mp_nlri *prefix,
                                               int count,
                                               BGPDUMP_ENTRY *entry) {
  bgpstream_elem_t * ri_queue = NULL;
  bgpstream_elem_t * ri;
  int idx;
  for (idx=0;idx<count;idx++) {
    ri  =  bd2bi_add_new_route_info(&ri_queue);
    if(ri == NULL) {
      // warning
      return ri_queue;
    }
    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT;
    ri->timestamp = entry->time;
    // peer
#ifdef BGPDUMP_HAVE_IPV6
    if(entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
#endif
    if(entry->body.zebra_message.address_family == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.zebra_message.source_ip.v4_addr;
    }
    ri->peer_asnumber = entry->body.zebra_message.source_as;
      // prefix (ipv4)
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->prefix.address.ipv4 = prefix->nlri[idx].address.v4_addr;
    ri->prefix.mask_len = prefix->nlri[idx].len;
      // nexthop (ipv4)
    ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->nexthop.ipv4 = entry->attr->nexthop;
    // as path
    if(entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
       entry->attr->aspath && entry->attr->aspath->str) {
      get_aspath_struct(entry->attr->aspath, &ri->aspath);
    }
  }
  return ri_queue;
}

#ifdef BGPDUMP_HAVE_IPV6
static bgpstream_elem_t *table_line_announce6(struct mp_nlri *prefix,
                                              int count,
                                              BGPDUMP_ENTRY *entry) {
  bgpstream_elem_t * ri_queue = NULL;
  bgpstream_elem_t * ri;
  int idx;
  for (idx=0;idx<count;idx++) {
    ri = bd2bi_add_new_route_info(&ri_queue);
    if(ri == NULL) {
      // warning
      return ri_queue;
    }
    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT;
    ri->timestamp = entry->time;
    // peer
#ifdef BGPDUMP_HAVE_IPV6
    if(entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
#endif
    if(entry->body.zebra_message.address_family == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.zebra_message.source_ip.v4_addr;
    }
    ri->peer_asnumber = entry->body.zebra_message.source_as;
    // prefix (ipv6)
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->prefix.address.ipv6 = prefix->nlri[idx].address.v6_addr;
    ri->prefix.mask_len = prefix->nlri[idx].len;
    //nexthop (ipv6)
    ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->nexthop.ipv6 = prefix->nexthop.v6_addr;
    // aspath
    if(entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
       entry->attr->aspath && entry->attr->aspath->str) {
      get_aspath_struct(entry->attr->aspath, &ri->aspath);
    }
  }
  return ri_queue;
}
#endif

static bgpstream_elem_t *table_line_withdraw(struct prefix *prefix,
                                             int count,
                                             BGPDUMP_ENTRY *entry) {
  bgpstream_elem_t * ri_queue = NULL;
  bgpstream_elem_t * ri;
  int idx;
  for(idx=0;idx<count;idx++) {
    ri = bd2bi_add_new_route_info(&ri_queue);
    if(ri == NULL) {
      // warning
      return ri_queue;
    }
    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_WITHDRAWAL;
    ri->timestamp = entry->time;
    // peer
#ifdef BGPDUMP_HAVE_IPV6
    if(entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
#endif
    if(entry->body.zebra_message.address_family == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.zebra_message.source_ip.v4_addr;
    }
    ri->peer_asnumber = entry->body.zebra_message.source_as;
    // prefix (ipv4)
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->prefix.address.ipv4 = prefix[idx].address.v4_addr;
    ri->prefix.mask_len = prefix[idx].len;
  }
  return ri_queue;
}

#ifdef BGPDUMP_HAVE_IPV6
static bgpstream_elem_t *table_line_withdraw6(struct prefix *prefix,
                                               int count,
                                               BGPDUMP_ENTRY *entry) {
  bgpstream_elem_t * ri_queue = NULL;
  bgpstream_elem_t * ri;
  int idx;
  for (idx=0;idx<count;idx++) {
    ri  =  bd2bi_add_new_route_info(&ri_queue);
    if(ri == NULL) {
      // warning
      return ri_queue;
    }
    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_WITHDRAWAL;
    ri->timestamp = entry->time;
    // peer
    if(entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
    if(entry->body.zebra_message.address_family == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.zebra_message.source_ip.v4_addr;
    }
    ri->peer_asnumber = entry->body.zebra_message.source_as;
    // prefix (ipv6)
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->prefix.address.ipv6 = prefix[idx].address.v6_addr;
    ri->prefix.mask_len = prefix[idx].len;
  }
  return ri_queue;
}
#endif

static bgpstream_elem_t *table_line_update(BGPDUMP_ENTRY *entry) {
  struct prefix *prefix;
  struct mp_nlri *prefix_mp;
  int count;
  bgpstream_elem_t * update = NULL;
  bgpstream_elem_t * current = NULL;

  // withdrawals (IPv4)
  if ((entry->body.zebra_message.withdraw_count) ||
      (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_UNREACH_NLRI)))  {
    prefix = entry->body.zebra_message.withdraw;
    count = entry->body.zebra_message.withdraw_count;
    current = table_line_withdraw(prefix, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count){
    prefix = entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count;
    current = table_line_withdraw(prefix, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count) {
    prefix = entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count;
    current = table_line_withdraw(prefix, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count) {
    prefix = entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count;
    current = table_line_withdraw(prefix, count, entry);
    concatenate_queues(&update, &current);
  }

  // withdrawals (IPv6)
#ifdef BGPDUMP_HAVE_IPV6
  if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count) {
    prefix = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count;
    current = table_line_withdraw6(prefix, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count) {
    prefix = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count;
    current = table_line_withdraw6(prefix, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count) {
    prefix =  entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count;
    current = table_line_withdraw6(prefix, count, entry);
    concatenate_queues(&update, &current);
  }
#endif

  // announce
  if ( (entry->body.zebra_message.announce_count) ||
       (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI))) {
    prefix = entry->body.zebra_message.announce;
    count = entry->body.zebra_message.announce_count;
    current = table_line_announce(prefix, count, entry);
    concatenate_queues(&update, &current);
  }

  // announce 1
  if (entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST] &&
      entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST];
    count = entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->prefix_count;
    current = table_line_announce_1(prefix_mp, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST] &&
      entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST];
    count = entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->prefix_count;
    current = table_line_announce_1(prefix_mp, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST] &&
      entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST];
    count = entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]->prefix_count;
    current = table_line_announce_1(prefix_mp, count, entry);
    concatenate_queues(&update, &current);
  }

  // announce ipv 6
#ifdef BGPDUMP_HAVE_IPV6
  if (entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST] &&
      entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST];
    count = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->prefix_count;
    current = table_line_announce6(prefix_mp, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST] &&
      entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST];
    count = entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->prefix_count;
    current = table_line_announce6(prefix_mp, count, entry);
    concatenate_queues(&update, &current);
  }
  if (entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST] &&
      entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST];
    count = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]->prefix_count;
    current = table_line_announce6(prefix_mp, count, entry);
    concatenate_queues(&update, &current);
  }
#endif
  return update;
}

static bgpstream_elem_t *bgp_state_change(BGPDUMP_ENTRY *entry) {
  bgpstream_elem_t * ri_queue = NULL;
  bgpstream_elem_t * ri;
  ri  =  bd2bi_add_new_route_info(&ri_queue);
  if(ri == NULL) {
    // warning
    return ri_queue;
  }
  // general information
  ri->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
  ri->timestamp = entry->time;
  // peer
#ifdef BGPDUMP_HAVE_IPV6
  if(entry->body.zebra_message.address_family == AFI_IP6) {
    ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->peer_address.ipv6 = entry->body.zebra_state_change.source_ip.v6_addr;
  }
#endif
  if(entry->body.zebra_message.address_family == AFI_IP) {
    ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->peer_address.ipv4 = entry->body.zebra_state_change.source_ip.v4_addr;
  }
  ri->peer_asnumber = entry->body.zebra_message.source_as;
  ri->old_state = entry->body.zebra_state_change.old_state;
  ri->new_state = entry->body.zebra_state_change.new_state;
  return ri_queue;
}


/* ==================== PUBLIC FUNCTIONS ==================== */

/* get routing information from entry */
bgpstream_elem_t *bgpstream_elem_queue_create(bgpstream_record_t *bs_record) {

  if(bs_record == NULL || bs_record->bd_entry == NULL ||
     bs_record->status != VALID_RECORD) {
    return NULL;
  }
  switch(bs_record->bd_entry->type) {
  case BGPDUMP_TYPE_MRTD_TABLE_DUMP:
    return table_line_mrtd_route(bs_record->bd_entry);
    break;
  case BGPDUMP_TYPE_TABLE_DUMP_V2:
    return table_line_dump_v2_prefix(bs_record->bd_entry);
    break;
  case BGPDUMP_TYPE_ZEBRA_BGP:
    switch(bs_record->bd_entry->subtype) {
    case BGPDUMP_SUBTYPE_ZEBRA_BGP_MESSAGE:
    case BGPDUMP_SUBTYPE_ZEBRA_BGP_MESSAGE_AS4:
      switch(bs_record->bd_entry->body.zebra_message.type) {
      case BGP_MSG_UPDATE:
	return table_line_update(bs_record->bd_entry);
	break;
      }
      break;
    case BGPDUMP_SUBTYPE_ZEBRA_BGP_STATE_CHANGE:        // state messages
    case BGPDUMP_SUBTYPE_ZEBRA_BGP_STATE_CHANGE_AS4:
      return bgp_state_change(bs_record->bd_entry);
      break;
    }
  }
  return NULL;
}


void bgpstream_elem_queue_destroy(bgpstream_elem_t * elem_queue) {
  bd2bi_destroy_route_info_queue(elem_queue);
}

int bgpstream_elem_type_snprint(char * restrict buf, size_t len,
                                bgpstream_elem_type_t type)
{
  /* ensure we have enough bytes to write our single character */
  if(len == 0) {
    return 1;
  } else if(len == 1) {
    buf[0] = '\0';
    return 1;
  }

  switch(type)
    {
    case BGPSTREAM_ELEM_TYPE_RIB:
      buf[0] = 'R';
      break;

    case BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT:
      buf[0] = 'A';
      break;

    case BGPSTREAM_ELEM_TYPE_WITHDRAWAL:
      buf[0] = 'W';
      break;

    case BGPSTREAM_ELEM_TYPE_PEERSTATE:
      buf[0] = 'S';
      break;

    default:
      buf[0] = '\0';
      break;
    }

  buf[1] = '\0';
  return 1;
}

int bgpstream_elem_peerstate_snprint(char * restrict buf, size_t len,
                                     bgpstream_elem_peerstate_t state)
{
  size_t written = 0;

  switch(state)
    {
    case BGPSTREAM_ELEM_PEERSTATE_IDLE:
      strncpy(buf, "IDLE", len);
      written = strlen("IDLE");
      break;

    case BGPSTREAM_ELEM_PEERSTATE_CONNECT:
      strncpy(buf, "CONNECT", len);
      written = strlen("CONNECT");
      break;

    case BGPSTREAM_ELEM_PEERSTATE_ACTIVE:
      strncpy(buf, "ACTIVE", len);
      written = strlen("ACTIVE");
      break;

    case BGPSTREAM_ELEM_PEERSTATE_OPENSENT:
      strncpy(buf, "OPENSENT", len);
      written = strlen("OPENSENT");
      break;

    case BGPSTREAM_ELEM_PEERSTATE_OPENCONFIRM:
      strncpy(buf, "OPENCONFIRM", len);
      written = strlen("OPENCONFIRM");
      break;

    case BGPSTREAM_ELEM_PEERSTATE_ESTABLISHED:
      strncpy(buf, "ESTABLISHED", len);
      written = strlen("ESTABLISHED");
      break;

    default:
      if(len > 0) {
        buf[0] = '\0';
      }
      break;
    }

  /* we promise to always nul-terminate */
  if(written > len) {
    buf[len-1] = '\0';
  }

  return written;
}


int bgpstream_elem_snprint(char * restrict buf, size_t len,
                           bgpstream_elem_t *elem)
{
  assert(elem);

  size_t written = 0;
  size_t n = 0;
  char *buf_p = buf;

  /** @todo remove all these things */
  /** @todo use a single buffer and write into it using each func */
  char *tmp1 = NULL;
  char *tmp2 = NULL;
  char tmp_buf[1024];
  char addr_buf[INET6_ADDRSTRLEN];
  char pfx_buf[INET6_ADDRSTRLEN+3];

  bl_as_storage_t a;

  /* common fields */

  // timestamp|peer_ip|peer_asn|message_type|
  bgpstream_elem_type_snprint(tmp_buf, 1024, elem->type);

  bgpstream_addr_ntop(addr_buf, INET6_ADDRSTRLEN, &elem->peer_address);
  written = snprintf(buf_p, len, "%"PRIu32"|%s|%"PRIu32"|%s|",
                     elem->timestamp,
                     addr_buf,
                     elem->peer_asnumber,
                     tmp_buf
                     );

  /** @todo remove these too */
  free(tmp1);
  tmp1 = NULL;

  buf_p += written;

  /* conditional fields */
  switch(elem->type)
    {
    case BGPSTREAM_ELEM_TYPE_RIB:
    case BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT:

      a = bl_get_origin_as(&elem->aspath);
      bgpstream_addr_ntop(addr_buf, INET6_ADDRSTRLEN, &elem->nexthop);
      bgpstream_pfx_snprint(pfx_buf, INET6_ADDRSTRLEN+3,
                             (bgpstream_pfx_t*)&(elem->prefix));

      snprintf(buf_p, (len-written), "%s|%s|%s|%s",
               pfx_buf,
               addr_buf,
               (tmp1 = bl_print_aspath(&(elem->aspath))),
               (tmp2 = bl_print_as(&a)));
      break;

    case BGPSTREAM_ELEM_TYPE_WITHDRAWAL:
      bgpstream_pfx_snprint(pfx_buf, INET6_ADDRSTRLEN+3,
                            (bgpstream_pfx_t*)&(elem->prefix));
      snprintf(buf_p, (len-written), "%s", pfx_buf);
      break;

    case BGPSTREAM_ELEM_TYPE_PEERSTATE:
      n = bgpstream_elem_peerstate_snprint(buf_p, (len-written),
                                           elem->old_state);
      written += n;
      buf_p += n;
      if(len-written > 0)
        {
          *buf_p = '|';
          buf_p++;
        }
      written++;

      n = bgpstream_elem_peerstate_snprint(buf_p, (len-written),
                                           elem->new_state);
      written += n;
      buf_p += n;
      break;
    default:
      fprintf(stderr, "Error during elem processing\n");
    }

  free(tmp1);
  tmp1 = NULL;
  free(tmp2);
  tmp2 = NULL;

  return written;

}

