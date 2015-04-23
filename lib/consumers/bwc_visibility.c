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

#include "bgpstream_utils_pfx_set.h"
#include "bgpstream_utils_id_set.h"


#include "bgpwatcher_consumer_interface.h"

#include "bwc_visibility.h"

#define NAME                        "visibility"
#define CONSUMER_METRIC_PREFIX      "prefix-visibility"

#define BUFFER_LEN 1024
#define METRIC_PREFIX_FORMAT       "%s.%s.v%d.%s"
#define META_METRIC_PREFIX_FORMAT  "%s.meta.bgpwatcher.consumer."NAME".%s"

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

  int peers_idx[BGPSTREAM_MAX_IP_VERSION_IDX];
  int ff_peers_idx[BGPSTREAM_MAX_IP_VERSION_IDX];
  int ff_asns_idx[BGPSTREAM_MAX_IP_VERSION_IDX];

  /* META metrics */
  int arrival_delay_idx;
  int processed_delay_idx;
} gen_metrics_t;


/* our 'instance' */
typedef struct bwc_visibility_state {

  int arrival_delay;
  int processed_delay;

  /** # pfxs in a full-feed table */
  int full_feed_size[BGPSTREAM_MAX_IP_VERSION_IDX];

  /** set of ASns providing a full-feed table */
  bgpstream_id_set_t *full_feed_asns[BGPSTREAM_MAX_IP_VERSION_IDX];  

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
	  STATE->full_feed_size[bgpstream_ipv2idx(BGPSTREAM_ADDR_VERSION_IPV4)] = atoi(optarg);
	  break;

	case '6':
	  STATE->full_feed_size[bgpstream_ipv2idx(BGPSTREAM_ADDR_VERSION_IPV4)] = atoi(optarg);
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
  char buffer[BUFFER_LEN];
  uint8_t i = 0;

 for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_FORMAT,
               CHAIN_STATE->metric_prefix, CONSUMER_METRIC_PREFIX, bgpstream_idx2number(i) , "peers_cnt");             
      if((STATE->gen_metrics.peers_idx[i] =
          timeseries_kp_add_key(STATE->kp, buffer)) == -1)
        {
          return -1;
        }

      snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_FORMAT,
               CHAIN_STATE->metric_prefix, CONSUMER_METRIC_PREFIX, bgpstream_idx2number(i) , "ff_peers_cnt");             
      if((STATE->gen_metrics.ff_peers_idx[i] =
          timeseries_kp_add_key(STATE->kp, buffer)) == -1)
        {
          return -1;
        }

      snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_FORMAT,
               CHAIN_STATE->metric_prefix, CONSUMER_METRIC_PREFIX, bgpstream_idx2number(i) , "ff_asns_cnt");             
      if((STATE->gen_metrics.ff_asns_idx[i] =
          timeseries_kp_add_key(STATE->kp, buffer)) == -1)
        {
          return -1;
        }
    }

  /* META Metrics */

  snprintf(buffer, BUFFER_LEN, META_METRIC_PREFIX_FORMAT,
           CHAIN_STATE->metric_prefix, "arrival_delay");
             
  if((STATE->gen_metrics.arrival_delay_idx =
      timeseries_kp_add_key(STATE->kp, buffer)) == -1)
    {
      return -1;
    }

  snprintf(buffer, BUFFER_LEN, META_METRIC_PREFIX_FORMAT,
           CHAIN_STATE->metric_prefix, "processed_delay");
             
  if((STATE->gen_metrics.processed_delay_idx =
      timeseries_kp_add_key(STATE->kp, buffer)) == -1)
    {
      return -1;
    }

    return 0;
}

static void find_ff_peers(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bgpstream_peer_id_t peerid;
  bgpstream_peer_sig_t *sg;
  int pfx_cnt;
  int i;
  
  for(bgpwatcher_view_iter_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_peer(it);
      bgpwatcher_view_iter_next_peer(it))
    {
      /* grab the peer id and its signature */
      peerid = bgpwatcher_view_iter_peer_get_peer_id(it);
      sg = bgpwatcher_view_iter_peer_get_sig(it);

      for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
        {
          pfx_cnt =
            bgpwatcher_view_iter_peer_get_pfx_cnt(it,
                                                  bgpstream_idx2ipv(i),
                                                  BGPWATCHER_VIEW_FIELD_ACTIVE);
          /* does this peer have any v4 tables? */
          if(pfx_cnt > 0)
            {
              CHAIN_STATE->peer_ids_cnt[i]++;              
            }

          /* does this peer have a full-feed table? */
          if(pfx_cnt >= STATE->full_feed_size[i])
            {
              /* add to the  full_feed set */
              bgpstream_id_set_insert(CHAIN_STATE->full_feed_peer_ids[i], peerid);
              bgpstream_id_set_insert(STATE->full_feed_asns[i], sg->peer_asnumber);
            }

        }
    }

  // fprintf(stderr, "IDS: %"PRIu32"\n", CHAIN_STATE->peer_ids_cnt[0]);              

  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      CHAIN_STATE->full_feed_peer_asns_cnt[i] = bgpstream_id_set_size(STATE->full_feed_asns[i]);
    }
}


static void dump_gen_metrics(bwc_t *consumer)
{
  int i;
  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
        timeseries_kp_set(STATE->kp, STATE->gen_metrics.peers_idx[i],
                    CHAIN_STATE->peer_ids_cnt[i]);
        timeseries_kp_set(STATE->kp, STATE->gen_metrics.ff_peers_idx[i],
                          bgpstream_id_set_size(CHAIN_STATE->full_feed_peer_ids[i]));
        timeseries_kp_set(STATE->kp, STATE->gen_metrics.ff_asns_idx[i],
                          CHAIN_STATE->full_feed_peer_asns_cnt[i]);                
    }

  /* META metrics */
  timeseries_kp_set(STATE->kp, STATE->gen_metrics.arrival_delay_idx,
                    STATE->arrival_delay);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.processed_delay_idx,
                    STATE->processed_delay);

  STATE->arrival_delay = 0;
  STATE->processed_delay = 0;
}

static void
reset_chain_state(bwc_t *consumer)
{
  int i;
  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      CHAIN_STATE->peer_ids_cnt[i] = 0;
      bgpstream_id_set_clear(CHAIN_STATE->full_feed_peer_ids[i]);
      CHAIN_STATE->full_feed_peer_asns_cnt[i] = 0;
      CHAIN_STATE->usable_table_flag[i] = 0;
      
    }
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

  state->full_feed_size[bgpstream_ipv2idx(BGPSTREAM_ADDR_VERSION_IPV4)]  = IPV4_FULLFEED_SIZE;
  state->full_feed_size[bgpstream_ipv2idx(BGPSTREAM_ADDR_VERSION_IPV6)]  = IPV6_FULLFEED_SIZE;

  int i;

  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      /* we asssume chain state data are already allocated by the consumer mgr */      
      if((state->full_feed_asns[i] = bgpstream_id_set_create()) == NULL)
        {
          fprintf(stderr, "Error: unable to create full-feed ASns set\n");
          goto err;
        }      
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

  int i;

  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      if(state->full_feed_asns[i] != NULL)
        {
          bgpstream_id_set_destroy(state->full_feed_asns[i]);
        }      
    }
  
  timeseries_kp_free(&state->kp);

  free(state);

  BWC_SET_STATE(consumer, NULL);
}



int bwc_visibility_process_view(bwc_t *consumer, uint8_t interests,
                                bgpwatcher_view_t *view)
{
  bgpwatcher_view_iter_t *it;
  int i;
  /* this MUST come first */
  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      bgpstream_id_set_clear(STATE->full_feed_asns[i]);
    }
  
  reset_chain_state(consumer);

  // compute arrival delay
  STATE->arrival_delay = zclock_time()/1000 - bgpwatcher_view_get_time(view);

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  /* find the full-feed peers */
  find_ff_peers(consumer, it);

  bgpwatcher_view_iter_destroy(it);
  

  CHAIN_STATE->usable_table_flag[bgpstream_ipv2idx(BGPSTREAM_ADDR_VERSION_IPV4)] = 1;
  CHAIN_STATE->usable_table_flag[bgpstream_ipv2idx(BGPSTREAM_ADDR_VERSION_IPV6)] = 1;

  CHAIN_STATE->visibility_computed = 1;

  /* @todo decide later what are the usability rules */
  
  /* compute processed delay (must come prior to dump_gen_metrics) */
  STATE->processed_delay = zclock_time()/1000 - bgpwatcher_view_get_time(view);

  /* dump metrics and tables */
  dump_gen_metrics(consumer);

  /* now flush the kp */
  if(timeseries_kp_flush(STATE->kp, bgpwatcher_view_get_time(view)) != 0)
    {
      return -1;
    }

  return 0;
}
