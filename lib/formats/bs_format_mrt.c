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
#include "bgpstream_utils_as_path_int.h"
#include "bgpstream_utils_community_int.h"
#include "bgpstream_elem_generator.h"
#include "bgpstream_log.h"
#include "utils.h"
#include "parsebgp.h"
#include <assert.h>

#define STATE ((state_t*)(format->state))
#define FDATA ((parsebgp_msg_t*)(record->__format_data->data))

// read in chunks of 1MB to minimize the number of partial parses we end up
// doing.  this is also the same length as the wandio thread buffer, so this
// might help reduce the time waiting for locks
#define BUFLEN 1024 * 1024

typedef struct peer_index_entry {

  /** Peer ASN */
  uint32_t peer_asn;

  /** Peer IP */
  bgpstream_addr_storage_t peer_ip;

} peer_index_entry_t;

KHASH_INIT(td2_peer, int, peer_index_entry_t, 1, kh_int_hash_func,
           kh_int_hash_equal);

typedef struct state {

  // options for libparsebgp
  parsebgp_opts_t parser_opts;

  // raw data buffer
  // TODO: once parsebgp supports reading using a read callback, just pass the
  // transport callback to the parser
  uint8_t buffer[BUFLEN];

  // number of bytes left to read in the buffer
  size_t remain;

  // pointer into buffer
  uint8_t *ptr;

  // elem generator instance
  bgpstream_elem_generator_t *gen;

  // the total number of successful (filtered and not) reads
  uint64_t successful_read_cnt;

  // the number of non-filtered reads (i.e. "useful")
  uint64_t valid_read_cnt;

  // state to store the "peer index table" when reading TABLE_DUMP_V2 records
  khash_t(td2_peer) *peer_table;

} state_t;

// TODO move lots of this code into a common parsebgp helper module so that we
// can reuse it for the BMP format

#define COPY_IP(dst, afi, src, do_unknown)                                     \
  do {                                                                         \
    switch (afi) {                                                             \
    case PARSEBGP_BGP_AFI_IPV4:                                                \
      (dst)->version = BGPSTREAM_ADDR_VERSION_IPV4;                            \
      memcpy(&(dst)->ipv4, src, 4);                                            \
      break;                                                                   \
                                                                               \
    case PARSEBGP_BGP_AFI_IPV6:                                                \
      (dst)->version = BGPSTREAM_ADDR_VERSION_IPV6;                            \
      memcpy(&(dst)->ipv6, src, 16);                                           \
      break;                                                                   \
                                                                               \
    default:                                                                   \
      do_unknown;                                                              \
    }                                                                          \
  } while (0)

static bgpstream_as_path_seg_type_t as_path_types[] = {
  BGPSTREAM_AS_PATH_SEG_INVALID, // INVALID
  BGPSTREAM_AS_PATH_SEG_SET, // PARSEBGP_BGP_UPDATE_AS_PATH_SEG_AS_SET
  BGPSTREAM_AS_PATH_SEG_ASN, // PARSEBGP_BGP_UPDATE_AS_PATH_SEG_AS_SEQ
  BGPSTREAM_AS_PATH_SEG_CONFED_SET, // PARSEBGP_BGP_UPDATE_AS_PATH_SEG_CONFED_SET
  BGPSTREAM_AS_PATH_SEG_CONFED_SEQ, // PARSEBGP_BGP_UPDATE_AS_PATH_SEG_CONFED_SEQ
};

static int append_segments(bgpstream_as_path_t *bs_path,
                           parsebgp_bgp_update_as_path_t *pbgp_path,
                           int asns_cnt)
{
  int i;
  int effective_cnt = 0;
  int to_append = 0;
  int appended = 0;
  parsebgp_bgp_update_as_path_seg_t *seg;

  if (asns_cnt == 0) {
    return 0;
  }

  for (i = 0; i < pbgp_path->segs_cnt; i++) {
    seg = &pbgp_path->segs[i];

    // how many ASNs does this segment count for in the path merging algorithm?
    // RFC 4271 has some tricky rules for how we should count segments
    if (seg->type == PARSEBGP_BGP_UPDATE_AS_PATH_SEG_AS_SEQ) {
      effective_cnt = seg->asns_cnt;
    } else if (seg->type == PARSEBGP_BGP_UPDATE_AS_PATH_SEG_AS_SET) {
      effective_cnt = 1;
    } else {
      effective_cnt = 0;
    }

    // ok, now that we know how many "ASNs" this segment counts for, how many of
    // its actual ASNs should we append?
    if (effective_cnt <= 1 || seg->asns_cnt <= (asns_cnt - appended)) {
      // its a special segment, or we have enough "budget" to append all of its
      // ASNs
      to_append = seg->asns_cnt;
    } else {
      // we need to append only a subset of its ASNs
      to_append = asns_cnt - appended;
    }
    assert(to_append <= seg->asns_cnt && to_append > 0);

    // ensure we're not going to wander off the end of as_path_types array
    if (seg->type < PARSEBGP_BGP_UPDATE_AS_PATH_SEG_AS_SET ||
        seg->type > PARSEBGP_BGP_UPDATE_AS_PATH_SEG_CONFED_SEQ) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Unknown AS Path segment type %d",
                    seg->type);
      return -1;
    }

    if (bgpstream_as_path_append(bs_path, as_path_types[seg->type], seg->asns,
                                 to_append) != 0) {
      return -1;
    }

    if (seg->type == PARSEBGP_BGP_UPDATE_AS_PATH_SEG_AS_SEQ) {
      // we appended "to_append" ASNs
      appended += to_append;
    } else {
      // in these special segments, we append the "effective_cnt"
      appended += effective_cnt;
    }
    // have we reached our budget?
    if (appended == asns_cnt) {
      break;
    }
    assert(appended < asns_cnt);
  }
  return 0;
}

// this is just an optimized version of the above function. there is some
// repeated code, but this makes things easier to maintain because it simplifies
// both cases (and perhaps yields faster code for the (overwhelmingly) common
// case.
static int append_segments_all(bgpstream_as_path_t *bs_path,
                               parsebgp_bgp_update_as_path_t *pbgp_path)
{
  int i;
  parsebgp_bgp_update_as_path_seg_t *seg;

  for (i = 0; i < pbgp_path->segs_cnt; i++) {
    seg = &pbgp_path->segs[i];
    if (bgpstream_as_path_append(bs_path, as_path_types[seg->type], seg->asns,
                                 seg->asns_cnt) != 0) {
      return -1;
    }
  }
  return 0;
}

static int handle_as_paths(bgpstream_as_path_t *path,
                           parsebgp_bgp_update_as_path_t *aspath,
                           parsebgp_bgp_update_as_path_t *as4path)
{
  bgpstream_as_path_clear(path);

  // common case?
  if (aspath && !as4path) {
    return append_segments_all(path, aspath);
  }

  // merge case?
  if (as4path && aspath && aspath->asns_cnt >= as4path->asns_cnt) {
    bgpstream_log(BGPSTREAM_LOG_FINE, "Merging AS_PATH (%d) and AS4_PATH (%d)",
                  aspath->asns_cnt, as4path->asns_cnt);
    // copy <diff> ASNs from AS_PATH into our new path and then copy ALL ASNs
    // from AS4_PATH into our new path
    if (append_segments(path, aspath, (aspath->asns_cnt - as4path->asns_cnt)) !=
          0 ||
        append_segments_all(path, as4path) != 0) {
      return -1;
    }
    return 0;
  }

  // rare: both (or neither) are present, but can't trust AS4_PATH
  if (aspath) {
    return append_segments_all(path, aspath);
  }

  // unheard of: only AS4_PATH is present
  if (as4path) {
    // this a little bit bizarre since AS_PATH is mandatory, but we might as
    // well use what we've got
    return append_segments_all(path, as4path);
  }

  // possible: no AS_PATH and no AS4_PATH
  return 0;
}

static int handle_path_attrs(bgpstream_elem_t *el,
                             parsebgp_bgp_update_path_attr_t *attrs)
{
  parsebgp_bgp_update_as_path_t *aspath = NULL;
  parsebgp_bgp_update_as_path_t *as4path = NULL;

  // AS Path(s)
  if (attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_AS_PATH].type ==
      PARSEBGP_BGP_PATH_ATTR_TYPE_AS_PATH) {
    aspath = &attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_AS_PATH].data.as_path;
  }
  if (attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_AS4_PATH].type ==
      PARSEBGP_BGP_PATH_ATTR_TYPE_AS4_PATH) {
    as4path = &attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_AS4_PATH].data.as_path;
  }
  if (handle_as_paths(el->aspath, aspath, as4path) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not parse AS_PATH");
    return -1;
  }

  // Communities
  if (attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_COMMUNITIES].type ==
        PARSEBGP_BGP_PATH_ATTR_TYPE_COMMUNITIES &&
      bgpstream_community_set_populate(
        el->communities,
        attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_COMMUNITIES].data.communities.raw,
        attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_COMMUNITIES].len) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Could not parse COMMUNITIES");
    return -1;
  }
  return 0;
}

// extract the appropriate NEXT-HOP information from the given attributes
//
// from my reading of RFC4760, it is theoretically possible for a single UPDATE
// to carry reachability information for both v4 and another (v6) AFI, so we use
// the is_mp_pfx flag to direct us to either the NEXT_HOP attr, or the MP_REACH
// attr.
static int handle_next_hop(bgpstream_elem_t *el,
                           parsebgp_bgp_update_path_attr_t *attrs,
                           int is_mp_pfx)
{
  parsebgp_bgp_update_mp_reach_t *mp_reach;

  if (is_mp_pfx && attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI].type ==
                     PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI) {
    // extract next-hop from MP_REACH attribute
    mp_reach = &attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI].data.mp_reach;
    COPY_IP(&el->nexthop, mp_reach->afi, mp_reach->next_hop, return -1);
  } else {
    // extract next-hop from NEXT_HOP attribute
    if (attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_NEXT_HOP].type !=
        PARSEBGP_BGP_PATH_ATTR_TYPE_NEXT_HOP) {
      return 0;
    }
    COPY_IP(&el->nexthop, PARSEBGP_BGP_AFI_IPV4,
            attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_NEXT_HOP].data.next_hop,
            return -1);
  }

  return 0;
}

static int handle_table_dump(bgpstream_elem_generator_t *gen,
                             parsebgp_mrt_msg_t *mrt)
{
  bgpstream_elem_t *el;
  parsebgp_mrt_table_dump_t *td = &mrt->types.table_dump;

  if ((el = bgpstream_elem_generator_get_new_elem(gen)) == NULL) {
    return -1;
  }

  // legacy table dump format is basically an elem

  el->type = BGPSTREAM_ELEM_TYPE_RIB;
  el->timestamp = mrt->timestamp_sec;
  el->timestamp_usec = mrt->timestamp_usec; // not used

  COPY_IP(&el->peer_address, mrt->subtype, &td->peer_ip, return -1);

  el->peer_asnumber = td->peer_asn;

  COPY_IP(&el->prefix.address, mrt->subtype, &td->prefix, return -1);
  el->prefix.mask_len = td->prefix_len;

  if (handle_next_hop(el, td->path_attrs.attrs,
                      mrt->subtype == PARSEBGP_BGP_AFI_IPV6 ? 1 : 0) != 0) {
    return -1;
  }

  if (handle_path_attrs(el, td->path_attrs.attrs) != 0) {
    return -1;
  }

  bgpstream_elem_generator_commit_elem(gen, el);

  return 0;
}

static int handle_td2_peer_index(bgpstream_format_t *format,
                                 parsebgp_mrt_table_dump_v2_peer_index_t *pi)
{
  int i;
  khiter_t k;
  int khret;
  peer_index_entry_t *bs_pie;
  parsebgp_mrt_table_dump_v2_peer_entry_t *pie;

  // alloc the table hash
  if ((STATE->peer_table = kh_init(td2_peer)) == NULL) {
    return -1;
  }

  // add peers to the table
  for (i = 0; i < pi->peer_count; i++) {
    k = kh_put(td2_peer, STATE->peer_table, i, &khret);
    if (khret == -1) {
      return -1;
    }

    pie = &pi->peer_entries[i];
    bs_pie = &kh_val(STATE->peer_table, k);

    bs_pie->peer_asn = pie->asn;
    COPY_IP(&bs_pie->peer_ip, pie->ip_afi, pie->ip, return -1);
  }

  return 0;
}

static int handle_td2_rib_entry(bgpstream_format_t *format,
                                bgpstream_elem_t *elem_tmpl,
                                parsebgp_mrt_msg_t *mrt,
                                parsebgp_bgp_afi_t afi,
                                parsebgp_mrt_table_dump_v2_rib_entry_t *re)
{
  bgpstream_elem_t *el;
  peer_index_entry_t *bs_pie;
  khiter_t k;

  if ((el = bgpstream_elem_generator_get_new_elem(STATE->gen)) == NULL) {
    return -1;
  }
  el->type = BGPSTREAM_ELEM_TYPE_RIB;
  el->timestamp = elem_tmpl->timestamp;
  el->timestamp_usec = elem_tmpl->timestamp_usec;

  // look the peer up in the peer index table
  if ((k = kh_get(td2_peer, STATE->peer_table, re->peer_index)) ==
      kh_end(STATE->peer_table)) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Missing Peer Index Table entry for Peer ID %d",
                  re->peer_index);
    return -1;
  }
  bs_pie = &kh_val(STATE->peer_table, k);
  bgpstream_addr_copy((bgpstream_ip_addr_t *)&el->peer_address,
                      (bgpstream_ip_addr_t *)&bs_pie->peer_ip);

  el->peer_asnumber = bs_pie->peer_asn;


  bgpstream_pfx_copy((bgpstream_pfx_t *)&el->prefix,
                      (bgpstream_pfx_t *)&elem_tmpl->prefix);

  if (handle_next_hop(el, re->path_attrs.attrs,
                      afi == PARSEBGP_BGP_AFI_IPV6 ? 1 : 0) != 0) {
    return -1;
  }

  if (handle_path_attrs(el, re->path_attrs.attrs) != 0) {
    return -1;
  }

  bgpstream_elem_generator_commit_elem(STATE->gen, el);

  return 0;
}

static int
handle_td2_afi_safi_rib(bgpstream_format_t *format, parsebgp_mrt_msg_t *mrt,
                        parsebgp_bgp_afi_t afi,
                        parsebgp_mrt_table_dump_v2_afi_safi_rib_t *asr)
{
  int i;

  // we use this elem to pre-populate the timestamp, prefix and then populate
  // peer and path information within the loop
  bgpstream_elem_t *elem;

  if ((elem = bgpstream_elem_create()) == NULL) {
    return -1;
  }

  elem->timestamp = mrt->timestamp_sec;
  elem->timestamp_usec = mrt->timestamp_usec;
  COPY_IP(&elem->prefix.address, afi, asr->prefix, return 0);
  elem->prefix.mask_len = asr->prefix_len;
  // other elem fields are specific to the entry

  // if we haven't seen a peer index table yet, then just give up
  if (STATE->peer_table == NULL) {
    bgpstream_log(BGPSTREAM_LOG_WARN,
                  "Missing Peer Index Table, skipping RIB entry");
    return -1;
  }

  for (i = 0; i < asr->entry_count; i++) {
    if (handle_td2_rib_entry(format, elem, mrt, afi, &asr->entries[i]) != 0) {
      return -1;
    }
  }

  bgpstream_elem_destroy(elem);

  return 0;
}

static int handle_table_dump_v2(bgpstream_format_t *format,
                                parsebgp_mrt_msg_t *mrt)
{
  parsebgp_mrt_table_dump_v2_t *td2 = &mrt->types.table_dump_v2;

  switch (mrt->subtype) {
  case PARSEBGP_MRT_TABLE_DUMP_V2_PEER_INDEX_TABLE:
    if (STATE->peer_table != NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "Peer index table has already been processed");
      return 0;
    }
    return handle_td2_peer_index(format, &td2->peer_index);
    break;

  case PARSEBGP_MRT_TABLE_DUMP_V2_RIB_IPV4_UNICAST:
    return handle_td2_afi_safi_rib(format, mrt, PARSEBGP_BGP_AFI_IPV4,
                                   &td2->afi_safi_rib);
  case PARSEBGP_MRT_TABLE_DUMP_V2_RIB_IPV6_UNICAST:
    return handle_td2_afi_safi_rib(format, mrt, PARSEBGP_BGP_AFI_IPV6,
                                   &td2->afi_safi_rib);
    break;

  default:
    // do nothing
    break;
  }

  return 0;
}

/**
 * Given a list of prefixes (NLRIs) and a mostly-filled elem to use as a
 * template, populate the given elem generator with one elem per NLRI.
 *
 * TODO: consider using a custom elem generator that rather than replicating
 * elems that mostly have the same content, it just serves the same elem over
 * and over, but alters the prefix. This should use less memory, and be quite a
 * bit faster. This would mean that we'd have to make the elem extraction
 * process a generator itself, which could be quite complicated (e.g., how do we
 * remember where we are in walking through the MRT message?)
 */
static int handle_bgp4mp_prefixes(bgpstream_elem_generator_t *gen,
                                  bgpstream_elem_type_t elem_type,
                                  bgpstream_elem_t *elem_tmpl,
                                  parsebgp_bgp_prefix_t *prefixes,
                                  int prefixes_cnt,
                                  int is_announcement)
{
  bgpstream_elem_t *el;

  int i;
  for (i = 0; i < prefixes_cnt; i++) {
    if (prefixes->type != PARSEBGP_BGP_PREFIX_UNICAST_IPV4 &&
        prefixes->type != PARSEBGP_BGP_PREFIX_UNICAST_IPV6) {
      continue;
    }

    if ((el = bgpstream_elem_generator_get_new_elem(gen)) == NULL) {
      return -1;
    }
    el->type = elem_type;
    el->timestamp = elem_tmpl->timestamp;
    el->timestamp_usec = elem_tmpl->timestamp_usec;
    bgpstream_addr_copy((bgpstream_ip_addr_t *)&el->peer_address,
                        (bgpstream_ip_addr_t *)&elem_tmpl->peer_address);
    el->peer_asnumber = elem_tmpl->peer_asnumber;

    // Prefix
    COPY_IP(&el->prefix.address, prefixes[i].afi, prefixes[i].addr, continue);
    el->prefix.mask_len = prefixes[i].len;

    if (is_announcement) {
      // Next-Hop
      bgpstream_addr_copy((bgpstream_ip_addr_t *)&el->nexthop,
                          (bgpstream_ip_addr_t *)&elem_tmpl->nexthop);

      // AS Path
      if (bgpstream_as_path_copy(el->aspath, elem_tmpl->aspath) != 0) {
        return -1;
      }

      // Communities
      if (bgpstream_community_set_copy(el->communities,
                                       elem_tmpl->communities) != 0) {
        return -1;
      }
    }

    bgpstream_elem_generator_commit_elem(gen, el);
  }

  return 0;
}

static int handle_bgp4mp_bgp_msg(bgpstream_elem_generator_t *gen,
                                 bgpstream_elem_t *elem_tmpl,
                                 parsebgp_mrt_msg_t *mrt,
                                 parsebgp_mrt_bgp4mp_t *bgp4mp)
{
  parsebgp_bgp_update_t *update;
  parsebgp_bgp_update_path_attr_t *attr;

  if (bgp4mp->data.bgp_msg.type != PARSEBGP_BGP_TYPE_UPDATE) {
    return 0;
  }

  update = &bgp4mp->data.bgp_msg.types.update;

  // generate one elem per withdrawal
  // native (v4) withdrawals
  if (handle_bgp4mp_prefixes(gen, BGPSTREAM_ELEM_TYPE_WITHDRAWAL, elem_tmpl,
                             update->withdrawn_nlris.prefixes,
                             update->withdrawn_nlris.prefixes_cnt, 0) != 0) {
    return -1;
  }
  // MP_UNREACH (v6) withdrawals
  attr = &update->path_attrs.attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_UNREACH_NLRI];
  if (attr->type == PARSEBGP_BGP_PATH_ATTR_TYPE_MP_UNREACH_NLRI &&
      handle_bgp4mp_prefixes(gen, BGPSTREAM_ELEM_TYPE_WITHDRAWAL, elem_tmpl,
                             attr->data.mp_unreach.withdrawn_nlris,
                             attr->data.mp_unreach.withdrawn_nlris_cnt, 0) != 0) {
    return -1;
  }

  // Announcements
  attr = &update->path_attrs.attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI];

  // if there are no announcements, bail out now
  if (update->announced_nlris.prefixes_cnt == 0 &&
      (attr->type != PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI ||
       attr->data.mp_reach.nlris_cnt == 0)) {
    return 0;
  }

  if (handle_path_attrs(elem_tmpl, update->path_attrs.attrs) != 0) {
    return -1;
  }

  // generate one elem per announcement
  // native (v4) announcements
  if (handle_next_hop(elem_tmpl, update->path_attrs.attrs, 0) != 0) {
    return -1;
  }
  if (handle_bgp4mp_prefixes(gen, BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT, elem_tmpl,
                             update->announced_nlris.prefixes,
                             update->announced_nlris.prefixes_cnt, 1) != 0) {
    return -1;
  }
  // MP_REACH (v6) announcements
  if (attr->type == PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI) {
    if (handle_next_hop(elem_tmpl, update->path_attrs.attrs, 1) != 0) {
      return -1;
    }
    if (handle_bgp4mp_prefixes(gen, BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT, elem_tmpl,
                               attr->data.mp_reach.nlris,
                               attr->data.mp_reach.nlris_cnt, 1) != 0) {
      return -1;
    }
  }
  return 0;
}

static int handle_bgp4mp_state_change(bgpstream_elem_generator_t *gen,
                                      bgpstream_elem_t *elem_tmpl,
                                      parsebgp_mrt_msg_t *mrt,
                                      parsebgp_mrt_bgp4mp_t *bgp4mp)
{
  // pointer to the elem in the generator
  bgpstream_elem_t *el;
  if ((el = bgpstream_elem_generator_get_new_elem(gen)) == NULL) {
    return -1;
  }
  el->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
  el->timestamp = elem_tmpl->timestamp;
  el->timestamp_usec = elem_tmpl->timestamp_usec;
  bgpstream_addr_copy((bgpstream_ip_addr_t *)&el->peer_address,
                      (bgpstream_ip_addr_t *)&elem_tmpl->peer_address);
  el->peer_asnumber = elem_tmpl->peer_asnumber;

  el->old_state = bgp4mp->data.state_change.old_state;
  el->new_state = bgp4mp->data.state_change.new_state;

  bgpstream_elem_generator_commit_elem(gen, el);

  return 0;
}

static int handle_bgp4mp(bgpstream_elem_generator_t *gen,
                         parsebgp_mrt_msg_t *mrt)
{
  int rc = 0;
  parsebgp_mrt_bgp4mp_t *bgp4mp = &mrt->types.bgp4mp;
  // we use this elem to pre-populate some common fields among the elems we'll
  // generate
  bgpstream_elem_t *elem;
  if ((elem = bgpstream_elem_create()) == NULL) {
    return -1;
  }

  elem->timestamp = mrt->timestamp_sec;
  elem->timestamp_usec = mrt->timestamp_usec;
  COPY_IP(&elem->peer_address, bgp4mp->afi, bgp4mp->peer_ip, return 0);
  elem->peer_asnumber = bgp4mp->peer_asn;
  // other elem fields are specific to the message

  switch (mrt->subtype) {
  case PARSEBGP_MRT_BGP4MP_STATE_CHANGE:
  case PARSEBGP_MRT_BGP4MP_STATE_CHANGE_AS4:
    rc = handle_bgp4mp_state_change(gen, elem, mrt, bgp4mp);
    break;

  case PARSEBGP_MRT_BGP4MP_MESSAGE:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_AS4:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_LOCAL:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_AS4_LOCAL:
    rc = handle_bgp4mp_bgp_msg(gen, elem, mrt, bgp4mp);
    break;

  default:
    bgpstream_log(BGPSTREAM_LOG_WARN,
                  "Skipping unknown BGP4MP record subtype %d", mrt->subtype);
    break;
  }

  bgpstream_elem_destroy(elem);

  return rc;
}

static int populate_elem_generator(bgpstream_format_t *format,
                                   parsebgp_msg_t *msg)
{
  parsebgp_mrt_msg_t *mrt;

  /* mark the generator as having no elems */
  bgpstream_elem_generator_empty(STATE->gen);

  if (msg == NULL) {
    return 0;
  }

  mrt = &msg->types.mrt;

  switch (mrt->type) {
  case PARSEBGP_MRT_TYPE_TABLE_DUMP:
    return handle_table_dump(STATE->gen, mrt);
    break;

  case PARSEBGP_MRT_TYPE_TABLE_DUMP_V2:
    return handle_table_dump_v2(format, mrt);
    break;

  case PARSEBGP_MRT_TYPE_BGP4MP:
  case PARSEBGP_MRT_TYPE_BGP4MP_ET:
    return handle_bgp4mp(STATE->gen, mrt);
    break;

  default:
    // a type we don't care about, so leave the elem generator empty
    bgpstream_log(BGPSTREAM_LOG_WARN, "Skipping unknown MRT record type %d",
                  mrt->type);
    break;
  }

  return 0;
}

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

static ssize_t refill_buffer(bgpstream_format_t *format, uint8_t *buf,
                             size_t buflen, size_t remain)
{
  size_t len = 0;
  int64_t new_read = 0;

  if (remain > 0) {
    // need to move remaining data to start of buffer
    memmove(buf, buf + buflen - remain, remain);
    len += remain;
  }

  // try and do a read
  if ((new_read = bgpstream_transport_read(format->transport, buf + len,
                                           buflen - len)) < 0) {
    // read failed
    return new_read;
  }

  // new_read could be 0, indicating EOF, so need to check returned len is
  // larger than passed in remain
  return len + new_read;
}

static bgpstream_format_status_t handle_eof(bgpstream_format_t *format,
                                            bgpstream_record_t *record,
                                            uint64_t skipped_cnt)
{
  assert(FDATA == NULL);

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

/* ==================== PUBLIC API BELOW HERE ==================== */

int bs_format_mrt_create(bgpstream_format_t *format,
                         bgpstream_resource_t *res)
{
  BS_FORMAT_SET_METHODS(mrt, format);
  parsebgp_opts_t *opts = NULL;

  if ((format->state = malloc_zero(sizeof(state_t))) == NULL) {
    return -1;
  }

  if ((STATE->gen = bgpstream_elem_generator_create()) == NULL) {
    return -1;
  }

  opts = &STATE->parser_opts;
  parsebgp_opts_init(opts);

  // select only the Path Attributes that we care about
  opts->bgp.path_attr_filter_enabled = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_ORIGIN] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_AS_PATH] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_NEXT_HOP] = 1;
  //opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_MED] = 1;
  //opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_LOCAL_PREF] = 1;
  //opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_ATOMIC_AGGREGATE] = 1;
  //opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_AGGREGATOR] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_COMMUNITIES] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_UNREACH_NLRI] = 1;
  opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_AS4_PATH] = 1;
  //opts->bgp.path_attr_filter[PARSEBGP_BGP_PATH_ATTR_TYPE_AS4_AGGREGATOR] = 1;

  // and ask for shallow parsing of communities
  opts->bgp.path_attr_raw_enabled = 1;
  opts->bgp.path_attr_raw[PARSEBGP_BGP_PATH_ATTR_TYPE_COMMUNITIES] = 1;

  // DEBUG: (switch to ignore in production)
  opts->ignore_not_implemented = 0;

  return 0;
}

bgpstream_format_status_t
bs_format_mrt_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{
  assert(record->__format_data->format == format);
  assert(FDATA == NULL);

  int refill = 0;
  ssize_t fill_len = 0;
  size_t dec_len = 0;
  uint64_t skipped_cnt = 0;
  parsebgp_error_t err;

 refill:
  if (STATE->remain == 0 || refill != 0) {
    // try to refill the buffer
    if ((fill_len =
         refill_buffer(format, STATE->buffer, BUFLEN, STATE->remain)) == 0) {
      // EOF
      return handle_eof(format, record, skipped_cnt);
    }
    if (fill_len < 0) {
      // read error
      // TODO: create a specific read error failure so that perhaps BGPStream
      // can retry
      return -1;
    }
    if (fill_len == STATE->remain) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "DEBUG: Corrupted dump or failed read\n");
      return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
    }
    // here we have something new to read
    STATE->remain = fill_len;
    STATE->ptr = STATE->buffer;
  }

  while (STATE->remain > 0) {
    if (FDATA == NULL &&
        (record->__format_data->data = parsebgp_create_msg()) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "ERROR: Failed to create message structure\n");
      return -1;
    }

    dec_len = STATE->remain;
    if ((err = parsebgp_decode(STATE->parser_opts, PARSEBGP_MSG_TYPE_MRT,
                               FDATA, STATE->ptr, &dec_len)) != PARSEBGP_OK) {
      if (err == PARSEBGP_PARTIAL_MSG) {
        // refill the buffer and try again
        refill = 1;
        parsebgp_destroy_msg(FDATA);
        record->__format_data->data = NULL;
        goto refill;
      }
      // else: its a fatal error
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "ERROR: Failed to parse message (%d:%s)\n", err,
                    parsebgp_strerror(err));
      parsebgp_destroy_msg(FDATA);
      record->__format_data->data = NULL;
      return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
    }
    // else: successful read
    STATE->ptr += dec_len;
    STATE->remain -= dec_len;

    // got a message!
    assert(FDATA != NULL);
    assert(FDATA->type == PARSEBGP_MSG_TYPE_MRT);

    // if this is a peer index table message, we parse it now and move on (we
    // could also add a "filtered" flag to the peer_index_entry_t struct so that
    // when elem parsing happens it can quickly filter out unwanted peers
    // without having to check ASN or IP

    if (FDATA->types.mrt.type == PARSEBGP_MRT_TYPE_TABLE_DUMP_V2 &&
        FDATA->types.mrt.subtype ==
          PARSEBGP_MRT_TABLE_DUMP_V2_PEER_INDEX_TABLE) {
      if (handle_td2_peer_index(
            format, &FDATA->types.mrt.types.table_dump_v2.peer_index) != 0) {
        bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to process Peer Index Table");
        parsebgp_destroy_msg(FDATA);
        record->__format_data->data = NULL;
        return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
      }
      // move on to the next record
      parsebgp_destroy_msg(FDATA);
      record->__format_data->data = NULL;
      continue;
    }

    STATE->successful_read_cnt++;

    // check the filters
    // TODO: if this is a BGP4MP or TD1 message (UPDATE), then we can do some
    // work to prep the path attributes (and then filter on them).

    if (is_wanted_time(FDATA->types.mrt.timestamp_sec, format->filter_mgr) !=
        0) {
      // we want this entry
      STATE->valid_read_cnt++;
      break;
    } else {
      // we dont want this entry, destroy it
      parsebgp_destroy_msg(FDATA);
      record->__format_data->data = NULL;
      if (skipped_cnt == UINT64_MAX) {
        // probably this will never happen, but lets just be careful we don't
        // wrap and think we haven't skipped anything
        skipped_cnt = 0;
      }
      skipped_cnt++;
      // fall through and repeat loop
    }
  }

  if (STATE->remain == 0 && FDATA == NULL) {
    // EOF
    return handle_eof(format, record, skipped_cnt);
  }

  // valid message, and it passes our filters
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
  record->attributes.record_time = FDATA->types.mrt.timestamp_sec;

  // we successfully read a record, return it
  return BGPSTREAM_FORMAT_OK;
}

int bs_format_mrt_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  if (bgpstream_elem_generator_is_populated(STATE->gen) == 0 &&
      populate_elem_generator(format, FDATA) != 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Elem generator population failed");
    return -1;
  }
  *elem = bgpstream_elem_generator_get_next_elem(STATE->gen);
  if (*elem == NULL) {
    return 0;
  }
  return 1;
}

void bs_format_mrt_destroy_data(bgpstream_format_t *format, void *data)
{
  bgpstream_elem_generator_clear(STATE->gen);
  parsebgp_destroy_msg((parsebgp_msg_t*)data);
}

void bs_format_mrt_destroy(bgpstream_format_t *format)
{
  bgpstream_elem_generator_destroy(STATE->gen);
  STATE->gen = NULL;

  if (STATE->peer_table != NULL) {
    kh_destroy(td2_peer, STATE->peer_table);
    STATE->peer_table = NULL;
  }

  free(format->state);
  format->state = NULL;
}
