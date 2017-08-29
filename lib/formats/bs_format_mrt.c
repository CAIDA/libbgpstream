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
#include "bgpstream_elem_generator.h"
#include "bgpstream_log.h"
#include "bgpstream_parsebgp_common.h"
#include "utils.h"
#include <assert.h>

#define STATE ((state_t*)(format->state))

typedef struct peer_index_entry {

  /** Peer ASN */
  uint32_t peer_asn;

  /** Peer IP */
  bgpstream_addr_storage_t peer_ip;

} peer_index_entry_t;

KHASH_INIT(td2_peer, int, peer_index_entry_t, 1, kh_int_hash_func,
           kh_int_hash_equal);

typedef struct state {

  // parsebgp decode wrapper state
  bgpstream_parsebgp_decode_state_t decoder;

  // elem generator instance
  bgpstream_elem_generator_t *gen;

  // state to store the "peer index table" when reading TABLE_DUMP_V2 records
  khash_t(td2_peer) *peer_table;

} state_t;

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

  if (bgpstream_parsebgp_process_next_hop(el, td->path_attrs.attrs,
                      mrt->subtype == PARSEBGP_BGP_AFI_IPV6 ? 1 : 0) != 0) {
    return -1;
  }

  if (bgpstream_parsebgp_process_path_attrs(el, td->path_attrs.attrs) != 0) {
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

  if (bgpstream_parsebgp_process_next_hop(el, re->path_attrs.attrs,
                      afi == PARSEBGP_BGP_AFI_IPV6 ? 1 : 0) != 0) {
    return -1;
  }

  if (bgpstream_parsebgp_process_path_attrs(el, re->path_attrs.attrs) != 0) {
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
    // Peer Index tables are processed during get_next_record
    assert(0);
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

  if (bgpstream_parsebgp_process_path_attrs(elem_tmpl,
                                            update->path_attrs.attrs) != 0) {
    return -1;
  }

  // generate one elem per announcement
  // native (v4) announcements
  if (bgpstream_parsebgp_process_next_hop(elem_tmpl, update->path_attrs.attrs,
                                          0) != 0) {
    return -1;
  }
  if (handle_bgp4mp_prefixes(gen, BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT, elem_tmpl,
                             update->announced_nlris.prefixes,
                             update->announced_nlris.prefixes_cnt, 1) != 0) {
    return -1;
  }
  // MP_REACH (v6) announcements
  if (attr->type == PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI) {
    if (bgpstream_parsebgp_process_next_hop(elem_tmpl, update->path_attrs.attrs,
                                            1) != 0) {
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

static int populate_filter_cb(bgpstream_format_t *format,
                              parsebgp_msg_t *msg)
{
  assert(msg->type == PARSEBGP_MSG_TYPE_MRT);

  // if this is a peer index table message, we parse it now and move on (we
  // could also add a "filtered" flag to the peer_index_entry_t struct so that
  // when elem parsing happens it can quickly filter out unwanted peers
  // without having to check ASN or IP
  if (msg->types.mrt.type == PARSEBGP_MRT_TYPE_TABLE_DUMP_V2 &&
      msg->types.mrt.subtype ==
      PARSEBGP_MRT_TABLE_DUMP_V2_PEER_INDEX_TABLE) {
    if (handle_td2_peer_index(
          format, &msg->types.mrt.types.table_dump_v2.peer_index) != 0) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Failed to process Peer Index Table");
      return -1;
    }
    // indicate that we want this message skipped
    return 0;
  }

  // check the filters
  // TODO: if this is a BGP4MP or TD1 message (UPDATE), then we can do some
  // work to prep the path attributes (and then filter on them).

  if (is_wanted_time(msg->types.mrt.timestamp_sec, format->filter_mgr) != 0) {
    // we want this entry
    return 1;
  } else {
    return 0;
  }
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

  opts = &STATE->decoder.parser_opts;
  parsebgp_opts_init(opts);
  bgpstream_parsebgp_opts_init(opts);

  return 0;
}

bgpstream_format_status_t
bs_format_mrt_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{
  return bgpstream_parsebgp_populate_record(&STATE->decoder, format, record,
                                            populate_filter_cb);
}

int bs_format_mrt_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  if (bgpstream_elem_generator_is_populated(STATE->gen) == 0 &&
      populate_elem_generator(format, BGPSTREAM_PARSEBGP_FDATA) != 0) {
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
