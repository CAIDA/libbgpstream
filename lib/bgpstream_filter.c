/*
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "bgpstream_filter.h"
#include "bgpstream_log.h"
#include "utils.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

/* allocate memory for a new bgpstream filter */
bgpstream_filter_mgr_t *bgpstream_filter_mgr_create()
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR: create start");
  bgpstream_filter_mgr_t *bs_filter_mgr =
    (bgpstream_filter_mgr_t *)malloc_zero(sizeof(bgpstream_filter_mgr_t));
  if (bs_filter_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  bs_filter_mgr->time_intervals_min = -1;
  bs_filter_mgr->time_intervals_max = -1;
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR: create end");
  return bs_filter_mgr;
}

void bgpstream_filter_mgr_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
                                     bgpstream_filter_type_t filter_type,
                                     const char *filter_value)
{
  bgpstream_str_set_t **v = NULL;
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter start");
  if (bs_filter_mgr == NULL) {
    return; // nothing to customize
  }

  switch (filter_type) {
  case BGPSTREAM_FILTER_TYPE_ELEM_PEER_ASN:
    if (bs_filter_mgr->peer_asns == NULL) {
      if ((bs_filter_mgr->peer_asns = bgpstream_id_set_create()) == NULL) {
        bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter malloc failed");
        bgpstream_log(BGPSTREAM_LOG_ERR, "can't allocate memory");
        /* TODO: this function should return failure code!! */
        /* look for other assert(0)'s */
        assert(0);
        return;
      }
    }
    bgpstream_id_set_insert(bs_filter_mgr->peer_asns,
                            (uint32_t)strtoul(filter_value, NULL, 10));
    return;

  case BGPSTREAM_FILTER_TYPE_ELEM_TYPE:
    if (strcmp(filter_value, "ribs") == 0) {
      bs_filter_mgr->elemtype_mask |= (BGPSTREAM_FILTER_ELEM_TYPE_RIB);
    } else if (strcmp(filter_value, "announcements") == 0) {
      bs_filter_mgr->elemtype_mask |= (BGPSTREAM_FILTER_ELEM_TYPE_ANNOUNCEMENT);
    } else if (strcmp(filter_value, "withdrawals") == 0) {
      bs_filter_mgr->elemtype_mask |= (BGPSTREAM_FILTER_ELEM_TYPE_WITHDRAWAL);
    } else if (strcmp(filter_value, "peerstates") == 0) {
      bs_filter_mgr->elemtype_mask |= (BGPSTREAM_FILTER_ELEM_TYPE_PEERSTATE);
    } else {
      bgpstream_log(BGPSTREAM_LOG_ERR,
                    "\tBSF_MGR: %s is not a known element type",
                    filter_value);
      assert(0);
    }
    return;

  case BGPSTREAM_FILTER_TYPE_ELEM_ASPATH:
    if (bs_filter_mgr->aspath_exprs == NULL) {
      if ((bs_filter_mgr->aspath_exprs = bgpstream_str_set_create()) == NULL) {
        bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter malloc failed");
        bgpstream_log(BGPSTREAM_LOG_ERR, "\tBSF_MGR: can't allocate memory");
        return;
      }
    }

    bgpstream_str_set_insert(bs_filter_mgr->aspath_exprs, filter_value);
    return;

  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_LESS:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_EXACT:
  case BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_ANY: {
    bgpstream_pfx_storage_t pfx;
    uint8_t matchtype;

    if (bs_filter_mgr->prefixes == NULL) {
      if ((bs_filter_mgr->prefixes = bgpstream_patricia_tree_create(NULL)) ==
          NULL) {
        bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter malloc failed");
        bgpstream_log(BGPSTREAM_LOG_ERR, "\tBSF_MGR: can't allocate memory");
        return;
      }
    }
    bgpstream_str2pfx(filter_value, &pfx);
    if (filter_type == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_MORE ||
        filter_type == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX) {
      matchtype = BGPSTREAM_PREFIX_MATCH_MORE;
    } else if (filter_type == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_LESS) {
      matchtype = BGPSTREAM_PREFIX_MATCH_LESS;
    } else if (filter_type == BGPSTREAM_FILTER_TYPE_ELEM_PREFIX_EXACT) {
      matchtype = BGPSTREAM_PREFIX_MATCH_EXACT;
    } else {
      matchtype = BGPSTREAM_PREFIX_MATCH_ANY;
    }

    pfx.allowed_matches = matchtype;
    if (bgpstream_patricia_tree_insert(bs_filter_mgr->prefixes,
                                       (bgpstream_pfx_t *)&pfx) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter malloc failed");
      bgpstream_log(BGPSTREAM_LOG_ERR, "\tBSF_MGR: can't add prefix");
      return;
    }
    return;
  }
  case BGPSTREAM_FILTER_TYPE_ELEM_COMMUNITY: {
    int mask = 0;
    khiter_t k;
    int khret;

    bgpstream_community_t comm;
    if (bs_filter_mgr->communities == NULL) {
      if ((bs_filter_mgr->communities = kh_init(bgpstream_community_filter)) ==
          NULL) {
        bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter malloc failed");
        bgpstream_log(BGPSTREAM_LOG_ERR, "\tBSF_MGR: can't allocate memory");
        return;
      }
    }
    if ((mask = bgpstream_str2community(filter_value, &comm)) < 0) {
      bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: can't convert community");
      return;
    }

    if ((k = kh_get(bgpstream_community_filter, bs_filter_mgr->communities,
                    comm)) == kh_end(bs_filter_mgr->communities)) {
      k = kh_put(bgpstream_community_filter, bs_filter_mgr->communities, comm,
                 &khret);
      kh_value(bs_filter_mgr->communities, k) = mask;
    }

    /* we use the AND because the less restrictive filter wins over the more
     * restrictive:
     * e.g. 10:0, 10:* is equivalent to 10:*
     */
    kh_value(bs_filter_mgr->communities, k) =
      kh_value(bs_filter_mgr->communities, k) & mask;
    /* DEBUG: fprintf(stderr, "%s - %d\n",
     *                filter_value, kh_value(bs_filter_mgr->communities, k) );
     */
    return;
  }

  case BGPSTREAM_FILTER_TYPE_ELEM_IP_VERSION:
    if (strcmp(filter_value, "4") == 0) {
      bs_filter_mgr->ipversion = BGPSTREAM_ADDR_VERSION_IPV4;
    } else if (strcmp(filter_value, "6") == 0) {
      bs_filter_mgr->ipversion = BGPSTREAM_ADDR_VERSION_IPV6;
    } else {
      bgpstream_log(BGPSTREAM_LOG_ERR, "\tBSF_MGR: Unknown IP version %s, ignoring",
                         filter_value);
    }
    return;

  case BGPSTREAM_FILTER_TYPE_PROJECT:
    v = &bs_filter_mgr->projects;
    break;
  case BGPSTREAM_FILTER_TYPE_COLLECTOR:
    v = &bs_filter_mgr->collectors;
    break;
  case BGPSTREAM_FILTER_TYPE_ROUTER:
    v = &bs_filter_mgr->routers;
    break;
  case BGPSTREAM_FILTER_TYPE_RECORD_TYPE:
    v = &bs_filter_mgr->bgp_types;
    break;
  default:
    bgpstream_log(BGPSTREAM_LOG_ERR, "\tBSF_MGR: unknown filter - ignoring");
    return;
  }

  if (*v == NULL) {
    if ((*v = bgpstream_str_set_create()) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter malloc failed");
      bgpstream_log(BGPSTREAM_LOG_ERR, "\tBSF_MGR: can't allocate memory");
      return;
    }
  }
  bgpstream_str_set_insert(*v, filter_value);

  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter stop");
  return;
}

void bgpstream_filter_mgr_rib_period_filter_add(
  bgpstream_filter_mgr_t *bs_filter_mgr, uint32_t period)
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter start");
  assert(bs_filter_mgr != NULL);
  if (period != 0 && bs_filter_mgr->last_processed_ts == NULL) {
    if ((bs_filter_mgr->last_processed_ts = kh_init(collector_ts)) == NULL) {
      bgpstream_log(BGPSTREAM_LOG_ERR, 
        "\tBSF_MGR: can't allocate memory for collectortype map");
    }
  }
  bs_filter_mgr->rib_period = period;
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter end");
}

void bgpstream_filter_mgr_interval_filter_add(
  bgpstream_filter_mgr_t *bs_filter_mgr, uint32_t begin_time, uint32_t end_time)
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter start");
  if (bs_filter_mgr == NULL) {
    return; // nothing to customize
  }
  // create a new filter structure
  bgpstream_interval_filter_t *f =
    (bgpstream_interval_filter_t *)malloc(sizeof(bgpstream_interval_filter_t));
  if (f == NULL) {
    bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter malloc failed");
    bgpstream_log(BGPSTREAM_LOG_ERR, "\tBSF_MGR: can't allocate memory");
    return;
  }
  // copying filter values
  f->begin_time = begin_time;
  f->end_time = end_time;
  f->next = bs_filter_mgr->time_intervals;
  bs_filter_mgr->time_intervals = f;

  if (bs_filter_mgr->time_intervals_min == -1 ||
      begin_time < bs_filter_mgr->time_intervals_min) {
    bs_filter_mgr->time_intervals_min = begin_time;
  }
  if (bs_filter_mgr->time_intervals_max == -1 ||
      end_time > bs_filter_mgr->time_intervals_max ||
      end_time == BGPSTREAM_FOREVER) {
    bs_filter_mgr->time_intervals_max = end_time;
  }

  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: add_filter stop");
}

int bgpstream_filter_mgr_validate(bgpstream_filter_mgr_t *filter_mgr)
{
  bgpstream_interval_filter_t *tif;
  /* currently we only validate the intervals */
  if (filter_mgr->time_intervals != NULL) {
    tif = filter_mgr->time_intervals;

    while (tif != NULL) {
      if (tif->end_time != BGPSTREAM_FOREVER &&
          tif->begin_time > tif->end_time) {
        /* invalid interval */
        fprintf(stderr, "ERROR: Interval %" PRIu32 ",%" PRIu32 " is invalid\n",
                tif->begin_time, tif->end_time);
        return -1;
      }

      tif = tif->next;
    }
  }

  return 0;
}

/* destroy the memory allocated for bgpstream filter */
void bgpstream_filter_mgr_destroy(bgpstream_filter_mgr_t *bs_filter_mgr)
{
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: destroy start");
  if (bs_filter_mgr == NULL) {
    return; // nothing to destroy
  }
  // destroying filters
  bgpstream_interval_filter_t *tif;
  khiter_t k;
  // projects
  if (bs_filter_mgr->projects != NULL) {
    bgpstream_str_set_destroy(bs_filter_mgr->projects);
  }
  // collectors
  if (bs_filter_mgr->collectors != NULL) {
    bgpstream_str_set_destroy(bs_filter_mgr->collectors);
  }
  // routers
  if (bs_filter_mgr->routers != NULL) {
    bgpstream_str_set_destroy(bs_filter_mgr->routers);
  }
  // bgp_types
  if (bs_filter_mgr->bgp_types != NULL) {
    bgpstream_str_set_destroy(bs_filter_mgr->bgp_types);
  }
  // peer asns
  if (bs_filter_mgr->peer_asns != NULL) {
    bgpstream_id_set_destroy(bs_filter_mgr->peer_asns);
  }
  // aspath expressions
  if (bs_filter_mgr->aspath_exprs != NULL) {
    bgpstream_str_set_destroy(bs_filter_mgr->aspath_exprs);
  }
  // prefixes
  if (bs_filter_mgr->prefixes != NULL) {
    bgpstream_patricia_tree_destroy(bs_filter_mgr->prefixes);
  }
  // communities
  if (bs_filter_mgr->communities != NULL) {
    kh_destroy(bgpstream_community_filter, bs_filter_mgr->communities);
  }
  // time_intervals
  tif = NULL;
  while (bs_filter_mgr->time_intervals != NULL) {
    tif = bs_filter_mgr->time_intervals;
    bs_filter_mgr->time_intervals = bs_filter_mgr->time_intervals->next;
    free(tif);
  }
  // rib/update frequency
  if (bs_filter_mgr->last_processed_ts != NULL) {
    for (k = kh_begin(bs_filter_mgr->last_processed_ts);
         k != kh_end(bs_filter_mgr->last_processed_ts); ++k) {
      if (kh_exist(bs_filter_mgr->last_processed_ts, k)) {
        free(kh_key(bs_filter_mgr->last_processed_ts, k));
      }
    }
    kh_destroy(collector_ts, bs_filter_mgr->last_processed_ts);
  }
  // free the mgr structure
  free(bs_filter_mgr);
  bs_filter_mgr = NULL;
  bgpstream_log(BGPSTREAM_LOG_VFINE, "\tBSF_MGR:: destroy end");
}
