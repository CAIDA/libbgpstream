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
#include "libipmeta.h"
#include "khash.h"

#include "bl_pfx_set.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_pergeovisibility.h"

#define BUFFER_LEN 1024

#define NAME "per-geo-visibility"

#define METRIC_PREFIX               "bgp.visibility"
#define METRIC_V4_PEERS_CNT         METRIC_PREFIX".v4_peers_cnt"
#define METRIC_V6_PEERS_CNT         METRIC_PREFIX".v6_peers_cnt"
#define METRIC_V4_FF_PEERS_CNT      METRIC_PREFIX".v4_full_feed_peers_cnt"
#define METRIC_V6_FF_PEERS_CNT      METRIC_PREFIX".v6_full_feed_peers_cnt"
#define METRIC_ASN_V4PFX_FORMAT     METRIC_PREFIX".asn.%"PRIu32".ipv4_pfx_cnt"
#define METRIC_ASN_V6PFX_FORMAT     METRIC_PREFIX".asn.%"PRIu32".ipv6_pfx_cnt"

#define ROUTED_PFX_PEERCNT 10
#define IPV4_FULLFEED_SIZE 400000
#define IPV6_FULLFEED_SIZE 10000

#define STATE					\
  (BWC_GET_STATE(consumer, pergeovisibility))

/* our 'class' */
static bwc_t bwc_pergeovisibility = {
  BWC_ID_PERGEOVISIBILITY,
  NAME,
  BWC_GENERATE_PTRS(pergeovisibility)
};

typedef struct pergeo_info {

  /** Index of the v4 metric for this CC in the KP */
  uint32_t v4_idx;

  /** Index of the v6 metric for this CC in the KP */
  uint32_t v6_idx;

  /** The v4 prefixes that this CC observed */
  bl_ipv4_pfx_set_t *v4pfxs;

  /** The v4 prefixes that this CC observed */
  bl_ipv6_pfx_set_t *v6pfxs;
  
} pergeo_info_t;


/** Map from COUNTRYCODE => v4PFX-SET */
KHASH_INIT(cc_pfxs,
	   char *,
	   pergeo_info_t,
	   1,
	   kh_str_hash_func,
	   kh_str_hash_equal);

KHASH_INIT(peerid_set,
           bl_peerid_t,
           char,
           0,
           kh_int_hash_func,
           kh_int_hash_equal);

typedef struct gen_metrics {

  int v4_peers_idx;

  int v6_peers_idx;

  int v4_ff_peers_idx;

  int v6_ff_peers_idx;

} gen_metrics_t;

/* our 'instance' */
typedef struct bwc_pergeovisibility_state {

  /** Set of v4 full-feed peers */
  khash_t(peerid_set) *v4ff_peerids;

  /** Set of v6 full-feed peers */
  khash_t(peerid_set) *v6ff_peerids;

  int v4_peer_cnt;

  int v6_peer_cnt;

  /** Map from CC => v4PFX-SET */
  khash_t(cc_pfxs) *countrycode_pfxs;

  /** Prefix visibility threshold */
  int pfx_vis_threshold;

  /** Timeseries Key Package */
  timeseries_kp_t *kp;

  /** General metric indexes */
  gen_metrics_t gen_metrics;

  /** ipmeta structures */
  ipmeta_t *ipmeta;
  ipmeta_provider_t *provider;
  ipmeta_record_set_t *records;
  
} bwc_pergeovisibility_state_t;

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

  return 0;
}

static void pergeo_info_destroy(pergeo_info_t info)
{
  bl_ipv4_pfx_set_destroy(info.v4pfxs);
  bl_ipv6_pfx_set_destroy(info.v6pfxs);
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

static pergeo_info_t *cc_pfxs_get_info(bwc_pergeovisibility_state_t *state,
                                      uint32_t asn)
{
  pergeo_info_t *info = NULL;
  char buffer[BUFFER_LEN];
  char country_code[3];
  khiter_t k;
  int khret;

  if((k = kh_get(cc_pfxs, state->countrycode_pfxs, country_code)) == kh_end(state->countrycode_pfxs))
    {
      k = kh_put(cc_pfxs, state->countrycode_pfxs, country_code, &khret);

      info = &kh_value(state->countrycode_pfxs, k);

      snprintf(buffer, BUFFER_LEN,
               METRIC_ASN_V4PFX_FORMAT,
               asn);
      if((info->v4_idx = timeseries_kp_add_key(state->kp, buffer)) == -1)
        {
          return NULL;
        }

      snprintf(buffer, BUFFER_LEN,
               METRIC_ASN_V6PFX_FORMAT,
               asn);
      if((info->v6_idx = timeseries_kp_add_key(state->kp, buffer)) == -1)
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
      info = &kh_value(state->countrycode_pfxs, k);
    }

  return info;
}

static int flip_v4table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
  bl_ipv4_pfx_t *v4pfx;

  bl_peerid_t peerid;
  bgpwatcher_pfx_peer_info_t *pfxinfo;

  pergeo_info_t *info;

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

          if((info = geo_pfxs_get_info(STATE, pfxinfo->orig_asn)) == NULL)
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

  pergeo_info_t *info;

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
  timeseries_kp_set(STATE->kp, STATE->gen_metrics.v4_peers_idx,
                    STATE->v4_peer_cnt);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.v6_peers_idx,
                    STATE->v6_peer_cnt);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.v4_ff_peers_idx,
                    kh_size(STATE->v4ff_peerids));

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.v6_ff_peers_idx,
                    kh_size(STATE->v6ff_peerids));

  kh_clear(peerid_set, STATE->v4ff_peerids);
  kh_clear(peerid_set, STATE->v6ff_peerids);
  STATE->v4_peer_cnt = 0;
  STATE->v6_peer_cnt = 0;
}

static void dump_table(bwc_t *consumer)
{
  khiter_t k;
  pergeo_info_t *info;

  for (k = kh_begin(STATE->countrycode_pfxs); k != kh_end(STATE->countrycode_pfxs); ++k)
    {
      if (kh_exist(STATE->countrycode_pfxs, k))
	{
          info = &kh_val(STATE->countrycode_pfxs, k);
          timeseries_kp_set(STATE->kp, info->v4_idx, bl_ipv4_pfx_set_size(info->v4pfxs));
          timeseries_kp_set(STATE->kp, info->v6_idx, bl_ipv6_pfx_set_size(info->v6pfxs));

          bl_ipv4_pfx_set_reset(info->v4pfxs);
          bl_ipv6_pfx_set_reset(info->v6pfxs);
	}
    }
}

/* ==================== CONSUMER INTERFACE FUNCTIONS ==================== */

bwc_t *bwc_pergeovisibility_alloc()
{
  return &bwc_pergeovisibility;
}

int bwc_pergeovisibility_init(bwc_t *consumer, int argc, char **argv)
{
  bwc_pergeovisibility_state_t *state = NULL;

  if((state = malloc_zero(sizeof(bwc_pergeovisibility_state_t))) == NULL)
    {
      return -1;
    }
  BWC_SET_STATE(consumer, state);

  /* set defaults here */

  state->pfx_vis_threshold = ROUTED_PFX_PEERCNT;

  // change it! (str -> set(pfxs)
  if((state->countrycode_pfxs = kh_init(cc_pfxs)) == NULL)
    {
      fprintf(stderr, "Error: Unable to create cc visibility map\n");
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

  if((state->kp = timeseries_kp_init(BWC_GET_TIMESERIES(consumer), 1)) == NULL)
    {
      fprintf(stderr, "Error: Could not create timeseries key package\n");
      goto err;
    }

  /* initialize ipmeta structure */
  if((state->ipmeta = ipmeta_init()) == NULL)
    {
      fprintf(stderr, "Error: Could not initialize ipmeta \n");
      goto err;
    }

/* parse the command line args */
  if(parse_args(consumer, argc, argv) != 0)
    {
      goto err;
    }

  /* react to args here */

  
  /* lookup the provider using the name  */
  if((state->provider = ipmeta_get_provider_by_name(state->ipmeta, "netacqu-edge")) == NULL)
    {
      fprintf(stderr, "ERROR: Invalid provider name: netacqu-edge)\n");
      goto err;
    }

  /* enable the provider  */
  if(ipmeta_enable_provider(state->ipmeta, state->provider,
			    " -b /data/external/netacuity-dumps/Edge-processed/2014-04-07.netacq-4-blocks.csv" 
                            " -l /data/external/netacuity-dumps/Edge-processed/2014-04-07.netacq-4-locations.csv" 
			    " -c /data/external/netacuity-dumps/country_codes.csv",
			    IPMETA_PROVIDER_DEFAULT_YES) != 0)
    {
      fprintf(stderr, "ERROR: Could not enable provider netacqu-edge\n");
      goto err;      
    }

  /* initialize a (reusable) record set structure  */
  if((state->records = ipmeta_record_set_init()) == NULL)
    {
      fprintf(stderr, "ERROR: Could not init record set\n");
      goto err;      
    }

  ipmeta_provider_netacq_edge_country_t **countries = NULL;  
  int num_countries = ipmeta_provider_netacq_edge_get_countries(state->provider, &countries);
  int i;
  khiter_t k;
  int khret;
  pergeo_info_t *geo_info;

  for(i=0; i < num_countries; i++)
    {
      // DEBUG
      printf("%s\n", countries[i]->iso2);
      // we assume netacq returns a set of unique countries
      // then we don't need to check if these iso2 are already
      // present in the countrycode map
      k = kh_put(cc_pfxs, state->countrycode_pfxs, 
		 strdup(countries[i]->iso2), &khret);
      
      geo_info = &kh_value(state->countrycode_pfxs, k);

      // initialize properly geo_info
      geo_info->v4pfxs = bl_ipv4_pfx_set_create();      
      // TODO: create ipv4 metric id for kp
      geo_info->v6pfxs = bl_ipv6_pfx_set_create();
      // TODO: create ipv6 metric id for kp
      
    }
     
  if(create_gen_metrics(consumer) != 0)
    {
      goto err;
    }

  return 0;

 err:
  // call the destroy?!? @ask-alistair
  return -1;
}

void bwc_pergeovisibility_destroy(bwc_t *consumer)
{
  bwc_pergeovisibility_state_t *state = STATE;

  if(state == NULL)
    {
      return;
    }

  /* destroy things here */
  if(state->countrycode_pfxs != NULL)
    {
      kh_free_vals(cc_pfxs, state->countrycode_pfxs, pergeo_info_destroy);
      kh_destroy(cc_pfxs, state->countrycode_pfxs);
      state->countrycode_pfxs = NULL;
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

  timeseries_kp_free(&state->kp);

  if(state->ipmeta != NULL)
    {
      ipmeta_free(state->ipmeta);
      state->ipmeta = NULL;
    }

  if(state->records != NULL)
    {
      ipmeta_record_set_free(&state->records);
      state->records = NULL;
    }
    
  free(state);

  BWC_SET_STATE(consumer, NULL);
}



int bwc_pergeovisibility_process_view(bwc_t *consumer, uint8_t interests,
				     bgpwatcher_view_t *view)
{

  bwc_pergeovisibility_state_t *state = STATE;

  bgpwatcher_view_iter_t *it;

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  /* find the full-feed peers */
  find_ff_peers(consumer, it);

  uint32_t addr; // network order
  uint8_t mask;
  ipmeta_record_t *rec;
  uint32_t num_ips;

  // -------------
  // for each ipv4 prefix
  ipmeta_lookup(state->provider, addr, mask, state->records);
  ipmeta_record_set_rewind(state->records);
  while ( (rec = ipmeta_record_set_next(state->records, &num_ips)) ) {
    // USE rec->country_code 
  }
  

  
  
  /* flip the view into a per-geo table */
  flip_v4table(consumer, it);
  flip_v6table(consumer, it);

  /* destroy the view iterator */
  bgpwatcher_view_iter_destroy(it);

  /* dump the general metrics */
  dump_gen_metrics(consumer);

  /* now dump the per-geo table */
  dump_table(consumer);

  /* now flush the kp */
  if(timeseries_kp_flush(STATE->kp, bgpwatcher_view_time(view)) != 0)
    {
      return -1;
    }

  return 0;
}
