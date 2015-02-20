/*
 * This file is part of bgpwatcher
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <libipmeta.h>
#include <czmq.h> /* for zclock_time() */

#include "utils.h"
#include "khash.h"

#include "bl_pfx_set.h"
#include "bgpstream_utils_id_set.h"


#include "bgpwatcher_consumer_interface.h"

#include "bwc_visibility.h"

#define BUFFER_LEN 1024

#define NAME "visibility"

#define METRIC_PREFIX               "bgp.visibility"
#define METRIC_V4_PEERS_CNT         METRIC_PREFIX".v4_peers_cnt"
#define METRIC_V6_PEERS_CNT         METRIC_PREFIX".v6_peers_cnt"
#define METRIC_V4_FF_PEERS_CNT      METRIC_PREFIX".v4_full_feed_peers_cnt"
#define METRIC_V6_FF_PEERS_CNT      METRIC_PREFIX".v6_full_feed_peers_cnt"

#define META_METRIC_PREFIX           "bgp.meta.bgpwatcher.consumer.visibility"
#define METRIC_ARRIVAL_DELAY         META_METRIC_PREFIX".arrival_delay"
#define METRIC_PROCESSED_DELAY       META_METRIC_PREFIX".processed_delay"

#define ROUTED_PFX_MIN_PEERCNT    10
#define ROUTED_PFX_MIN_MASK_LEN   6
#define IPV4_FULLFEED_SIZE        400000
#define IPV6_FULLFEED_SIZE        10000


#define STATE					\
  (BWC_GET_STATE(consumer, visibility))

#define CHAIN_STATE                             \
  (BWC_GET_CHAIN_STATE(consumer))

/* our 'class' */
static bwc_t bwc_visibility = {
  BWC_ID_VISIBILITY,
  NAME,
  BWC_GENERATE_PTRS(visibility)
};

/** key package ids related to
 *  generic metrics
 */
typedef struct gen_metrics {
  int v4_peers_idx;
  int v6_peers_idx;
  int v4_ff_peers_idx;
  int v6_ff_peers_idx;

  /* META metrics */
  int arrival_delay_idx;
  int processed_delay_idx;
} gen_metrics_t;


/* our 'instance' */
typedef struct bwc_visibility_state {

  int arrival_delay;
  int processed_delay;

  /** # pfxs in a v4 full-feed table */
  int v4_fullfeed_size;

  /** # pfxs in a v6 full-feed table */
  int v6_fullfeed_size;

  /** Timeseries Key Package */
  timeseries_kp_t *kp;

  /** General metric indexes */
  gen_metrics_t gen_metrics;

} bwc_visibility_state_t;


/** Print usage information to stderr */
static void usage(bwc_t *consumer)
{
  fprintf(stderr,
	  "consumer usage: %s\n"
          "       -4 <pfx-cnt>  # pfxs in a IPv4 full-feed table (default: %d)\n"
          "       -6 <pfx-cnt>  # pfxs in a IPv6 full-feed table (default: %d)\n"
	  "       -m <mask-len> minimum mask length for pfxs (default: %d)\n"
	  "       -p <peer-cnt> # peers that must observe a pfx (default: %d)\n",
	  consumer->name,
          IPV4_FULLFEED_SIZE,
          IPV6_FULLFEED_SIZE,
          ROUTED_PFX_MIN_MASK_LEN,
          ROUTED_PFX_MIN_PEERCNT);
}


/** Parse the arguments given to the consumer */
static int parse_args(bwc_t *consumer, int argc, char **argv)
{
  int opt;

  assert(argc > 0 && argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* remember the argv strings DO NOT belong to us */
  while((opt = getopt(argc, argv, ":4:6:m:p:?")) >= 0)
    {
      switch(opt)
	{
	case '4':
	  STATE->v4_fullfeed_size = atoi(optarg);
	  break;

	case '6':
	  STATE->v6_fullfeed_size = atoi(optarg);
	  break;

	case 'm':
	  CHAIN_STATE->pfx_vis_mask_len_threshold = atoi(optarg);
	  break;

	case 'p':
	  CHAIN_STATE->pfx_vis_peers_threshold = atoi(optarg);
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


static int create_gen_metrics(bwc_t *consumer)
{
  if((STATE->gen_metrics.v4_peers_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_V4_PEERS_CNT)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.v6_peers_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_V6_PEERS_CNT)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.v4_ff_peers_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_V4_FF_PEERS_CNT)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.v6_ff_peers_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_V6_FF_PEERS_CNT)) == -1)
    {
      return -1;
    }

  /* META Metrics */
  if((STATE->gen_metrics.arrival_delay_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_ARRIVAL_DELAY)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.processed_delay_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_PROCESSED_DELAY)) == -1)
    {
      return -1;
    }

    return 0;
}

static void find_ff_peers(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bgpstream_peer_id_t peerid;
  int pfx_cnt;

  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_PEER);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_PEER);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_PEER))
    {
      /* grab the peer id */
      peerid = bgpwatcher_view_iter_get_peerid(it);

      pfx_cnt = bgpwatcher_view_iter_get_peer_v4pfx_cnt(it);
      /* does this peer have any v4 tables? */
      if(pfx_cnt > 0)
        {
          CHAIN_STATE->v4_peer_cnt++;
        }
      /* does this peer have a full-feed v4 table? */
      if(pfx_cnt >= STATE->v4_fullfeed_size)
        {
          /* add to the v4 fullfeed set */
	  bgpstream_id_set_insert(CHAIN_STATE->v4ff_peerids, peerid);
        }

      pfx_cnt = bgpwatcher_view_iter_get_peer_v6pfx_cnt(it);
      /* does this peer have any v6 tables? */
      if(pfx_cnt > 0)
        {
          CHAIN_STATE->v6_peer_cnt++;
        }
      /* does this peer have a full-feed v6 table? */
      if(pfx_cnt >= STATE->v6_fullfeed_size)
        {
          /* add to the v6 fullfeed table */
	  bgpstream_id_set_insert(CHAIN_STATE->v6ff_peerids, peerid);
        }
    }
}

static void dump_gen_metrics(bwc_t *consumer)
{
  timeseries_kp_set(STATE->kp, STATE->gen_metrics.v4_peers_idx,
                    CHAIN_STATE->v4_peer_cnt);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.v6_peers_idx,
                    CHAIN_STATE->v6_peer_cnt);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.v4_ff_peers_idx,
                    bgpstream_id_set_size(CHAIN_STATE->v4ff_peerids));

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.v6_ff_peers_idx,
                    bgpstream_id_set_size(CHAIN_STATE->v6ff_peerids));

  /* META metrics */
  timeseries_kp_set(STATE->kp, STATE->gen_metrics.arrival_delay_idx,
                    STATE->arrival_delay);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.processed_delay_idx,
                    STATE->processed_delay);

  STATE->arrival_delay = 0;
  STATE->processed_delay = 0;
}

static void reset_chain_state(bwc_t *consumer)
{
  bgpstream_id_set_clear(CHAIN_STATE->v4ff_peerids);
  bgpstream_id_set_clear(CHAIN_STATE->v6ff_peerids);

  CHAIN_STATE->v4_peer_cnt = 0;
  CHAIN_STATE->v6_peer_cnt = 0;

  CHAIN_STATE->v4_usable = 0;
  CHAIN_STATE->v6_usable = 0;
}

/* ==================== CONSUMER INTERFACE FUNCTIONS ==================== */

bwc_t *bwc_visibility_alloc()
{
  return &bwc_visibility;
}

int bwc_visibility_init(bwc_t *consumer, int argc, char **argv)
{
  bwc_visibility_state_t *state = NULL;

  if((state = malloc_zero(sizeof(bwc_visibility_state_t))) == NULL)
    {
      return -1;
    }
  BWC_SET_STATE(consumer, state);

  /* set defaults here */
  CHAIN_STATE->pfx_vis_peers_threshold = ROUTED_PFX_MIN_PEERCNT;
  CHAIN_STATE->pfx_vis_mask_len_threshold = ROUTED_PFX_MIN_MASK_LEN;

  state->v4_fullfeed_size  = IPV4_FULLFEED_SIZE;
  state->v6_fullfeed_size  = IPV6_FULLFEED_SIZE;

  if((CHAIN_STATE->v4ff_peerids = bgpstream_id_set_create()) == NULL)
    {
      fprintf(stderr, "Error: unable to create full-feed peers (v4)\n");
      goto err;
    }
  if((CHAIN_STATE->v6ff_peerids = bgpstream_id_set_create()) == NULL)
    {
      fprintf(stderr, "Error: unable to create full-feed peers (v6)\n");
      goto err;
    }

  if((state->kp = timeseries_kp_init(BWC_GET_TIMESERIES(consumer), 1)) == NULL)
    {
      fprintf(stderr, "Error: Could not create timeseries key package\n");
      goto err;
    }

  /* parse the command line args */
  if(parse_args(consumer, argc, argv) != 0)
    {
      goto err;
    }

  if(create_gen_metrics(consumer) != 0)
    {
      goto err;
    }

  return 0;

 err:
  bwc_visibility_destroy(consumer);
  return -1;
}

void bwc_visibility_destroy(bwc_t *consumer)
{

  bwc_visibility_state_t *state = STATE;

  if(state == NULL)
    {
      return;
    }

  /* destroy things here */
  if(CHAIN_STATE->v4ff_peerids != NULL)
    {
      bgpstream_id_set_destroy(CHAIN_STATE->v4ff_peerids);
      CHAIN_STATE->v4ff_peerids = NULL;
    }
  if(CHAIN_STATE->v6ff_peerids != NULL)
    {
      bgpstream_id_set_destroy(CHAIN_STATE->v6ff_peerids);
      CHAIN_STATE->v6ff_peerids = NULL;
    }

  timeseries_kp_free(&state->kp);

  free(state);

  BWC_SET_STATE(consumer, NULL);
}



int bwc_visibility_process_view(bwc_t *consumer, uint8_t interests,
                                bgpwatcher_view_t *view)
{
  bgpwatcher_view_iter_t *it;

  /* this MUST come first */
  reset_chain_state(consumer);

  CHAIN_STATE->visibility_computed = 1;

  // compute arrival delay
  STATE->arrival_delay = zclock_time()/1000 - bgpwatcher_view_time(view);

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  /* find the full-feed peers */
  find_ff_peers(consumer, it);

  if(bgpstream_id_set_size(CHAIN_STATE->v4ff_peerids) >=
     CHAIN_STATE->pfx_vis_peers_threshold)
    {
      CHAIN_STATE->v4_usable = 1;
    }

  if(bgpstream_id_set_size(CHAIN_STATE->v6ff_peerids) >=
     CHAIN_STATE->pfx_vis_peers_threshold)
    {
      CHAIN_STATE->v6_usable = 1;
    }

  // compute processed delay (must come prior to dump_gen_metrics)
  STATE->processed_delay = zclock_time()/1000 - bgpwatcher_view_time(view);
  /* dump metrics and tables */
  dump_gen_metrics(consumer);

  /* now flush the kp */
  if(timeseries_kp_flush(STATE->kp, bgpwatcher_view_time(view)) != 0)
    {
      return -1;
    }

  return 0;
}
