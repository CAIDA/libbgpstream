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

#ifndef _BGPSTREAM_FILTER_H
#define _BGPSTREAM_FILTER_H

#include "bgpstream.h"
#include "bgpstream_constants.h"
#include "khash.h"

#define BGPSTREAM_FILTER_ELEM_TYPE_RIB 0x1
#define BGPSTREAM_FILTER_ELEM_TYPE_ANNOUNCEMENT 0x2
#define BGPSTREAM_FILTER_ELEM_TYPE_WITHDRAWAL 0x4
#define BGPSTREAM_FILTER_ELEM_TYPE_PEERSTATE 0x8

/* hash table community filter:
 * community -> filter mask (asn only, value only, both) */
KHASH_INIT(bgpstream_community_filter, bgpstream_community_t, uint8_t, 1,
           bgpstream_community_hash_value, bgpstream_community_equal_value);
typedef khash_t(bgpstream_community_filter) bgpstream_community_filter_t;

typedef struct struct_bgpstream_interval_filter_t {
  uint32_t begin_time;
  uint32_t end_time;
  struct struct_bgpstream_interval_filter_t *next;
} bgpstream_interval_filter_t;

KHASH_INIT(collector_ts, char *, uint32_t, 1, kh_str_hash_func,
           kh_str_hash_equal);

typedef khash_t(collector_ts) collector_ts_t;

typedef struct struct_bgpstream_filter_mgr_t {
  bgpstream_str_set_t *projects;
  bgpstream_str_set_t *collectors;
  bgpstream_str_set_t *routers;
  bgpstream_str_set_t *bgp_types;
  bgpstream_str_set_t *aspath_exprs;
  bgpstream_id_set_t *peer_asns;
  bgpstream_patricia_tree_t *prefixes;
  bgpstream_community_filter_t *communities;
  bgpstream_interval_filter_t *time_intervals;
  int64_t time_intervals_min; // lower bound of all intervals
  int64_t time_intervals_max; // upper bound of all intervals
  collector_ts_t *last_processed_ts;
  uint32_t rib_period;
  uint8_t ipversion;
  uint8_t elemtype_mask;
} bgpstream_filter_mgr_t;

/* allocate memory for a new bgpstream filter */
bgpstream_filter_mgr_t *bgpstream_filter_mgr_create();

/* configure filters in order to select a subset of the bgp data available */
void bgpstream_filter_mgr_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
                                     bgpstream_filter_type_t filter_type,
                                     const char *filter_value);

void bgpstream_filter_mgr_rib_period_filter_add(
  bgpstream_filter_mgr_t *bs_filter_mgr, uint32_t period);

void bgpstream_filter_mgr_interval_filter_add(
  bgpstream_filter_mgr_t *bs_filter_mgr, uint32_t begin_time,
  uint32_t end_time);

/* validate the current filters */
int bgpstream_filter_mgr_validate(bgpstream_filter_mgr_t *mgr);

/* destroy the memory allocated for bgpstream filter */
void bgpstream_filter_mgr_destroy(bgpstream_filter_mgr_t *bs_filter_mgr);

#endif /* _BGPSTREAM_FILTER_H */
