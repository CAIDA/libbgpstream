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

#include <czmq.h>

#include "utils.h"
#include "khash.h"

#include "bl_pfx_set.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_perasvisibility.h"

#define BUFFER_LEN 1024

#define NAME "per-as-visibility"

#define METRIC_PREFIX               "bgp.visibility.asn"

#define METRIC_ASN_V4PFX_FORMAT     METRIC_PREFIX".%"PRIu32".ipv4_pfx_cnt"
#define METRIC_ASN_V6PFX_FORMAT     METRIC_PREFIX".%"PRIu32".ipv6_pfx_cnt"

#define META_METRIC_PREFIX                              \
  "bgp.meta.bgpwatcher.consumer.per-as-visibility"
#define METRIC_ARRIVAL_DELAY        META_METRIC_PREFIX".arrival_delay"
#define METRIC_PROCESSED_DELAY      META_METRIC_PREFIX".processed_delay"

#define STATE					\
  (BWC_GET_STATE(consumer, perasvisibility))

/* our 'class' */
static bwc_t bwc_perasvisibility = {
  BWC_ID_PERASVISIBILITY,
  NAME,
  BWC_GENERATE_PTRS(perasvisibility)
};

typedef struct peras_info {

  /** Index of the v4 metric for this ASN in the KP */
  uint32_t v4_idx;

  /** Index of the v6 metric for this ASN in the KP */
  uint32_t v6_idx;

  /** The v4 prefixes that this AS observed */
  bl_ipv4_pfx_set_t *v4pfxs;

  /** The v4 prefixes that this AS observed */
  bl_ipv6_pfx_set_t *v6pfxs;
} peras_info_t;

/** Map from ASN => v4PFX-SET */
KHASH_INIT(as_pfxs,
	   uint32_t,
	   peras_info_t,
	   1,
	   kh_int_hash_func,
	   kh_int_hash_equal);

KHASH_INIT(peerid_set,
           bl_peerid_t,
           char,
           0,
           kh_int_hash_func,
           kh_int_hash_equal);

typedef struct gen_metrics {

  /* META metrics */
  int arrival_delay_idx;
  int processed_delay_idx;

} gen_metrics_t;

/* our 'instance' */
typedef struct bwc_perasvisibility_state {

  /** Map from ASN => v4PFX-SET */
  khash_t(as_pfxs) *as_pfxs;

  /** Timeseries Key Package (general) */
  timeseries_kp_t *kp_gen;

  /** Timeseries Key Package (v4) */
  timeseries_kp_t *kp_v4;

  /** Timeseries Key Package (v6) */
  timeseries_kp_t *kp_v6;

  /** General metric indexes */
  gen_metrics_t gen_metrics;

  /* META metric values */
  int arrival_delay;
  int processed_delay;

} bwc_perasvisibility_state_t;

/** Print usage information to stderr */
static void usage(bwc_t *consumer)
{
  fprintf(stderr,
	  "consumer usage: %s\n",
	  consumer->name);
}

/** Parse the arguments given to the consumer */
static int parse_args(bwc_t *consumer, int argc, char **argv)
{
  int opt;

  assert(argc > 0 && argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* remember the argv strings DO NOT belong to us */
  while((opt = getopt(argc, argv, ":?")) >= 0)
    {
      switch(opt)
	{
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
  /* META Metrics */
  if((STATE->gen_metrics.arrival_delay_idx =
      timeseries_kp_add_key(STATE->kp_gen, METRIC_ARRIVAL_DELAY)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.processed_delay_idx =
      timeseries_kp_add_key(STATE->kp_gen, METRIC_PROCESSED_DELAY)) == -1)
    {
      return -1;
    }

  return 0;
}

static void peras_info_destroy(peras_info_t info)
{
  bl_ipv4_pfx_set_destroy(info.v4pfxs);
  bl_ipv6_pfx_set_destroy(info.v6pfxs);
}

static peras_info_t *as_pfxs_get_info(bwc_perasvisibility_state_t *state,
                                      uint32_t asn)
{
  peras_info_t *info = NULL;
  char buffer[BUFFER_LEN];
  khiter_t k;
  int khret;

  if((k = kh_get(as_pfxs, state->as_pfxs, asn)) == kh_end(state->as_pfxs))
    {
      k = kh_put(as_pfxs, state->as_pfxs, asn, &khret);

      info = &kh_value(state->as_pfxs, k);

      snprintf(buffer, BUFFER_LEN,
               METRIC_ASN_V4PFX_FORMAT,
               asn);
      if((info->v4_idx = timeseries_kp_add_key(state->kp_v4, buffer)) == -1)
        {
          return NULL;
        }

      snprintf(buffer, BUFFER_LEN,
               METRIC_ASN_V6PFX_FORMAT,
               asn);
      if((info->v6_idx = timeseries_kp_add_key(state->kp_v6, buffer)) == -1)
        {
          return NULL;
        }

      if((info->v4pfxs = bl_ipv4_pfx_set_create()) == NULL)
        {
          return NULL;
        }
      if((info->v6pfxs = bl_ipv6_pfx_set_create()) == NULL)
        {
          return NULL;
        }
    }
  else
    {
      info = &kh_value(state->as_pfxs, k);
    }

  return info;
}

static int flip_v4table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bl_ipv4_pfx_t *v4pfx;

  bl_peerid_t peerid;
  bgpwatcher_pfx_peer_info_t *pfxinfo;

  peras_info_t *info;

  /* IPv4 */
  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {
      /* get the current v4 prefix */
      v4pfx = bgpwatcher_view_iter_get_v4pfx(it);

      /* only consider prefixes whose mask is shorter than a /6 */
      if(v4pfx->mask_len <
         BWC_GET_CHAIN_STATE(consumer)->pfx_vis_mask_len_threshold)
      	{
      	  continue;
      	}

      /* only consider pfxs with peers_cnt >= pfx_vis_threshold */
      if(bgpwatcher_view_iter_size(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER)
	 < BWC_GET_CHAIN_STATE(consumer)->pfx_vis_peers_threshold)
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
	  if(bl_id_set_exists(BWC_GET_CHAIN_STATE(consumer)->v4ff_peerids,
                              peerid) == 0)
            {
	      continue;
	    }

	  pfxinfo = bgpwatcher_view_iter_get_v4pfx_pfxinfo(it);

          if((info = as_pfxs_get_info(STATE, pfxinfo->orig_asn)) == NULL)
            {
              return -1;
            }
          bl_ipv4_pfx_set_insert(info->v4pfxs, *v4pfx);
	}
    }

  return 0;
}

static int flip_v6table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bl_ipv6_pfx_t *v6pfx;

  bl_peerid_t peerid;
  bgpwatcher_pfx_peer_info_t *pfxinfo;

  peras_info_t *info;

  /* IPv6 */
  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX))
    {
      /* get the current v6 prefix */
      v6pfx = bgpwatcher_view_iter_get_v6pfx(it);

      /* only consider prefixes whose mask is shorter than a /6 */
      if(v6pfx->mask_len <
         BWC_GET_CHAIN_STATE(consumer)->pfx_vis_mask_len_threshold)
      	{
      	  continue;
      	}

      /* only consider pfxs with peers_cnt >= pfx_vis_threshold */
      if(bgpwatcher_view_iter_size(it, BGPWATCHER_VIEW_ITER_FIELD_V6PFX_PEER)
	 < BWC_GET_CHAIN_STATE(consumer)->pfx_vis_peers_threshold)
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
	  if(bl_id_set_exists(BWC_GET_CHAIN_STATE(consumer)->v6ff_peerids,
                              peerid) == 0)
            {
	      continue;
	    }

	  pfxinfo = bgpwatcher_view_iter_get_v6pfx_pfxinfo(it);

          if((info = as_pfxs_get_info(STATE, pfxinfo->orig_asn)) == NULL)
            {
              return -1;
            }
          bl_ipv6_pfx_set_insert(info->v6pfxs, *v6pfx);
	}
    }

  return 0;
}

static void dump_gen_metrics(bwc_t *consumer)
{

  /* META metrics */
  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.arrival_delay_idx,
                    STATE->arrival_delay);

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.processed_delay_idx,
                    STATE->processed_delay);

  STATE->arrival_delay = 0;
  STATE->processed_delay = 0;
}

static void dump_table(bwc_t *consumer)
{
  khiter_t k;
  peras_info_t *info;

  for (k = kh_begin(STATE->as_pfxs); k != kh_end(STATE->as_pfxs); ++k)
    {
      if (kh_exist(STATE->as_pfxs, k))
	{
          info = &kh_val(STATE->as_pfxs, k);
          timeseries_kp_set(STATE->kp_v4, info->v4_idx,
                            bl_ipv4_pfx_set_size(info->v4pfxs));
          timeseries_kp_set(STATE->kp_v6, info->v6_idx,
                            bl_ipv6_pfx_set_size(info->v6pfxs));

          bl_ipv4_pfx_set_reset(info->v4pfxs);
          bl_ipv6_pfx_set_reset(info->v6pfxs);
	}
    }
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

  if((state->as_pfxs = kh_init(as_pfxs)) == NULL)
    {
      fprintf(stderr, "Error: Unable to create as visibility map\n");
      goto err;
    }

  if((state->kp_gen =
      timeseries_kp_init(BWC_GET_TIMESERIES(consumer), 1)) == NULL)
    {
      fprintf(stderr, "Error: Could not create timeseries key package (gen)\n");
      goto err;
    }

  if((state->kp_v4 =
      timeseries_kp_init(BWC_GET_TIMESERIES(consumer), 1)) == NULL)
    {
      fprintf(stderr, "Error: Could not create timeseries key package (v4)\n");
      goto err;
    }

  if((state->kp_v6 =
      timeseries_kp_init(BWC_GET_TIMESERIES(consumer), 1)) == NULL)
    {
      fprintf(stderr, "Error: Could not create timeseries key package (v6)\n");
      goto err;
    }


  /* parse the command line args */
  if(parse_args(consumer, argc, argv) != 0)
    {
      goto err;
    }

  /* react to args here */

  if(create_gen_metrics(consumer) != 0)
    {
      goto err;
    }

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
  if(state->as_pfxs != NULL)
    {
      kh_free_vals(as_pfxs, state->as_pfxs, peras_info_destroy);
      kh_destroy(as_pfxs, state->as_pfxs);
      state->as_pfxs = NULL;
    }

  timeseries_kp_free(&state->kp_gen);

  timeseries_kp_free(&state->kp_v4);

  timeseries_kp_free(&state->kp_v6);

  free(state);

  BWC_SET_STATE(consumer, NULL);
}

int bwc_perasvisibility_process_view(bwc_t *consumer, uint8_t interests,
				     bgpwatcher_view_t *view)
{
  bgpwatcher_view_iter_t *it;

  if(BWC_GET_CHAIN_STATE(consumer)->visibility_computed == 0)
    {
      fprintf(stderr,
              "ERROR: The Per-AS Visibility requires the Visibility consumer "
              "to be run first\n");
      return -1;
    }

  // compute arrival delay
  STATE->arrival_delay = zclock_time()/1000 - bgpwatcher_view_time(view);

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  /* flip the view into a per-AS table */
  flip_v4table(consumer, it);
  flip_v6table(consumer, it);

  /* now dump the per-as table(s) */
  dump_table(consumer);

  /* now flush the v4 kp */
  if(BWC_GET_CHAIN_STATE(consumer)->v4_usable != 0 &&
     timeseries_kp_flush(STATE->kp_v4, bgpwatcher_view_time(view)) != 0)
    {
      return -1;
    }

  /* now flush the v6 kp */
  if(BWC_GET_CHAIN_STATE(consumer)->v6_usable != 0 &&
     timeseries_kp_flush(STATE->kp_v6, bgpwatcher_view_time(view)) != 0)
    {
      return -1;
    }

  // compute processed delay
  STATE->processed_delay = zclock_time()/1000 - bgpwatcher_view_time(view);
  /* dump the general metrics */
  dump_gen_metrics(consumer);

  /* now flush the gen kp */
  if(timeseries_kp_flush(STATE->kp_gen, bgpwatcher_view_time(view)) != 0)
    {
      return -1;
    }

  /* destroy the view iterator */
  bgpwatcher_view_iter_destroy(it);

  return 0;
}
