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

  /* TODO: it is a little unsafe to have the generator directly in the format if
     the user tries to extract elems from two records simultaneously. consider
     moving all the generator state into a wrapper for the message that is
     stored in the format data pointer of the record */

  // reusable elem instance
  bgpstream_elem_t *elem;

  // have we extracted all the possible elems out of the current message?
  int end_of_elems;

  // state to store the "peer index table" when reading TABLE_DUMP_V2 records
  khash_t(td2_peer) *peer_table;

  // index of the NEXT rib entry to read from a TDv2 message
  int next_re;

  // state for UPDATE elem extraction
  bgpstream_parsebgp_upd_state_t upd_state;

} state_t;

static int handle_table_dump(bgpstream_format_t *format,
                             parsebgp_mrt_msg_t *mrt)
{
  bgpstream_elem_t *el = STATE->elem;
  parsebgp_mrt_table_dump_t *td = &mrt->types.table_dump;

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

  // only one elem per message
  STATE->end_of_elems = 1;

  return 1;
}

static int handle_td2_rib_entry(bgpstream_format_t *format,
                                parsebgp_mrt_msg_t *mrt,
                                parsebgp_bgp_afi_t afi,
                                parsebgp_mrt_table_dump_v2_rib_entry_t *re)
{
  peer_index_entry_t *bs_pie;
  khiter_t k;

  // look the peer up in the peer index table
  if ((k = kh_get(td2_peer, STATE->peer_table, re->peer_index)) ==
      kh_end(STATE->peer_table)) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "Missing Peer Index Table entry for Peer ID %d",
                  re->peer_index);
    return -1;
  }
  bs_pie = &kh_val(STATE->peer_table, k);
  bgpstream_addr_copy((bgpstream_ip_addr_t *)&STATE->elem->peer_address,
                      (bgpstream_ip_addr_t *)&bs_pie->peer_ip);

  STATE->elem->peer_asnumber = bs_pie->peer_asn;

  if (bgpstream_parsebgp_process_next_hop(
        STATE->elem, re->path_attrs.attrs,
        afi == PARSEBGP_BGP_AFI_IPV6 ? 1 : 0) != 0) {
    return -1;
  }

  if (bgpstream_parsebgp_process_path_attrs(STATE->elem,
                                            re->path_attrs.attrs) != 0) {
    return -1;
  }

  return 0;
}

static int
handle_td2_afi_safi_rib(bgpstream_format_t *format, parsebgp_mrt_msg_t *mrt,
                        parsebgp_bgp_afi_t afi,
                        parsebgp_mrt_table_dump_v2_afi_safi_rib_t *asr)
{
  // if this is the first time we've been called, prep the elem
  if (STATE->next_re == 0) {
    STATE->elem->type = BGPSTREAM_ELEM_TYPE_RIB;
    STATE->elem->timestamp = mrt->timestamp_sec;
    STATE->elem->timestamp_usec = mrt->timestamp_usec;
    COPY_IP(&STATE->elem->prefix.address, afi, asr->prefix, return 0);
    STATE->elem->prefix.mask_len = asr->prefix_len;
    // other elem fields are specific to the entry

    // if we haven't seen a peer index table yet, then just give up
    if (STATE->peer_table == NULL) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "Missing Peer Index Table, skipping RIB entry");
      return -1;
    }
  }

  // since this is a generator, we just process one rib entry each time
  if (handle_td2_rib_entry(format, mrt, afi, &asr->entries[STATE->next_re]) !=
      0) {
    return -1;
  }

  // move on to the next rib entry
  STATE->next_re++;
  if (STATE->next_re == asr->entry_count) {
    STATE->end_of_elems = 1;
  }

  return 1;
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

static int handle_bgp4mp_state_change(bgpstream_format_t *format,
                                      parsebgp_mrt_bgp4mp_t *bgp4mp)
{
  STATE->elem->type = BGPSTREAM_ELEM_TYPE_PEERSTATE;
  STATE->elem->old_state = bgp4mp->data.state_change.old_state;
  STATE->elem->new_state = bgp4mp->data.state_change.new_state;
  STATE->end_of_elems = 1;
  return 1;
}

static int handle_bgp4mp(bgpstream_format_t *format,
                         parsebgp_mrt_msg_t *mrt)
{
  int rc = 0;
  parsebgp_mrt_bgp4mp_t *bgp4mp = &mrt->types.bgp4mp;

  STATE->elem->timestamp = mrt->timestamp_sec;
  STATE->elem->timestamp_usec = mrt->timestamp_usec;
  COPY_IP(&STATE->elem->peer_address, bgp4mp->afi, bgp4mp->peer_ip, return 0);
  STATE->elem->peer_asnumber = bgp4mp->peer_asn;
  // other elem fields are specific to the message

  switch (mrt->subtype) {
  case PARSEBGP_MRT_BGP4MP_STATE_CHANGE:
  case PARSEBGP_MRT_BGP4MP_STATE_CHANGE_AS4:
    rc = handle_bgp4mp_state_change(format, bgp4mp);
    break;

  case PARSEBGP_MRT_BGP4MP_MESSAGE:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_AS4:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_LOCAL:
  case PARSEBGP_MRT_BGP4MP_MESSAGE_AS4_LOCAL:
    rc = bgpstream_parsebgp_process_update(&STATE->upd_state, STATE->elem,
                                           &bgp4mp->data.bgp_msg);
    if (rc == 0) {
      STATE->end_of_elems = 1;
    }
    break;

  default:
    bgpstream_log(BGPSTREAM_LOG_WARN,
                  "Skipping unknown BGP4MP record subtype %d", mrt->subtype);
    break;
  }

  return rc;
}

static void reset_generator(bgpstream_format_t *format)
{
  bgpstream_elem_clear(STATE->elem);
  STATE->end_of_elems = 0;
  STATE->next_re = 0;
  bgpstream_parsebgp_upd_state_reset(&STATE->upd_state);
}

/* -------------------- RECORD FILTERING -------------------- */

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

static int populate_filter_cb(bgpstream_format_t *format,
                              parsebgp_msg_t *msg,
                              uint32_t *ts_sec)
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
    *ts_sec = msg->types.mrt.timestamp_sec;
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

  if ((STATE->elem = bgpstream_elem_create()) == NULL) {
    return -1;
  }

  STATE->decoder.msg_type = PARSEBGP_MSG_TYPE_MRT;

  opts = &STATE->decoder.parser_opts;
  parsebgp_opts_init(opts);
  bgpstream_parsebgp_opts_init(opts);

  return 0;
}

bgpstream_format_status_t
bs_format_mrt_populate_record(bgpstream_format_t *format,
                              bgpstream_record_t *record)
{
  reset_generator(format);
  return bgpstream_parsebgp_populate_record(&STATE->decoder, format, record,
                                            populate_filter_cb);
}

int bs_format_mrt_get_next_elem(bgpstream_format_t *format,
                                bgpstream_record_t *record,
                                bgpstream_elem_t **elem)
{
  parsebgp_mrt_msg_t *mrt;
  int rc;

  *elem = NULL;

  if (BGPSTREAM_PARSEBGP_FDATA == NULL || STATE->end_of_elems != 0) {
    // end-of-elems
    return 0;
  }

  mrt = &BGPSTREAM_PARSEBGP_FDATA->types.mrt;
  switch (mrt->type) {
  case PARSEBGP_MRT_TYPE_TABLE_DUMP:
    rc = handle_table_dump(format, mrt);
    break;

  case PARSEBGP_MRT_TYPE_TABLE_DUMP_V2:
    rc = handle_table_dump_v2(format, mrt);
    break;

  case PARSEBGP_MRT_TYPE_BGP4MP:
  case PARSEBGP_MRT_TYPE_BGP4MP_ET:
    rc = handle_bgp4mp(format, mrt);
    break;

  default:
    // a type we don't care about, so return end-of-elems
    bgpstream_log(BGPSTREAM_LOG_WARN, "Skipping unknown MRT record type %d",
                  mrt->type);
    rc = 0;
    break;
  }

  if (rc <= 0) {
    return rc;
  }

  // return a borrowed pointer to the elem we populated
  *elem = STATE->elem;
  return 1;
}

void bs_format_mrt_destroy_data(bgpstream_format_t *format, void *data)
{
  reset_generator(format);
  parsebgp_destroy_msg((parsebgp_msg_t*)data);
}

void bs_format_mrt_destroy(bgpstream_format_t *format)
{
  bgpstream_elem_destroy(STATE->elem);
  STATE->elem = NULL;

  if (STATE->peer_table != NULL) {
    kh_destroy(td2_peer, STATE->peer_table);
    STATE->peer_table = NULL;
  }

  free(format->state);
  format->state = NULL;
}
