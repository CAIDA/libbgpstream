#include "bgpstream_parsebgp_common.h"
#include "bgpstream_format_interface.h"
#include "bgpstream_record_int.h"
#include "bgpstream_utils_as_path_int.h"
#include "bgpstream_utils_community_int.h"
#include "bgpstream_log.h"
#include <assert.h>
#include <string.h>

static bgpstream_as_path_seg_type_t as_path_types[] = {
  BGPSTREAM_AS_PATH_SEG_INVALID,    // INVALID
  BGPSTREAM_AS_PATH_SEG_SET,        // PARSEBGP_BGP_UPDATE_AS_PATH_SEG_AS_SET
  BGPSTREAM_AS_PATH_SEG_ASN,        // PARSEBGP_BGP_UPDATE_AS_PATH_SEG_AS_SEQ
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

static ssize_t refill_buffer(bgpstream_parsebgp_decode_state_t *state,
                             bgpstream_transport_t *transport)
{
  size_t len = 0;
  int64_t new_read = 0;

  if (state->remain > 0) {
    // need to move remaining data to start of buffer
    memmove(state->buffer,
            state->buffer + BGPSTREAM_PARSEBGP_BUFLEN - state->remain,
            state->remain);
    len += state->remain;
  }

  // try and do a read
  if ((new_read = bgpstream_transport_read(transport, state->buffer + len,
                                           BGPSTREAM_PARSEBGP_BUFLEN - len)) <
      0) {
    // read failed
    return new_read;
  }

  // new_read could be 0, indicating EOF, so need to check returned len is
  // larger than passed in remain
  return len + new_read;
}

static bgpstream_format_status_t
handle_eof(bgpstream_parsebgp_decode_state_t *state, bgpstream_record_t *record,
           uint64_t skipped_cnt)
{
  assert(BGPSTREAM_PARSEBGP_FDATA == NULL);

  // just to be kind, set the record time to the dump time
  record->attributes.record_time = record->attributes.dump_time;

  if (skipped_cnt == 0) {
    // signal that the previous record really was the last in the dump
    record->dump_pos = BGPSTREAM_DUMP_END;
  }
  // was this the first thing we tried to read?
  if (state->successful_read_cnt == 0) {
    // then it is an empty file
    record->status = BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE;
    record->dump_pos = BGPSTREAM_DUMP_END;
    return BGPSTREAM_FORMAT_EMPTY_DUMP;
  }

  // so we managed to read some things, but did we get anything useful from
  // this file?
  if (state->valid_read_cnt == 0) {
    // dump contained data, but we filtered all of them out
    record->status = BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE;
    record->dump_pos = BGPSTREAM_DUMP_END;
    return BGPSTREAM_FORMAT_FILTERED_DUMP;
  }

  // otherwise, signal end of dump (record has not been filled)
  return BGPSTREAM_FORMAT_END_OF_DUMP;
}

/* -------------------- PUBLIC API FUNCTIONS -------------------- */

void bgpstream_parsebgp_upd_state_reset(
  bgpstream_parsebgp_upd_state_t *upd_state)
{
  memset(upd_state, 0, sizeof(*upd_state));
}

static int handle_prefix(bgpstream_elem_t *elem,
                         bgpstream_elem_type_t elem_type,
                         parsebgp_bgp_prefix_t *prefix)
{
  if (prefix->type != PARSEBGP_BGP_PREFIX_UNICAST_IPV4 &&
      prefix->type != PARSEBGP_BGP_PREFIX_UNICAST_IPV6) {
    return 0;
  }

  elem->type = elem_type;

  // Prefix
  COPY_IP(&elem->prefix.address, prefix->afi, prefix->addr, return 0);
  elem->prefix.mask_len = prefix->len;

  return 1;
}

#define WITHDRAWAL_GENERATOR(nlri_type, prefixes)                              \
  do {                                                                         \
    rc = 0;                                                                    \
    while (upd_state->withdrawal_##nlri_type##_cnt > 0 && rc == 0) {           \
      if ((rc = handle_prefix(                                                 \
             elem, BGPSTREAM_ELEM_TYPE_WITHDRAWAL,                             \
             &prefixes[upd_state->withdrawal_##nlri_type##_idx])) < 0) {       \
        bgpstream_log(BGPSTREAM_LOG_ERR, "Could not extract withdrawal elem"); \
        return -1;                                                             \
      }                                                                        \
      upd_state->withdrawal_##nlri_type##_cnt--;                               \
      upd_state->withdrawal_##nlri_type##_idx++;                               \
    }                                                                          \
    if (rc != 0) {                                                             \
      return rc;                                                               \
    }                                                                          \
  } while (0)

#define ANNOUNCEMENT_GENERATOR(nlri_type, prefixes, is_mp_reach)               \
  do {                                                                         \
    rc = 0;                                                                    \
    while (upd_state->announce_##nlri_type##_cnt > 0 && rc == 0) {             \
      if (upd_state->next_hop_##nlri_type##_done == 0) {                       \
        if (bgpstream_parsebgp_process_next_hop(                               \
              elem, update->path_attrs.attrs, is_mp_reach) != 0) {             \
          bgpstream_log(BGPSTREAM_LOG_ERR, "Could not extract next-hop");      \
          return -1;                                                           \
        }                                                                      \
        upd_state->next_hop_##nlri_type##_done = 1;                            \
      }                                                                        \
                                                                               \
      if ((rc = handle_prefix(                                                 \
             elem, BGPSTREAM_ELEM_TYPE_ANNOUNCEMENT,                           \
             &prefixes[upd_state->announce_##nlri_type##_idx])) < 0) {         \
        bgpstream_log(BGPSTREAM_LOG_ERR,                                       \
                      "Could not extract announcement elem");                  \
        return -1;                                                             \
      }                                                                        \
      upd_state->announce_##nlri_type##_cnt--;                                 \
      upd_state->announce_##nlri_type##_idx++;                                 \
    }                                                                          \
    if (rc != 0) {                                                             \
      return rc;                                                               \
    }                                                                          \
  } while (0)

int bgpstream_parsebgp_process_update(bgpstream_parsebgp_upd_state_t *upd_state,
                                      bgpstream_elem_t *elem,
                                      parsebgp_bgp_msg_t *bgp)
{
  parsebgp_bgp_update_t *update = &bgp->types.update;
  int rc = 0;

  if (upd_state->ready == 0) {
    // need to check some things, and get ready to yield elems

    // first, a sanity check
    if (bgp->type != PARSEBGP_BGP_TYPE_UPDATE) {
      return 0;
    }

    // how many native withdrawals will we process?
    upd_state->withdrawal_v4_cnt = update->withdrawn_nlris.prefixes_cnt;

    // how many MP_UNREACH (IPv6) withdrawals still to yield
    if (update->path_attrs.attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_UNREACH_NLRI]
          .type == PARSEBGP_BGP_PATH_ATTR_TYPE_MP_UNREACH_NLRI) {
      upd_state->withdrawal_v6_cnt =
        update->path_attrs.attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_UNREACH_NLRI]
          .data.mp_unreach.withdrawn_nlris_cnt;
    }

    // how many native (IPv4) announcements still to yield
    upd_state->announce_v4_cnt = update->announced_nlris.prefixes_cnt;

    // how many MP_REACH (IPv6) announcements still to yield
    if (update->path_attrs.attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI]
          .type == PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI) {
      upd_state->announce_v6_cnt =
        update->path_attrs.attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI]
          .data.mp_reach.nlris_cnt;
    }

    // all other flags left set to zero

    upd_state->ready = 1;
  }

  // are we at end-of-elems?
  if (upd_state->withdrawal_v4_cnt == 0 &&
      upd_state->withdrawal_v6_cnt == 0 &&
      upd_state->announce_v4_cnt == 0 &&
      upd_state->announce_v6_cnt == 0) {
    return 0;
  }

  // IPv4 Withdrawals
  WITHDRAWAL_GENERATOR(v4, update->withdrawn_nlris.prefixes);

  // IPv6 Withdrawals
  WITHDRAWAL_GENERATOR(
    v6, update->path_attrs.attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_UNREACH_NLRI]
          .data.mp_unreach.withdrawn_nlris);

  // at this point we need the path attributes processed
  if (upd_state->path_attr_done == 0) {
    if (bgpstream_parsebgp_process_path_attrs(elem,
                                              update->path_attrs.attrs) != 0) {
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not extract path attributes");
      return -1;
    }
    upd_state->path_attr_done = 1;
  }

  // IPv4 Announcements (will also trigger next-hop extraction)
  ANNOUNCEMENT_GENERATOR(v4, update->announced_nlris.prefixes, 0);

  // IPv6 Announcements (will also trigger next-hop extraction)
  ANNOUNCEMENT_GENERATOR(
    v6,
    update->path_attrs.attrs[PARSEBGP_BGP_PATH_ATTR_TYPE_MP_REACH_NLRI]
      .data.mp_reach.nlris,
    1);

  return 0;
}


int bgpstream_parsebgp_process_path_attrs(
  bgpstream_elem_t *el, parsebgp_bgp_update_path_attr_t *attrs)
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

int bgpstream_parsebgp_process_next_hop(bgpstream_elem_t *el,
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

bgpstream_format_status_t bgpstream_parsebgp_populate_record(
  bgpstream_parsebgp_decode_state_t *state, bgpstream_format_t *format,
  bgpstream_record_t *record, bgpstream_parsebgp_check_filter_cb_t *cb)
{
  assert(record->__format_data->format == format);
  assert(BGPSTREAM_PARSEBGP_FDATA == NULL);

  int refill = 0;
  ssize_t fill_len = 0;
  size_t dec_len = 0;
  uint64_t skipped_cnt = 0;
  parsebgp_error_t err;
  int filter;
  uint32_t ts_sec = 0;

 refill:
  // if there's nothing left in the buffer, it could just be because we happened
  // to empty it, so let's try and get some more data from the transport just in
  // case.
  // on the other hand, if there are some bytes left in the buffer, but we've
  // got to the end, and there's a partial message left, the "refill" flag will
  // be set which causes us to do a forced refill (the remaining bytes will be
  // shifted to the beginning of the buffer, and the rest filled).
  if (state->remain == 0 || refill != 0) {
    // try to refill the buffer
    if ((fill_len = refill_buffer(state, format->transport)) == 0) {
      // EOF
      return handle_eof(state, record, skipped_cnt);
    }
    if (fill_len < 0) {
      // read error
      // TODO: create a specific read error failure so that perhaps BGPStream
      // can retry
      bgpstream_log(BGPSTREAM_LOG_ERR, "Could not refill buffer\n");
      return -1;
    }
    if (fill_len == state->remain) {
      bgpstream_log(BGPSTREAM_LOG_WARN,
                    "DEBUG: Corrupted dump or failed read\n");
      return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
    }
    // here we have something new to read
    state->remain = fill_len;
    state->ptr = state->buffer;

    // reset the "force refill" flag
    refill = 0;
  }

  // if we still have nothing to read, then we have nothing to read!
  if (state->remain == 0) {
    // EOF
    return handle_eof(state, record, skipped_cnt);
  }

  if (BGPSTREAM_PARSEBGP_FDATA == NULL &&
      (record->__format_data->data = parsebgp_create_msg()) == NULL) {
    bgpstream_log(BGPSTREAM_LOG_ERR,
                  "ERROR: Failed to create message structure\n");
    return -1;
  }

  dec_len = state->remain;
  if ((err = parsebgp_decode(state->parser_opts, state->msg_type,
                             BGPSTREAM_PARSEBGP_FDATA, state->ptr, &dec_len)) !=
      PARSEBGP_OK) {
    if (err == PARSEBGP_PARTIAL_MSG) {
      // refill the buffer and try again
      parsebgp_destroy_msg(BGPSTREAM_PARSEBGP_FDATA);
      record->__format_data->data = NULL;
      refill = 1;
      goto refill;
    }
    // else: its a fatal error
    bgpstream_log(BGPSTREAM_LOG_ERR, "ERROR: Failed to parse message (%d:%s)\n",
                  err, parsebgp_strerror(err));
    parsebgp_destroy_msg(BGPSTREAM_PARSEBGP_FDATA);
    record->__format_data->data = NULL;
    return BGPSTREAM_FORMAT_CORRUPTED_DUMP;
  }
  // else: successful read
  state->ptr += dec_len;
  state->remain -= dec_len;
  state->successful_read_cnt++;
  assert(BGPSTREAM_PARSEBGP_FDATA != NULL);

  // got a message!
  // let the caller decide if they want it
  if ((filter = cb(format, BGPSTREAM_PARSEBGP_FDATA, &ts_sec)) < 0) {
    bgpstream_log(BGPSTREAM_LOG_ERR, "Format-specific filtering failed");
    return -1;
  }

  if (filter == 0) {
    // move on to the next record
    if (skipped_cnt == UINT64_MAX) {
      // probably this will never happen, but lets just be careful we don't
      // wrap and think we haven't skipped anything
      skipped_cnt = 0;
    }
    skipped_cnt++;
    parsebgp_destroy_msg(BGPSTREAM_PARSEBGP_FDATA);
    record->__format_data->data = NULL;
    // there is a cool corner case here when our buffer ends perfectly at the
    // end of a message, AND we filter the message out. previously i had a
    // simple "continue" which would have dropped out of the loop (since
    // remain == 0) and then would have been caught by the EOF check below.
    // to avoid this, we jump back to the refill point (but without a forced
    // refill), which in the normal case will drop into this loop as if a
    // continue had been called, and in the special case where remain == 0,
    // will try and refill the buffer.
    refill = 0; // don't force the refill, just let it happen naturally
    goto refill;
  }

  // valid message, and it passes our filters
  state->valid_read_cnt++;
  record->status = BGPSTREAM_RECORD_STATUS_VALID_RECORD;

  // if this is the first record we read and no previous
  // valid record has been discarded because of time
  if (state->valid_read_cnt == 1 && state->successful_read_cnt == 1) {
    record->dump_pos = BGPSTREAM_DUMP_START;
  } else {
    record->dump_pos = BGPSTREAM_DUMP_MIDDLE;
    // NB when the *next* record is pre-fetched, this may be changed to
    // end-of-dump by the reader (since we'll discover that there are no more
    // records)
  }

  // update the record time
  record->attributes.record_time = ts_sec;

  // we successfully read a record, return it
  return BGPSTREAM_FORMAT_OK;
}

void bgpstream_parsebgp_opts_init(parsebgp_opts_t *opts)
{
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
}
