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
#define METRIC_PREFIX_TH_FORMAT    "%s."CONSUMER_METRIC_PREFIX".%"PRIu32".v%d.visibility_threshold.%s.%s"
#define META_METRIC_PREFIX_FORMAT  "%s.meta.bgpwatcher.consumer."NAME".%s"

#define MAX_NUM_PEERS 1024


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
  VIS_1_FF_ASN = 0,
  VIS_25_PERCENT = 1,
  VIS_50_PERCENT = 2,
  VIS_75_PERCENT = 3,
  VIS_100_PERCENT = 4,
} vis_thresholds_t;

#define VIS_THRESHOLDS_CNT 5


typedef struct visibility_counters {
  uint32_t visible_pfxs;
  uint64_t visibile_ips;
  uint32_t ff_peer_asns_sum;  
} visibility_counters_t;

typedef struct peras_info {

  /** All v4 prefixes that this AS observed */
  bgpstream_ipv4_pfx_set_t *v4pfxs;
  
  /** All v6 prefixes that this AS observed */
  bgpstream_ipv6_pfx_set_t *v6pfxs;
    
  /** number of visible prefixes based on 
   * thresholds (1 ff, or 25, 50, 75, 100 percent) */
  visibility_counters_t visibility_counters[VIS_THRESHOLDS_CNT*BGPSTREAM_MAX_IP_VERSION_IDX];
  
  uint32_t visible_pfxs_idx[VIS_THRESHOLDS_CNT*BGPSTREAM_MAX_IP_VERSION_IDX];  
  uint32_t visible_ips_idx[VIS_THRESHOLDS_CNT*BGPSTREAM_MAX_IP_VERSION_IDX];
  uint32_t ff_peer_asns_sum_idx[VIS_THRESHOLDS_CNT*BGPSTREAM_MAX_IP_VERSION_IDX];  

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
  int processing_time_idx;

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

  /** General metric indexes */
  uint32_t origin_asns[MAX_NUM_PEERS];
  uint16_t valid_origins; 

  /* META metric values */
  int arrival_delay;
  int processed_delay;
  int processing_time;
  
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

  snprintf(buffer, BUFFER_LEN, META_METRIC_PREFIX_FORMAT,
           CHAIN_STATE->metric_prefix, "processing_time");
             
  if((STATE->gen_metrics.processing_time_idx =
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
char *threshold_string(int i)
{
  switch(i)
    {
    case VIS_1_FF_ASN:
      return "min_1_ff_peer_asn";
    case VIS_25_PERCENT:
      return "min_25%_ff_peer_asns";
    case VIS_50_PERCENT:
      return "min_50%_ff_peer_asns";
    case VIS_75_PERCENT:
      return "min_75%_ff_peer_asns";
    case VIS_100_PERCENT:
      return "min_100%_ff_peer_asns";
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
          for(j = 0; j<VIS_THRESHOLDS_CNT; j++)
            {
              info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].visible_pfxs = 0;
              snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_TH_FORMAT,
                       CHAIN_STATE->metric_prefix, asn, bgpstream_idx2number(i), threshold_string(j),
                       "visible_prefixes_cnt");             
              if((info->visible_pfxs_idx[i*VIS_THRESHOLDS_CNT+j] =
                  timeseries_kp_add_key(STATE->kp[i], buffer)) == -1)
                {
                  return NULL;
                }
              
              info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].visibile_ips = 0;
              snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_TH_FORMAT,
                       CHAIN_STATE->metric_prefix, asn, bgpstream_idx2number(i), threshold_string(j),
                       "visibile_ips_cnt");             
              if((info->visible_ips_idx[i*VIS_THRESHOLDS_CNT+j] =
                  timeseries_kp_add_key(STATE->kp[i], buffer)) == -1)
                {
                  return NULL;
                }
              
              info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].ff_peer_asns_sum = 0;
              snprintf(buffer, BUFFER_LEN, METRIC_PREFIX_TH_FORMAT,
                       CHAIN_STATE->metric_prefix, asn, bgpstream_idx2number(i), threshold_string(j),
                       "ff_peer_asns_sum");             
              if((info->ff_peer_asns_sum_idx[i*VIS_THRESHOLDS_CNT+j] =
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
update_visibility_counters(visibility_counters_t *visibility_counters, uint8_t net_size,
                           int asns_count, int vX_ff)
{
  double ratio;
  uint64_t ips = 1 << net_size;
  if(vX_ff == 0 || asns_count <= 0)
    {
      return;
    }

  visibility_counters[VIS_1_FF_ASN].visible_pfxs++;
  visibility_counters[VIS_1_FF_ASN].visibile_ips += ips;
  visibility_counters[VIS_1_FF_ASN].ff_peer_asns_sum += asns_count;

  ratio = (double) asns_count / (double) vX_ff;
  
  if(ratio == 1)
    {
      visibility_counters[VIS_100_PERCENT].visible_pfxs++;
      visibility_counters[VIS_100_PERCENT].visibile_ips += ips;
      visibility_counters[VIS_100_PERCENT].ff_peer_asns_sum += asns_count;
    }
  if(ratio >= 0.75)
    {
      visibility_counters[VIS_75_PERCENT].visible_pfxs++;
      visibility_counters[VIS_75_PERCENT].visibile_ips += ips;
      visibility_counters[VIS_75_PERCENT].ff_peer_asns_sum += asns_count;
    }
  if(ratio >= 0.5)
    {
      visibility_counters[VIS_50_PERCENT].visible_pfxs++;
      visibility_counters[VIS_50_PERCENT].visibile_ips += ips;
      visibility_counters[VIS_50_PERCENT].ff_peer_asns_sum += asns_count;
    }
  if(ratio >= 0.25)
    {
      visibility_counters[VIS_25_PERCENT].visible_pfxs++;
      visibility_counters[VIS_25_PERCENT].visibile_ips += ips;
      visibility_counters[VIS_25_PERCENT].ff_peer_asns_sum += asns_count;
    }                  
}


KHASH_SET_INIT_INT(int_set);

static int flip_table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bgpstream_pfx_t *pfx;
  bgpstream_peer_id_t peerid;
  bgpstream_peer_sig_t *sg;
  uint8_t net_size;
  
  /* full feed asns observing a prefix */
  bgpstream_id_set_t *ff_asns = bgpstream_id_set_create();
  int asns_count = 0;

  /* origin AS */
  uint32_t origin_asn;
  
  peras_info_t *info;
  int i;

  uint16_t a;
  uint8_t found;
  
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

      STATE->valid_origins = 0;

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
          origin_asn = bgpwatcher_view_iter_pfx_peer_get_orig_asn(it);         
          assert(origin_asn < BGPWATCHER_VIEW_ASN_NOEXPORT_START);
          
          found = 0;
          for(a = 0; a < STATE->valid_origins; a++)
            {
              if(STATE->origin_asns[a] == origin_asn)
                {
                  found = 1;
                  break;
                }                
            }
          if(!found)
            {
              STATE->origin_asns[STATE->valid_origins] = origin_asn;
              STATE->valid_origins++;
            }
        }

      net_size = 0; 
      asns_count = bgpstream_id_set_size(ff_asns);

      for(a = 0; a < STATE->valid_origins; a++)        
        {
          if((info = as_pfxs_get_info(consumer, STATE->origin_asns[a])) == NULL)
            {
              return -1;
            }

          if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4)
            {
              bgpstream_ipv4_pfx_set_insert(info->v4pfxs,
                                            (bgpstream_ipv4_pfx_t *)pfx);
              net_size = 32 - pfx->mask_len;
            }
          else
            {
              if(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV6)
                {
                  bgpstream_ipv6_pfx_set_insert(info->v6pfxs,
                                                (bgpstream_ipv6_pfx_t *)pfx);
                  if(pfx->mask_len <= 64)
                    {
                      net_size = 64 - pfx->mask_len;
                    }
                  else
                    {
                      net_size = 64;
                    }
                }
            }

          update_visibility_counters(&info->visibility_counters[bgpstream_ipv2idx(pfx->address.version)*VIS_THRESHOLDS_CNT],
                                     net_size, asns_count,
                                     BWC_GET_CHAIN_STATE(consumer)->full_feed_peer_asns_cnt[i]);
        }
      
      bgpstream_id_set_clear(ff_asns);
    }
  
  bgpstream_id_set_destroy(ff_asns);
  
  return 0;     
}

static void dump_gen_metrics(bwc_t *consumer)
{

  /* META metrics */
  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.arrival_delay_idx,
                    STATE->arrival_delay);

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.processed_delay_idx,
                    STATE->processed_delay);
  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.processing_time_idx,
                    STATE->processing_time);

  STATE->arrival_delay = 0;
  STATE->processed_delay = 0;
  STATE->processing_time = 0;
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

          bgpstream_ipv4_pfx_set_clear(info->v4pfxs);
          bgpstream_ipv6_pfx_set_clear(info->v6pfxs);

          
          for(i=0; i<BGPSTREAM_MAX_IP_VERSION_IDX; i++)
            {
              
              for(j=0; j<VIS_THRESHOLDS_CNT; j++)
                {
                  timeseries_kp_set(STATE->kp[i], info->visible_pfxs_idx[i*VIS_THRESHOLDS_CNT+j],
                                    info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].visible_pfxs);
                  info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].visible_pfxs = 0;

                  timeseries_kp_set(STATE->kp[i], info->visible_ips_idx[i*VIS_THRESHOLDS_CNT+j],
                                    info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].visibile_ips);
                  info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].visibile_ips = 0;

                  timeseries_kp_set(STATE->kp[i], info->ff_peer_asns_sum_idx[i*VIS_THRESHOLDS_CNT+j],
                                    info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].ff_peer_asns_sum);
                  info->visibility_counters[i*VIS_THRESHOLDS_CNT+j].ff_peer_asns_sum = 0;

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

  state->valid_origins = 0;

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

  STATE->processing_time = STATE->processed_delay - STATE->arrival_delay;
  
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
