#include "bgpstream_parsebgp_common.h"
#include "bgpstream_utils_as_path_int.h"
#include "bgpstream_utils_community_int.h"
#include "bgpstream_log.h"
#include <assert.h>
#include <string.h>

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

/* -------------------- PUBLIC API FUNCTIONS -------------------- */

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
