/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bs_format_mrt.h"
#include "bgpstream_format_interface.h"
#include "bgpstream_record_int.h"
#include "bgpdump_lib.h"
#include "bgpstream_elem_generator.h"
#include "bgpstream_log.h"
#include "utils.h"
#include <assert.h>


/* ==================== BGPDUMP JUNK ==================== */

#include "bgpstream_utils_as_path_int.h"
#include "bgpstream_utils_community_int.h"

/*
  Some code adapted from libbgpdump:

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

/* ribs related functions */
static int table_line_mrtd_route(bgpstream_elem_generator_t *self,
                                 BGPDUMP_ENTRY *entry)
{

  bgpstream_elem_t *ri;
  if ((ri = bgpstream_elem_generator_get_new_elem(self)) == NULL) {
    return -1;
  }

  // general
  ri->type = BGPSTREAM_ELEM_TYPE_RIB;
  ri->timestamp = entry->time;

  // peer and prefix
  if (entry->subtype == AFI_IP6) {
    ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->peer_address.ipv6 = entry->body.mrtd_table_dump.peer_ip.v6_addr;
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->prefix.address.ipv6 = entry->body.mrtd_table_dump.prefix.v6_addr;
  } else {
    ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->peer_address.ipv4 = entry->body.mrtd_table_dump.peer_ip.v4_addr;
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->prefix.address.ipv4 = entry->body.mrtd_table_dump.prefix.v4_addr;
  }
  ri->prefix.mask_len = entry->body.mrtd_table_dump.mask;
  ri->peer_asnumber = entry->body.mrtd_table_dump.peer_as;

  // as path
  if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
      entry->attr->aspath) {
    bgpstream_as_path_populate(ri->aspath, entry->attr->aspath);
  }

  // communities
  if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) &&
      entry->attr->community) {
    bgpstream_community_set_populate(ri->communities, entry->attr->community);
  }

  // nextop
  if ((entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI)) &&
      entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]) {
    ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->nexthop.ipv6 =
      entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop.v6_addr;
  } else {
    ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->nexthop.ipv4 = entry->attr->nexthop;
  }
  return 0;
}

int table_line_dump_v2_prefix(bgpstream_elem_generator_t *self,
                              BGPDUMP_ENTRY *entry)
{
  bgpstream_elem_t *ri;

  BGPDUMP_TABLE_DUMP_V2_PREFIX *e = &(entry->body.mrtd_table_dump_v2_prefix);
  int i;

  for (i = 0; i < e->entry_count; i++) {
    attributes_t *attr = e->entries[i].attr;
    if (!attr)
      continue;

    if ((ri = bgpstream_elem_generator_get_new_elem(self)) == NULL) {
      return -1;
    }

    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_RIB;
    ri->timestamp = entry->time;

    // peer
    if (e->entries[i].peer.afi == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = e->entries[i].peer.peer_ip.v4_addr;
    } else {
      if (e->entries[i].peer.afi == AFI_IP6) {
        ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
        ri->peer_address.ipv6 = e->entries[i].peer.peer_ip.v6_addr;
      } else {
        return -1;
      }
    }

    ri->peer_asnumber = e->entries[i].peer.peer_as;

    // prefix
    if (e->afi == AFI_IP) {
      ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->prefix.address.ipv4 = e->prefix.v4_addr;
    } else {
      if (e->afi == AFI_IP6) {
        ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV6;
        ri->prefix.address.ipv6 = e->prefix.v6_addr;
      }
    }
    ri->prefix.mask_len = e->prefix_length;
    // as path
    if (attr->aspath) {
      bgpstream_as_path_populate(ri->aspath, attr->aspath);
    }
    // communities
    if (attr->community) {
      bgpstream_community_set_populate(ri->communities, attr->community);
    }
    // next hop
    if ((attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI)) &&
        attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]) {
      ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->nexthop.ipv6 =
        attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->nexthop.v6_addr;
    } else {
      ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->nexthop.ipv4 = attr->nexthop;
    }
  }
  return 0;
}

static int table_line_announce(bgpstream_elem_generator_t *self,
                               struct prefix *prefix, int count,
                               BGPDUMP_ENTRY *entry)
{
  bgpstream_elem_t *ri;
  int idx;
  for (idx = 0; idx < count; idx++) {
    if ((ri = bgpstream_elem_generator_get_new_elem(self)) == NULL) {
      return -1;
    }

    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT;
    ri->timestamp = entry->time;
    // peer
    if (entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
    if (entry->body.zebra_message.address_family == AFI_IP) {
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
    if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
        entry->attr->aspath) {
      bgpstream_as_path_populate(ri->aspath, entry->attr->aspath);
    }
    // communities
    if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) &&
        entry->attr->community) {
      bgpstream_community_set_populate(ri->communities, entry->attr->community);
    }
  }
  return 0;
}

static int table_line_announce_1(bgpstream_elem_generator_t *self,
                                 struct mp_nlri *prefix, int count,
                                 BGPDUMP_ENTRY *entry)
{
  bgpstream_elem_t *ri;
  int idx;
  for (idx = 0; idx < count; idx++) {
    if ((ri = bgpstream_elem_generator_get_new_elem(self)) == NULL) {
      return -1;
    }

    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT;
    ri->timestamp = entry->time;
    // peer
    if (entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
    if (entry->body.zebra_message.address_family == AFI_IP) {
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
    if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
        entry->attr->aspath) {
      bgpstream_as_path_populate(ri->aspath, entry->attr->aspath);
    }
    // communities
    if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) &&
        entry->attr->community) {
      bgpstream_community_set_populate(ri->communities, entry->attr->community);
    }
  }
  return 0;
}

static int table_line_announce6(bgpstream_elem_generator_t *self,
                                struct mp_nlri *prefix, int count,
                                BGPDUMP_ENTRY *entry)
{
  bgpstream_elem_t *ri;
  int idx;
  for (idx = 0; idx < count; idx++) {
    if ((ri = bgpstream_elem_generator_get_new_elem(self)) == NULL) {
      return -1;
    }

    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT;
    ri->timestamp = entry->time;
    // peer
    if (entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
    if (entry->body.zebra_message.address_family == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.zebra_message.source_ip.v4_addr;
    }
    ri->peer_asnumber = entry->body.zebra_message.source_as;
    // prefix (ipv6)
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->prefix.address.ipv6 = prefix->nlri[idx].address.v6_addr;
    ri->prefix.mask_len = prefix->nlri[idx].len;
    // nexthop (ipv6)
    ri->nexthop.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->nexthop.ipv6 = prefix->nexthop.v6_addr;
    // aspath
    if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_AS_PATH) &&
        entry->attr->aspath) {
      bgpstream_as_path_populate(ri->aspath, entry->attr->aspath);
    }
    // communities
    if (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_COMMUNITIES) &&
        entry->attr->community) {
      bgpstream_community_set_populate(ri->communities, entry->attr->community);
    }
  }
  return 0;
}

static int table_line_withdraw(bgpstream_elem_generator_t *self,
                               struct prefix *prefix, int count,
                               BGPDUMP_ENTRY *entry)
{
  bgpstream_elem_t *ri;
  int idx;
  for (idx = 0; idx < count; idx++) {
    if ((ri = bgpstream_elem_generator_get_new_elem(self)) == NULL) {
      return -1;
    }

    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_WITHDRAWAL;
    ri->timestamp = entry->time;
    // peer
    if (entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
    if (entry->body.zebra_message.address_family == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.zebra_message.source_ip.v4_addr;
    }
    ri->peer_asnumber = entry->body.zebra_message.source_as;
    // prefix (ipv4)
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->prefix.address.ipv4 = prefix[idx].address.v4_addr;
    ri->prefix.mask_len = prefix[idx].len;
  }
  return 0;
}

static int table_line_withdraw6(bgpstream_elem_generator_t *self,
                                struct prefix *prefix, int count,
                                BGPDUMP_ENTRY *entry)
{
  bgpstream_elem_t *ri;
  int idx;
  for (idx = 0; idx < count; idx++) {
    if ((ri = bgpstream_elem_generator_get_new_elem(self)) == NULL) {
      return -1;
    }

    // general info
    ri->type = BGPSTREAM_ELEM_TYPE_WITHDRAWAL;
    ri->timestamp = entry->time;
    // peer
    if (entry->body.zebra_message.address_family == AFI_IP6) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
      ri->peer_address.ipv6 = entry->body.zebra_message.source_ip.v6_addr;
    }
    if (entry->body.zebra_message.address_family == AFI_IP) {
      ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
      ri->peer_address.ipv4 = entry->body.zebra_message.source_ip.v4_addr;
    }
    ri->peer_asnumber = entry->body.zebra_message.source_as;
    // prefix (ipv6)
    ri->prefix.address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->prefix.address.ipv6 = prefix[idx].address.v6_addr;
    ri->prefix.mask_len = prefix[idx].len;
  }
  return 0;
}

static int table_line_update(bgpstream_elem_generator_t *self,
                             BGPDUMP_ENTRY *entry)
{
  struct prefix *prefix;
  struct mp_nlri *prefix_mp;
  int count;

  // withdrawals (IPv4)
  if ((entry->body.zebra_message.withdraw_count) ||
      (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_UNREACH_NLRI))) {
    prefix = entry->body.zebra_message.withdraw;
    count = entry->body.zebra_message.withdraw_count;
    if (table_line_withdraw(self, prefix, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count) {
    prefix = entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST]->prefix_count;
    if (table_line_withdraw(self, prefix, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count) {
    prefix = entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->nlri;
    count =
      entry->attr->mp_info->withdraw[AFI_IP][SAFI_MULTICAST]->prefix_count;
    if (table_line_withdraw(self, prefix, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]
        ->prefix_count) {
    prefix =
      entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP][SAFI_UNICAST_MULTICAST]
              ->prefix_count;
    if (table_line_withdraw(self, prefix, count, entry) != 0) {
      return -1;
    }
  }

  // withdrawals (IPv6)
  if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count) {
    prefix = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST]->prefix_count;
    if (table_line_withdraw6(self, prefix, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count) {
    prefix = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->nlri;
    count =
      entry->attr->mp_info->withdraw[AFI_IP6][SAFI_MULTICAST]->prefix_count;
    if (table_line_withdraw6(self, prefix, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST] &&
      entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]
        ->prefix_count) {
    prefix =
      entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]->nlri;
    count = entry->attr->mp_info->withdraw[AFI_IP6][SAFI_UNICAST_MULTICAST]
              ->prefix_count;
    if (table_line_withdraw6(self, prefix, count, entry) != 0) {
      return -1;
    }
  }

  // announce
  if ((entry->body.zebra_message.announce_count) ||
      (entry->attr->flag & ATTR_FLAG_BIT(BGP_ATTR_MP_REACH_NLRI))) {
    prefix = entry->body.zebra_message.announce;
    count = entry->body.zebra_message.announce_count;
    if (table_line_announce(self, prefix, count, entry) != 0) {
      return -1;
    }
  }

  // announce 1
  if (entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST] &&
      entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST];
    count = entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST]->prefix_count;
    if (table_line_announce_1(self, prefix_mp, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST] &&
      entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST];
    count =
      entry->attr->mp_info->announce[AFI_IP][SAFI_MULTICAST]->prefix_count;
    if (table_line_announce_1(self, prefix_mp, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST] &&
      entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]
        ->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST];
    count = entry->attr->mp_info->announce[AFI_IP][SAFI_UNICAST_MULTICAST]
              ->prefix_count;
    if (table_line_announce_1(self, prefix_mp, count, entry) != 0) {
      return -1;
    }
  }

  // announce ipv6
  if (entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST] &&
      entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST];
    count = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST]->prefix_count;
    if (table_line_announce6(self, prefix_mp, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST] &&
      entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST];
    count =
      entry->attr->mp_info->announce[AFI_IP6][SAFI_MULTICAST]->prefix_count;
    if (table_line_announce6(self, prefix_mp, count, entry) != 0) {
      return -1;
    }
  }
  if (entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST] &&
      entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]
        ->prefix_count) {
    prefix_mp = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST];
    count = entry->attr->mp_info->announce[AFI_IP6][SAFI_UNICAST_MULTICAST]
              ->prefix_count;
    if (table_line_announce6(self, prefix_mp, count, entry) != 0) {
      return -1;
    }
  }

  return 0;
}

static int bgp_state_change(bgpstream_elem_generator_t *self,
                            BGPDUMP_ENTRY *entry)
{
  bgpstream_elem_t *ri;
  if ((ri = bgpstream_elem_generator_get_new_elem(self)) == NULL) {
    return -1;
  }

  // general information
  ri->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
  ri->timestamp = entry->time;
  // peer
  if (entry->body.zebra_message.address_family == AFI_IP6) {
    ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV6;
    ri->peer_address.ipv6 = entry->body.zebra_state_change.source_ip.v6_addr;
  }
  if (entry->body.zebra_message.address_family == AFI_IP) {
    ri->peer_address.version = BGPSTREAM_ADDR_VERSION_IPV4;
    ri->peer_address.ipv4 = entry->body.zebra_state_change.source_ip.v4_addr;
  }
  ri->peer_asnumber = entry->body.zebra_message.source_as;
  ri->old_state = entry->body.zebra_state_change.old_state;
  ri->new_state = entry->body.zebra_state_change.new_state;
  return 0;
}

static int populate_elem_generator(bgpstream_elem_generator_t *gen,
                                   BGPDUMP_ENTRY *bd_entry)
{
  /* mark the generator as having no elems */
  bgpstream_elem_generator_empty(gen);

  if (bd_entry == NULL) {
    return 0;
  }

  switch (bd_entry->type) {
  case BGPDUMP_TYPE_MRTD_TABLE_DUMP:
    return table_line_mrtd_route(gen, bd_entry);
    break;

  case BGPDUMP_TYPE_TABLE_DUMP_V2:
    return table_line_dump_v2_prefix(gen, bd_entry);
    break;

  case BGPDUMP_TYPE_ZEBRA_BGP:
    switch (bd_entry->subtype) {
    case BGPDUMP_SUBTYPE_ZEBRA_BGP_MESSAGE:
    case BGPDUMP_SUBTYPE_ZEBRA_BGP_MESSAGE_AS4:
      switch (bd_entry->body.zebra_message.type) {
      case BGP_MSG_UPDATE:
        return table_line_update(gen, bd_entry);
        break;
      }
      break;

    case BGPDUMP_SUBTYPE_ZEBRA_BGP_STATE_CHANGE: // state messages
    case BGPDUMP_SUBTYPE_ZEBRA_BGP_STATE_CHANGE_AS4:
      return bgp_state_change(gen, bd_entry);
      break;
    }
  }

  /* if we fall through to here, then we have a record that we don't extract any
     elems for, so don't return an error, just leave an empty elem generator */
  return 0;
}

/* ==================== END BGPDUMP JUNK ==================== */

#define STATE ((state_t*)(format->state))
#define FDATA ((BGPDUMP_ENTRY*)(record->__format_data->data))

typedef struct state {

  // bgpdump instance (TODO: replace with parsebgp instance)
  BGPDUMP *bgpdump;

  // elem generator instance
  bgpstream_elem_generator_t *elem_generator;

  // the total number of successful (filtered and not) reads
  uint64_t successful_read_cnt;

  // the number of non-filtered reads (i.e. "useful")
  uint64_t valid_read_cnt;

} state_t;

static int is_wanted_time(uint32_t record_time,
                          bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_interval_filter_t *tif;

  if (filter_mgr->time_intervals == NULL) {
    // no time filtering
    return 1;
  }

  tif = filter_mgr->time_intervals;

  while (tif != NULL) {
    if (record_time >= tif->begin_time &&
        (tif->end_time == BGPSTREAM_FOREVER || record_time <= tif->end_time)) {
      // matches a filter interval
      return 1;
    }
    tif = tif->next;
  }

  return 0;
}

/* ==================== PUBLIC API BELOW HERE ==================== */

int bs_format_mrt_create(bgpstream_format_t *format,
                         bgpstream_resource_t *res)
{
  BS_FORMAT_SET_METHODS(mrt, format);

  if ((format->state = malloc_zero(sizeof(state_t))) == NULL) {
    return -1;
  }

  if ((STATE->elem_generator = bgpstream_elem_generator_create()) == NULL) {
    return -1;
  }

  bgpstream_log(BGPSTREAM_LOG_FINE, "Opening %s", res->uri);
  if ((STATE->bgpdump = bgpdump_open_dump(format->transport)) == NULL) {
    return -1;
  }

  return 0;
}

bgpstream_format_status_t
bs_format_mrt_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{
  assert(record->__format_data->format == format);
  assert(FDATA == NULL);
  // DEBUG: testing an assumption:
  assert(record->dump_pos != BGPSTREAM_DUMP_END);
  uint64_t skipped_cnt = 0;

  while (1) {
    // read until we either get a successful read, or some kind of explicit error,
    // don't return if its a "normal" empty read (as happens when bgpdump reads
    // the peer index table in a RIB)
    while ((record->__format_data->data = bgpdump_read_next(STATE->bgpdump)) ==
           NULL) {
      // didn't read anything... why?
      if (STATE->bgpdump->corrupted_read) {
        record->status = BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD;
        return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
      }
      if (STATE->bgpdump->eof) {
        // just to be kind, set the record time to the dump time
        record->attributes.record_time = record->attributes.dump_time;

        if (skipped_cnt == 0) {
          // signal that the previous record really was the last in the dump
          record->dump_pos = BGPSTREAM_DUMP_END;
        }
        // was this the first thing we tried to read?
        if (STATE->successful_read_cnt == 0) {
          // then it is an empty file
          record->status = BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE;
          record->dump_pos = BGPSTREAM_DUMP_END;
          return BGPSTREAM_FORMAT_EMPTY_DUMP;
        }

        // so we managed to read some things, but did we get anything useful from
        // this file?
        if (STATE->valid_read_cnt == 0) {
          // dump contained data, but we filtered all of them out
          record->status = BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE;
          record->dump_pos = BGPSTREAM_DUMP_END;
          return BGPSTREAM_FORMAT_FILTERED_DUMP;
        }

        // otherwise, signal end of dump (record has not been filled)
        return BGPSTREAM_FORMAT_END_OF_DUMP;
      }
      // otherwise, just keep reading
    }
    assert(FDATA != NULL);

    // successful read, check the filters
    STATE->successful_read_cnt++;

    // check the filters
    if (is_wanted_time(FDATA->time, format->filter_mgr) != 0) {
      // we want this entry
      STATE->valid_read_cnt++;
      break;
    } else {
      // we dont want this entry, destroy it
      bgpdump_free_mem(FDATA);
      skipped_cnt++;
      // fall through and repeat loop
    }
  }

  // the only thing left is a good, valid read
  record->status = BGPSTREAM_RECORD_STATUS_VALID_RECORD;

  // if this is the first record we read and no previous
  // valid record has been discarded because of time
  if (STATE->valid_read_cnt == 1 && STATE->successful_read_cnt == 1) {
    record->dump_pos = BGPSTREAM_DUMP_START;
  } else {
    record->dump_pos = BGPSTREAM_DUMP_MIDDLE;
    // NB when the *next* record is pre-fetched, this may be changed to
    // end-of-dump by the reader (since we'll discover that there are no more
    // records)
  }

  // update the record time
  record->attributes.record_time = FDATA->time;

  // we successfully read a record, return it
  return BGPSTREAM_FORMAT_OK;
}

int bs_format_mrt_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  if (bgpstream_elem_generator_is_populated(STATE->elem_generator) == 0 &&
      populate_elem_generator(STATE->elem_generator, FDATA) != 0) {
    return -1;
  }
  *elem = bgpstream_elem_generator_get_next_elem(STATE->elem_generator);
  if (*elem == NULL) {
    return 0;
  }
  return 1;
}

void bs_format_mrt_destroy_data(bgpstream_format_t *format, void *data)
{
  bgpstream_elem_generator_clear(STATE->elem_generator);
  bgpdump_free_mem((BGPDUMP_ENTRY*)data);
}

void bs_format_mrt_destroy(bgpstream_format_t *format)
{
  bgpdump_close_dump(STATE->bgpdump);
  STATE->bgpdump = NULL;

  bgpstream_elem_generator_destroy(STATE->elem_generator);
  STATE->elem_generator = NULL;

  free(format->state);
  format->state = NULL;
}
