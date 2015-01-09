/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "khash.h"

#include "bl_pfx_set.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_perasvisibility.h"

#define BUFFER_LEN 1024

#define NAME "per-as-visibility"

#define METRIC_PREFIX               "bgp.visibility"
#define METRIC_V4_PEERS_CNT      METRIC_PREFIX".v4_peers_cnt"
#define METRIC_V6_PEERS_CNT      METRIC_PREFIX".v6_peers_cnt"
#define METRIC_V4_FF_PEERS_CNT      METRIC_PREFIX".v4_full_feed_peers_cnt"
#define METRIC_V6_FF_PEERS_CNT      METRIC_PREFIX".v6_full_feed_peers_cnt"
#define METRIC_ASN_V4PFX_FORMAT     METRIC_PREFIX".asn.%"PRIu32".ipv4_pfx_cnt"
#define METRIC_ASN_V6PFX_FORMAT     METRIC_PREFIX".asn.%"PRIu32".ipv6_pfx_cnt"

#define ROUTED_PFX_PEERCNT 10
#define IPV4_FULLFEED_SIZE 400000
#define IPV6_FULLFEED_SIZE 10000

#define STATE					\
  (BWC_GET_STATE(consumer, perasvisibility))

/* our 'class' */
static bwc_t bwc_perasvisibility = {
  BWC_ID_PERASVISIBILITY,
  NAME,
  BWC_GENERATE_PTRS(perasvisibility)
};

/** Map from ASN => v4PFX-SET */
KHASH_INIT(as_v4pfxs /* name */,
	   uint32_t /* khkey_t */,
	   bl_ipv4_pfx_set_t* /* khval_t */,
	   1  /* kh_is_set */,
	   kh_int_hash_func /*__hash_func */,
	   kh_int_hash_equal /* __hash_equal */);

/** Map from ASN => v6PFX-SET */
KHASH_INIT(as_v6pfxs /* name */,
	   uint32_t /* khkey_t */,
	   bl_ipv6_pfx_set_t* /* khval_t */,
	   1  /* kh_is_set */,
	   kh_int_hash_func /*__hash_func */,
	   kh_int_hash_equal /* __hash_equal */);

KHASH_INIT(peerid_set,
          bl_peerid_t,
          char,
          0,
          kh_int_hash_func,
          kh_int_hash_equal);

/* our 'instance' */
typedef struct bwc_perasvisibility_state {

  /** Set of v4 full-feed peers */
  khash_t(peerid_set) *v4ff_peerids;

  /** Set of v6 full-feed peers */
  khash_t(peerid_set) *v6ff_peerids;

  int v4_peer_cnt;

  int v6_peer_cnt;

  /** Map from ASN => v4PFX-SET */
  khash_t(as_v4pfxs) *as_v4pfxs;

  /** Map from ASN => v6PFX-SET */
  khash_t(as_v6pfxs) *as_v6pfxs;

  /** Prefix visibility threshold */
  int pfx_vis_threshold;

} bwc_perasvisibility_state_t;

/** Print usage information to stderr */
static void usage(bwc_t *consumer)
{
  fprintf(stderr,
	  "consumer usage: %s\n"
	  "       -p <peer-cnt> # peers that must observe a pfx (default: %d)\n",
	  consumer->name,
	  ROUTED_PFX_PEERCNT);
}

/** Parse the arguments given to the consumer */
static int parse_args(bwc_t *consumer, int argc, char **argv)
{
  int opt;

  assert(argc > 0 && argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* remember the argv strings DO NOT belong to us */
  while((opt = getopt(argc, argv, ":p:?")) >= 0)
    {
      switch(opt)
	{
	case 'p':
	  STATE->pfx_vis_threshold = atoi(optarg);
	  break;

	case '?':
	case ':':
	default:
	  usage(consumer);
	  return -1;
	}
    }

  return 0;
}

static void find_ff_peers(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  int khret;

  bl_peerid_t peerid;
  int pfx_cnt;

  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_PEER);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_PEER);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      /* grab the peer id */
      peerid = bgpwatcher_view_iter_get_peerid(it);

      pfx_cnt = bgpwatcher_view_iter_get_peer_v4pfx_cnt(it);
      /* does this peer have any v4 table? */
      if(pfx_cnt > 0)
        {
          STATE->v4_peer_cnt++;
        }
      /* does this peer have a full-feed v4 table? */
      if(pfx_cnt >= IPV4_FULLFEED_SIZE)
        {
          /* add to the v4 fullfeed table */
          kh_put(peerid_set, STATE->v4ff_peerids, peerid, &khret);
        }

      pfx_cnt = bgpwatcher_view_iter_get_peer_v6pfx_cnt(it);
      /* does this peer have any v6 table? */
      if(pfx_cnt > 0)
        {
          STATE->v6_peer_cnt++;
        }
      /* does this peer have a full-feed v6 table? */
      if(pfx_cnt >= IPV6_FULLFEED_SIZE)
        {
          /* add to the v6 fullfeed table */
          kh_put(peerid_set, STATE->v6ff_peerids, peerid, &khret);
        }
    }
}

static void as_v4pfxs_insert(bwc_perasvisibility_state_t *state,
			     uint32_t asn, bl_ipv4_pfx_t *pfx)
{
  int khret;
  khiter_t k;
  bl_ipv4_pfx_set_t* pfx_set;

  if((k = kh_get(as_v4pfxs, state->as_v4pfxs, asn)) == kh_end(state->as_v4pfxs))
    {
      k = kh_put(as_v4pfxs, state->as_v4pfxs, asn, &khret);
      pfx_set = kh_value(state->as_v4pfxs, k) = bl_ipv4_pfx_set_create();
    }
  else
    {
      pfx_set = kh_value(state->as_v4pfxs, k);
    }

  assert(pfx_set != NULL);
  bl_ipv4_pfx_set_insert(pfx_set, *pfx);
}

static void as_v6pfxs_insert(bwc_perasvisibility_state_t *state,
			       uint32_t asn, bl_ipv6_pfx_t *pfx)
{
  int khret;
  khiter_t k;
  bl_ipv6_pfx_set_t* pfx_set;

  if((k = kh_get(as_v6pfxs, state->as_v6pfxs, asn)) == kh_end(state->as_v6pfxs))
    {
      k = kh_put(as_v6pfxs, state->as_v6pfxs, asn, &khret);
      pfx_set = kh_value(state->as_v6pfxs, k) = bl_ipv6_pfx_set_create();
    }
  else
    {
      pfx_set = kh_value(state->as_v6pfxs, k);
    }

  assert(pfx_set != NULL);
  bl_ipv6_pfx_set_insert(pfx_set, *pfx);
}

static void flip_v4table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bl_ipv4_pfx_t *v4pfx;

  bl_peerid_t peerid;
  bgpwatcher_pfx_peer_info_t *pfxinfo;

  /* IPv4 */
  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {
      /* get the current v4 prefix */
      v4pfx = bgpwatcher_view_iter_get_v4pfx(it);

      /* only consider pfxs with peers_cnt >= pfx_vis_threshold */
      if(bgpwatcher_view_iter_size(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER)
	 < STATE->pfx_vis_threshold)
	{
	  continue;
	}

      /* iterate over the peers for the current v4pfx */
      for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER);
	  !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER);
	  bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
	{
          /* only consider peers that are full-feed */
          peerid = bgpwatcher_view_iter_get_v4pfx_peerid(it);

          if(kh_get(peerid_set, STATE->v4ff_peerids, peerid)
             == kh_end(STATE->v4ff_peerids))
            {
              continue;
            }

	  pfxinfo = bgpwatcher_view_iter_get_v4pfx_pfxinfo(it);
	  as_v4pfxs_insert(STATE, pfxinfo->orig_asn, v4pfx);
	}
    }
}

static void flip_v6table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bl_ipv6_pfx_t *v6pfx;

  bl_peerid_t peerid;
  bgpwatcher_pfx_peer_info_t *pfxinfo;

  /* IPv6 */
  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX))
    {
      /* get the current v6 prefix */
      v6pfx = bgpwatcher_view_iter_get_v6pfx(it);

      /* only consider pfxs with peers_cnt >= pfx_vis_threshold */
      if(bgpwatcher_view_iter_size(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER)
	 < STATE->pfx_vis_threshold)
	{
	  continue;
	}

      /* iterate over the peers for the current v6pfx */
      for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER);
	  !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER);
	  bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER))
	{
          /* only consider peers that are full-feed */
          peerid = bgpwatcher_view_iter_get_v6pfx_peerid(it);

          if(kh_get(peerid_set, STATE->v6ff_peerids, peerid)
             == kh_end(STATE->v6ff_peerids))
            {
              continue;
            }

	  pfxinfo = bgpwatcher_view_iter_get_v6pfx_pfxinfo(it);
	  as_v6pfxs_insert(STATE, pfxinfo->orig_asn, v6pfx);
	}
    }
}

static void dump_table(bwc_t *consumer, uint32_t time)
{
  khiter_t k;
  char buffer[BUFFER_LEN];

  for (k = kh_begin(STATE->as_v4pfxs); k != kh_end(STATE->as_v4pfxs); ++k)
    {
      if (kh_exist(STATE->as_v4pfxs, k))
	{
	  // OUTPUT: number of ipv4 prefixes seen by each AS
	  snprintf(buffer, BUFFER_LEN,
		   METRIC_ASN_V4PFX_FORMAT,
		   kh_key(STATE->as_v4pfxs, k));
	  timeseries_set_single(BWC_GET_TIMESERIES(consumer),
				buffer,
				bl_ipv4_pfx_set_size(kh_value(STATE->as_v4pfxs, k)),
				time);
	}
    }

  for (k = kh_begin(STATE->as_v6pfxs); k != kh_end(STATE->as_v6pfxs); ++k)
    {
      if (kh_exist(STATE->as_v6pfxs, k))
	{
	  // OUTPUT: number of ipv6 prefixes seen by each AS
	  snprintf(buffer, BUFFER_LEN,
		   METRIC_ASN_V6PFX_FORMAT,
		   kh_key(STATE->as_v6pfxs, k));
	  timeseries_set_single(BWC_GET_TIMESERIES(consumer),
				buffer,
				bl_ipv6_pfx_set_size(kh_value(STATE->as_v6pfxs, k)),
				time);
	}
    }

  /* now clear the tables */
  kh_free_vals(as_v4pfxs, STATE->as_v4pfxs, bl_ipv4_pfx_set_destroy);
  kh_clear(as_v4pfxs, STATE->as_v4pfxs);
  kh_free_vals(as_v6pfxs, STATE->as_v6pfxs, bl_ipv6_pfx_set_destroy);
  kh_clear(as_v6pfxs, STATE->as_v6pfxs);
}

/* ==================== CONSUMER INTERFACE FUNCTIONS ==================== */

bwc_t *bwc_perasvisibility_alloc()
{
  return &bwc_perasvisibility;
}

int bwc_perasvisibility_init(bwc_t *consumer, int argc, char **argv)
{
  bwc_perasvisibility_state_t *state = NULL;

  if((state = malloc_zero(sizeof(bwc_perasvisibility_state_t))) == NULL)
    {
      return -1;
    }
  BWC_SET_STATE(consumer, state);

  /* set defaults here */

  state->pfx_vis_threshold = ROUTED_PFX_PEERCNT;

  if((state->as_v4pfxs = kh_init(as_v4pfxs)) == NULL)
    {
      fprintf(stderr, "Error: Unable to create as visibility map (v4)\n");
      goto err;
    }
  if((state->as_v6pfxs = kh_init(as_v6pfxs)) == NULL)
    {
      fprintf(stderr, "Error: unable to create as visibility map (v6)\n");
      goto err;
    }
  if((state->v4ff_peerids = kh_init(peerid_set)) == NULL)
    {
      fprintf(stderr, "Error: unable to create full-feed peers (v4)\n");
      goto err;
    }
  if((state->v6ff_peerids = kh_init(peerid_set)) == NULL)
    {
      fprintf(stderr, "Error: unable to create full-feed peers (v6)\n");
      goto err;
    }

  /* parse the command line args */
  if(parse_args(consumer, argc, argv) != 0)
    {
      goto err;
    }

  /* react to args here */

  return 0;

 err:
  return -1;
}

void bwc_perasvisibility_destroy(bwc_t *consumer)
{
  bwc_perasvisibility_state_t *state = STATE;

  if(state == NULL)
    {
      return;
    }

  /* destroy things here */
  if(state->as_v4pfxs != NULL)
    {
      kh_free_vals(as_v4pfxs, state->as_v4pfxs, bl_ipv4_pfx_set_destroy);
      kh_destroy(as_v4pfxs, state->as_v4pfxs);
      state->as_v4pfxs = NULL;
    }
  if(state->as_v6pfxs != NULL)
    {
      kh_free_vals(as_v6pfxs, state->as_v6pfxs, bl_ipv6_pfx_set_destroy);
      kh_destroy(as_v6pfxs, state->as_v6pfxs);
      state->as_v6pfxs = NULL;
    }
  if(state->v4ff_peerids != NULL)
    {
      kh_destroy(peerid_set, state->v4ff_peerids);
      state->v4ff_peerids = NULL;
    }
  if(state->v6ff_peerids != NULL)
    {
      kh_destroy(peerid_set, state->v6ff_peerids);
      state->v6ff_peerids = NULL;
    }

  free(state);

  BWC_SET_STATE(consumer, NULL);
}

/** @note this code ASSUMES that BGP Watcher is only publishing tables from
    FULL-FEED peers.  If this ever changes, then this code MUST be updated */
int bwc_perasvisibility_process_view(bwc_t *consumer, uint8_t interests,
				     bgpwatcher_view_t *view)
{
  bgpwatcher_view_iter_t *it;

  uint32_t time = bgpwatcher_view_time(view);

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  /* find the full-feed peers */
  find_ff_peers(consumer, it);

  /* flip the view into a per-AS table */
  flip_v4table(consumer, it);
  flip_v6table(consumer, it);

  /* destroy the view iterator */
  bgpwatcher_view_iter_destroy(it);

  /* how many v4 peers? */
  timeseries_set_single(BWC_GET_TIMESERIES(consumer),
                        METRIC_V4_PEERS_CNT,
                        STATE->v4_peer_cnt,
                        time);

  /* how many v6 peers? */
  timeseries_set_single(BWC_GET_TIMESERIES(consumer),
                        METRIC_V6_PEERS_CNT,
                        STATE->v6_peer_cnt,
                        time);

  /* how many ff v4 peers? */
  timeseries_set_single(BWC_GET_TIMESERIES(consumer),
                        METRIC_V4_FF_PEERS_CNT,
                        kh_size(STATE->v4ff_peerids),
                        time);

  /* how many ff v6 peers? */
  timeseries_set_single(BWC_GET_TIMESERIES(consumer),
                        METRIC_V6_FF_PEERS_CNT,
                        kh_size(STATE->v6ff_peerids),
                        time);

  kh_clear(peerid_set, STATE->v4ff_peerids);
  kh_clear(peerid_set, STATE->v6ff_peerids);
  STATE->v4_peer_cnt = 0;
  STATE->v6_peer_cnt = 0;

  /* now dump the per-as table */
  dump_table(consumer, time);

  return 0;
}
