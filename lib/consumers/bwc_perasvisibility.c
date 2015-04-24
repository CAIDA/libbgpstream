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

#include "bgpstream_utils_pfx_set.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_perasvisibility.h"


#define NAME                         "per-as-visibility"
#define CONSUMER_METRIC_PREFIX       "prefix-visibility.asn"


#define BUFFER_LEN 1024
#define METRIC_PREFIX_FORMAT       "%s.%s.%"PRIu32".v%d.%s"
#define METRIC_PREFIX_PERC_FORMAT  "%s.%s.%"PRIu32".v%d.perc.%s.%s"
#define META_METRIC_PREFIX_FORMAT  "%s.meta.bgpwatcher.consumer."NAME".%s"



#define STATE					\
  (BWC_GET_STATE(consumer, perasvisibility))

#define CHAIN_STATE                             \
  (BWC_GET_CHAIN_STATE(consumer))


/* our 'class' */
static bwc_t bwc_perasvisibility = {
  BWC_ID_PERASVISIBILITY,
  NAME,
  BWC_GENERATE_PTRS(perasvisibility)
};


typedef enum {
  VIS_25_PERCENT = 0,
  VIS_50_PERCENT = 1,
  VIS_75_PERCENT = 2,
  VIS_100_PERCENT = 3,
} vis_percentiles_t;

typedef struct peras_info {

  /** All v4 prefixes that this AS observed */
  bgpstream_ipv4_pfx_set_t *v4pfxs;
  
  /** All v6 prefixes that this AS observed */
  bgpstream_ipv6_pfx_set_t *v6pfxs;

  /* sum full feed ASns observing prefixes 
   * originated by this AS*/
  uint32_t ff_asns_sum[BGPSTREAM_MAX_IP_VERSION_IDX];

  /** number of visible prefixes based on a percentage
   * threshold (25, 50, 75, 100 percent) */
  uint32_t visible_pfxs[4*BGPSTREAM_MAX_IP_VERSION_IDX];
  
  uint32_t all_pfxs_idx[BGPSTREAM_MAX_IP_VERSION_IDX];
  uint32_t ff_asns_sum_idx[BGPSTREAM_MAX_IP_VERSION_IDX];
  uint32_t visible_pfxs_idx[4*BGPSTREAM_MAX_IP_VERSION_IDX];  

} peras_info_t;

/** Map from ASN => v4PFX-SET */
KHASH_INIT(as_pfxs,
	   uint32_t,
	   peras_info_t,
	   1,
	   kh_int_hash_func,
	   kh_int_hash_equal);

KHASH_INIT(peerid_set,
           bgpstream_peer_id_t,
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

  /** Map from ASN => PFX-SET */
  khash_t(as_pfxs) *as_pfxs;

  /** Timeseries Key Package (general) */
  timeseries_kp_t *kp_gen;

  /** Timeseries Key Packages (v4/v6) */
  timeseries_kp_t *kp[BGPSTREAM_MAX_IP_VERSION_IDX];

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
  char buffer[BUFFER_LEN];

  /* META Metrics */
  snprintf(buffer, BUFFER_LEN, META_METRIC_PREFIX_FORMAT,
           CHAIN_STATE->metric_prefix, "arrival_delay");
             
  if((STATE->gen_metrics.arrival_delay_idx =
      timeseries_kp_add_key(STATE->kp_gen, buffer)) == -1)
    {
      return -1;
    }

  snprintf(buffer, BUFFER_LEN, META_METRIC_PREFIX_FORMAT,
           CHAIN_STATE->metric_prefix, "processed_delay");
             
  if((STATE->gen_metrics.processed_delay_idx =
      timeseries_kp_add_key(STATE->kp_gen, buffer)) == -1)
    {
      return -1;
    }

  return 0;
}

static void peras_info_destroy(peras_info_t info)
{
  bgpstream_ipv4_pfx_set_destroy(info.v4pfxs);
  bgpstream_ipv6_pfx_set_destroy(info.v6pfxs);
}

static
char *percentage_string(int i)
{
  switch(i)
    {
    case 0:
      return "25";
    case 1:
      return "50";
    case 2:
      return "75";
    case 3:
      return "100";
    default:
      return "ERROR";
    }
  return "ERROR";
}

static peras_info_t *
as_pfxs_get_info(bwc_t *consumer,                 
                 uint32_t asn)
{
  peras_info_t *info = NULL;
  char buffer[BUFFER_LEN];
  khiter_t k;
  int khret;
  int i,j;
  
  if((k = kh_get(as_pfxs, STATE->as_pfxs, asn)) == kh_end(STATE->as_pfxs))
    {
      k = kh_put(as_pfxs, STATE->as_pfxs, asn, &khret);

      info = &kh_value(STATE->as_pfxs, k);

      if((info->v4pfxs = bgpstream_ipv4_pfx_set_create()) == NULL)
        {
          return NULL;
        }
      if((info->v6pfxs = bgpstream_ipv6_pfx_set_create()) == NULL)
        {
          return NULL;
        }

      
      for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
        {
          info->ff_asns_sum[i] = 0;
          
          snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_FORMAT,
                   CHAIN_STATE->metric_prefix, CONSUMER_METRIC_PREFIX, asn, bgpstream_idx2number(i) , "prefixes_cnt");             
          if((info->all_pfxs_idx[i] =
              timeseries_kp_add_key(STATE->kp[i], buffer)) == -1)
            {
              return NULL;
            }

          snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_FORMAT,
                   CHAIN_STATE->metric_prefix, CONSUMER_METRIC_PREFIX, asn, bgpstream_idx2number(i) , "asns_vis_sum");             
          if((info->ff_asns_sum_idx[i] =
              timeseries_kp_add_key(STATE->kp[i], buffer)) == -1)
            {
              return NULL;
            }

          for(j = 0; j<4; j++)
            {
              info->visible_pfxs[i*4+j] = 0;
              snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_PERC_FORMAT,
                       CHAIN_STATE->metric_prefix, CONSUMER_METRIC_PREFIX,
                       asn, bgpstream_idx2number(i), percentage_string(j), "visible_prefixes_cnt");             
              if((info->visible_pfxs_idx[i*4+j] =
                  timeseries_kp_add_key(STATE->kp[i], buffer)) == -1)
                {
                  return NULL;
                }
            }             
        }      
    }
  else
    {
      info = &kh_value(STATE->as_pfxs, k);
    }

  return info;
}

static void
update_visibility_counters(uint32_t *visibility_counters, int asns_count, int vX_ff)
{
  if(vX_ff == 0)
    {
      return;
    }
  double ratio = (double) asns_count / (double) vX_ff;

  if(ratio == 1)
    {
      visibility_counters[VIS_100_PERCENT]++;
    }
  if(ratio >= 0.75)
    {
      visibility_counters[VIS_75_PERCENT]++;
    }
  if(ratio >= 0.5)
    {
      visibility_counters[VIS_50_PERCENT]++;
    }
  if(ratio >= 0.25)
    {
      visibility_counters[VIS_25_PERCENT]++;
    }                  
}


KHASH_SET_INIT_INT(int_set);

static int flip_table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bgpstream_pfx_t *pfx;
  bgpstream_peer_id_t peerid;
  bgpstream_peer_sig_t *sg;
  khiter_t k;
  int khret;
  
  /* full feed asns observing a prefix */
  bgpstream_id_set_t *ff_asns = bgpstream_id_set_create();
  int asns_count = 0;

  uint32_t asn;

  /*  origin ases 
   * (as seen by full feed peers) */
  khash_t(int_set) *ff_origin_asns = kh_init(int_set);
  
  peras_info_t *info;
  int i;
  
  for(bgpwatcher_view_iter_first_pfx(it, 0 /* all ip versions*/, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_pfx(it);
      bgpwatcher_view_iter_next_pfx(it))
    {

      pfx = bgpwatcher_view_iter_pfx_get_pfx(it);
      i = bgpstream_ipv2idx(pfx->address.version);
      
      /* only consider ipv4 prefixes whose mask is shorter than a /6 */
      if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4 &&
         pfx->mask_len < BWC_GET_CHAIN_STATE(consumer)->pfx_vis_mask_len_threshold)
        {
          continue;
        }      

      /* iterate over the peers for the current pfx and get the number of unique
       * full feed AS numbers observing this prefix as well as the unique set of
       * origin ASes */
      for(bgpwatcher_view_iter_pfx_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
          bgpwatcher_view_iter_pfx_has_more_peer(it);
          bgpwatcher_view_iter_pfx_next_peer(it))
        {
          /* only consider peers that are full-feed */
          peerid = bgpwatcher_view_iter_peer_get_peer_id(it);
          sg = bgpwatcher_view_iter_peer_get_sig(it);          
          
            if(bgpstream_id_set_exists(BWC_GET_CHAIN_STATE(consumer)->full_feed_peer_ids[i],
                                     peerid) == 0)
            {
              continue;
            }
          
          bgpstream_id_set_insert(ff_asns, sg->peer_asnumber);
          asn = bgpwatcher_view_iter_pfx_peer_get_orig_asn(it);

          assert(asn < BGPWATCHER_VIEW_ASN_NOEXPORT_START);
          
          kh_put(int_set, ff_origin_asns, asn, &khret);
          
        }

      asns_count = bgpstream_id_set_size(ff_asns);

      for(k = kh_begin(ff_origin_asns); k != kh_end(ff_origin_asns); ++k)
        {
          if(kh_exist(ff_origin_asns, k))
            {
              if((info = as_pfxs_get_info(consumer, kh_key(ff_origin_asns,k))) == NULL)
                {
                  return -1;
                }

              if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4)
                {
                  bgpstream_ipv4_pfx_set_insert(info->v4pfxs,
                                                (bgpstream_ipv4_pfx_t *)pfx);
                }
              else
                {
                  if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV6)
                    {
                      bgpstream_ipv6_pfx_set_insert(info->v6pfxs,
                                                    (bgpstream_ipv6_pfx_t *)pfx);
                    }
                }

              info->ff_asns_sum[bgpstream_ipv2idx(pfx->address.version)] += asns_count;
              update_visibility_counters(&info->visible_pfxs[bgpstream_ipv2idx(pfx->address.version)*4], asns_count,
                                         BWC_GET_CHAIN_STATE(consumer)->full_feed_peer_asns_cnt[i]);

            }
        }
      
      bgpstream_id_set_clear(ff_asns);
      kh_clear(int_set,ff_origin_asns);
    }
  
  bgpstream_id_set_destroy(ff_asns);
  kh_destroy(int_set, ff_origin_asns);
  
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
  int i = 0;
  int j = 0;
  for (k = kh_begin(STATE->as_pfxs); k != kh_end(STATE->as_pfxs); ++k)
    {
      if (kh_exist(STATE->as_pfxs, k))
	{
          info = &kh_val(STATE->as_pfxs, k);

          i = bgpstream_ipv2idx(BGPSTREAM_ADDR_VERSION_IPV4);
          timeseries_kp_set(STATE->kp[i], info->all_pfxs_idx[i],
                            bgpstream_ipv4_pfx_set_size(info->v4pfxs));
          bgpstream_ipv4_pfx_set_clear(info->v4pfxs);

          i = bgpstream_ipv2idx(BGPSTREAM_ADDR_VERSION_IPV6);
          timeseries_kp_set(STATE->kp[i], info->all_pfxs_idx[i],
                            bgpstream_ipv6_pfx_set_size(info->v6pfxs));
          bgpstream_ipv6_pfx_set_clear(info->v6pfxs);

          
          for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
            {
              timeseries_kp_set(STATE->kp[i], info->ff_asns_sum_idx[i],
                                info->ff_asns_sum[i]);
              info->ff_asns_sum[i] = 0;
              
              for(j=0; j<4; j++)
                {
                  timeseries_kp_set(STATE->kp[i], info->visible_pfxs_idx[i*4+j],
                                    info->visible_pfxs[i*4+j]);
                  info->visible_pfxs[i*4+j] = 0;
                  
                }
            }       
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
  int i;

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


  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      if((state->kp[i] =
          timeseries_kp_init(BWC_GET_TIMESERIES(consumer), 1)) == NULL)
        {
          fprintf(stderr, "Error: Could not create timeseries key package\n");
          goto err;
        }
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
  int i;
  
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

  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      timeseries_kp_free(&state->kp[i]);
    }

  free(state);

  BWC_SET_STATE(consumer, NULL);
}

int bwc_perasvisibility_process_view(bwc_t *consumer, uint8_t interests,
				     bgpwatcher_view_t *view)
{
  bgpwatcher_view_iter_t *it;
  int i;
  
  if(BWC_GET_CHAIN_STATE(consumer)->visibility_computed == 0)
    {
      fprintf(stderr,
              "ERROR: The Per-AS Visibility requires the Visibility consumer "
              "to be run first\n");
      return -1;
    }

  // compute arrival delay
  STATE->arrival_delay = zclock_time()/1000 - bgpwatcher_view_get_time(view);

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  /* flip the view into a per-AS table */
  flip_table(consumer, it);

  /* now dump the per-as table(s) */
  dump_table(consumer);

  /* now flush the kps */
  for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
    {
      if(BWC_GET_CHAIN_STATE(consumer)->usable_table_flag[i] != 0 &&
         timeseries_kp_flush(STATE->kp[i], bgpwatcher_view_get_time(view)) != 0)
        {
          return -1;
        }
    }

  // compute processed delay
  STATE->processed_delay = zclock_time()/1000 - bgpwatcher_view_get_time(view);
  /* dump the general metrics */
  dump_gen_metrics(consumer);

  /* now flush the gen kp */
  if(timeseries_kp_flush(STATE->kp_gen, bgpwatcher_view_get_time(view)) != 0)
    {
      return -1;
    }

  /* destroy the view iterator */
  bgpwatcher_view_iter_destroy(it);

  return 0;
}
