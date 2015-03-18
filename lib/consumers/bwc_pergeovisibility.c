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

#include "utils.h"
#include "libipmeta.h"
#include "khash.h"
#include "czmq.h"

#include "bgpstream_utils_pfx_set.h"
#include "bgpstream_utils_id_set.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_pergeovisibility.h"

#define BUFFER_LEN 1024

#define NAME "per-geo-visibility"

#define METRIC_PREFIX               "bgp.visibility.geo.netacuity"

#define METRIC_CC_V4PFX_FORMAT      METRIC_PREFIX".%s.%s.ipv4_pfx_cnt"
#define METRIC_CC_V6PFX_FORMAT      METRIC_PREFIX".%s.%s.ipv6_pfx_cnt"

#define META_METRIC_PREFIX           \
  "bgp.meta.bgpwatcher.consumer.per-geo-visibility"
#define METRIC_CACHE_MISS_CNT        META_METRIC_PREFIX".cache_miss_cnt"
#define METRIC_CACHE_HITS_CNT        META_METRIC_PREFIX".cache_hit_cnt"
#define METRIC_ARRIVAL_DELAY         META_METRIC_PREFIX".arrival_delay"
#define METRIC_PROCESSED_DELAY       META_METRIC_PREFIX".processed_delay"
#define METRIC_MAXCOUNTRIES_PERPFX   META_METRIC_PREFIX".max_numcountries_perpfx"
#define METRIC_AVGCOUNTRIES_PERPFX   META_METRIC_PREFIX".avg_numcountries_perpfx"
#define METRIC_VISIBLE_PFXS          META_METRIC_PREFIX".visible_pfxs_cnt"
#define METRIC_MAXRECS_PERPFXS       META_METRIC_PREFIX".max_records_perpfx"

#define GEO_PROVIDER_NAME  "netacq-edge"


#define STATE					\
  (BWC_GET_STATE(consumer, pergeovisibility))

/* our 'class' */
static bwc_t bwc_pergeovisibility = {
  BWC_ID_PERGEOVISIBILITY,
  NAME,
  BWC_GENERATE_PTRS(pergeovisibility)
};


/** pergeo_info_t
 *  network visibility information related to a single
 *  geographical location (currently country codes)
 */
typedef struct pergeo_info {

  /** Index of the v4 metric for this CC in the KP */
  uint32_t v4_idx;

  /** Index of the v6 metric for this CC in the KP */
  // IPV6 --> uint32_t v6_idx;

  /** The number of v4 prefixes that this CC observed */
  int v4pfxs_cnt;

  /** The number of v6 prefixes that this CC observed */
  // IPV6 --> int v6pfxs_cnt;

  /* TODO: think about how to manage multiple geo
   * providers as well as multiple counters
   */

} pergeo_info_t;


/** Destroy pergeo information
 *  @param info structure to destroy
 */
static void pergeo_info_destroy(pergeo_info_t info)
{
  // currently there are no dynamic allocated memory
  //
}


/** Map from COUNTRYCODE => per geo info,
 *    i.e. a PFX-SET
 */
KHASH_INIT(cc_pfxs,
	   char *,
	   pergeo_info_t,
	   1,
	   kh_str_hash_func,
	   kh_str_hash_equal);


/** key package ids related to
 *  generic metrics
 */
typedef struct gen_metrics {

  int cache_misses_cnt_idx;
  int cache_hits_cnt_idx;
  int arrival_delay_idx;
  int processed_delay_idx;
  int max_numcountries_perpfx_idx;
  double avg_numcountries_perpfx_idx;
  int num_visible_pfx_idx;
  int max_records_perpfx_idx;

} gen_metrics_t;


/* our 'instance' */
typedef struct bwc_pergeovisibility_state {
  int cache_misses_cnt;
  int cache_hits_cnt;
  int arrival_delay;
  int processed_delay;
  int max_numcountries_perpfx;
  double avg_numcountries_perpfx;
  int num_visible_pfx;
  int max_records_perpfx;

  /** Map from CC => pfxs-generated info */
  khash_t(cc_pfxs) *countrycode_pfxs;

  /** netacq-edge files */
  char blocks_file[BUFFER_LEN];
  char locations_file[BUFFER_LEN];
  char countries_file[BUFFER_LEN];

  /** Timeseries Key Package (gen) */
  timeseries_kp_t *kp_gen;

  /** Timeseries Key Package (v4) */
  timeseries_kp_t *kp_v4;

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
	  "       -c <file>     country decode file (mandatory option)\n"
	  "       -b <file>     blocks file (mandatory option)\n"
	  "       -l <file>     locations file (mandatory option)\n",
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
  while((opt = getopt(argc, argv, ":b:c:l:?")) >= 0)
    {
      switch(opt)
	{
	case 'b':
	  strcpy(STATE->blocks_file, optarg);
	  break;
	case 'c':
	  strcpy(STATE->countries_file, optarg);
	  break;
	case 'l':
	  strcpy(STATE->locations_file, optarg);
	  break;
	case '?':
	case ':':
	default:
	  usage(consumer);
	  return -1;
	}
    }

  // blocks countries and locations are mandatory options
  if(STATE->blocks_file[0] == '\0' ||
     STATE->countries_file[0] ==  '\0' ||
     STATE->locations_file[0] == '\0')
    {
      usage(consumer);
      return -1;
    }


  return 0;
}

static int init_ipmeta(bwc_t *consumer)
{
  char provider_options[BUFFER_LEN] = "";

  /* lookup the provider using the name  */
  if((STATE->provider =
      ipmeta_get_provider_by_name(STATE->ipmeta, GEO_PROVIDER_NAME)) == NULL)
    {
      fprintf(stderr, "ERROR: Invalid provider name: %s\n", GEO_PROVIDER_NAME);
      return -1;
    }

  /* enable the provider  */
  snprintf(provider_options, BUFFER_LEN, "-b %s -l %s -c %s -D intervaltree",
	   STATE->blocks_file,
	   STATE->locations_file,
	   STATE->countries_file);

  if(ipmeta_enable_provider(STATE->ipmeta,
			    STATE->provider,
			    provider_options,
			    IPMETA_PROVIDER_DEFAULT_YES) != 0)
    {
      fprintf(stderr, "ERROR: Could not enable provider %s\n",
              GEO_PROVIDER_NAME);
      return -1;
    }

  /* initialize a (reusable) record set structure  */
  if((STATE->records = ipmeta_record_set_init()) == NULL)
    {
      fprintf(stderr, "ERROR: Could not init record set\n");
      return -1;
    }

  return 0;
}

static int create_per_cc_metrics(bwc_t *consumer)
{
  ipmeta_provider_netacq_edge_country_t **countries = NULL;
  int num_countries =
    ipmeta_provider_netacq_edge_get_countries(STATE->provider, &countries);
  int i;
  khiter_t k;
  int khret;
  pergeo_info_t *geo_info;
  char buffer[BUFFER_LEN];

  for(i=0; i < num_countries; i++)
    {
      // Warning: we assume netacq returns a set of unique countries
      // then we don't need to check if these iso2 are already
      // present in the countrycode map
      k = kh_put(cc_pfxs, STATE->countrycode_pfxs,
		 strdup(countries[i]->iso2), &khret);

      geo_info = &kh_value(STATE->countrycode_pfxs, k);

      // initialize properly geo_info and create ipvX metrics id for kp
      geo_info->v4pfxs_cnt = 0;

      snprintf(buffer, BUFFER_LEN, METRIC_CC_V4PFX_FORMAT,
               countries[i]->continent, countries[i]->iso2);
      if((geo_info->v4_idx = timeseries_kp_add_key(STATE->kp_v4, buffer)) == -1)
	{
	  fprintf(stderr, "ERROR: Could not create key metric\n");
	}

      // IPV6 --> geo_info->v6pfxs_cnt = 0;

      // IPV6 -->  snprintf(buffer, BUFFER_LEN, METRIC_CC_V6PFX_FORMAT, countries[i]->continent, countries[i]->iso2);
      // IPV6 -->  if((geo_info->v6_idx = timeseries_kp_add_key(STATE->kp, buffer)) == -1)
      // IPV6 --> {
      // IPV6 --> fprintf(stderr, "ERROR: Could not create key metric\n");
      // IPV6 --> }
    }

  return 0;
}

static int create_gen_metrics(bwc_t *consumer)
{
  if((STATE->gen_metrics.cache_misses_cnt_idx =
      timeseries_kp_add_key(STATE->kp_gen, METRIC_CACHE_MISS_CNT)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.cache_hits_cnt_idx =
      timeseries_kp_add_key(STATE->kp_gen, METRIC_CACHE_HITS_CNT)) == -1)
    {
      return -1;
    }

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

  if((STATE->gen_metrics.max_numcountries_perpfx_idx =
      timeseries_kp_add_key(STATE->kp_gen, METRIC_MAXCOUNTRIES_PERPFX)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.avg_numcountries_perpfx_idx =
      timeseries_kp_add_key(STATE->kp_gen, METRIC_AVGCOUNTRIES_PERPFX)) == -1)
    {
      return -1;
    }

    if((STATE->gen_metrics.num_visible_pfx_idx =
      timeseries_kp_add_key(STATE->kp_gen, METRIC_VISIBLE_PFXS)) == -1)
    {
      return -1;
    }

    if((STATE->gen_metrics.max_records_perpfx_idx =
      timeseries_kp_add_key(STATE->kp_gen, METRIC_MAXRECS_PERPFXS)) == -1)
    {
      return -1;
    }

    return 0;
}

static void dump_gen_metrics(bwc_t *consumer)
{

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.cache_misses_cnt_idx,
                    STATE->cache_misses_cnt);

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.cache_hits_cnt_idx,
                    STATE->cache_hits_cnt);

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.arrival_delay_idx,
                    STATE->arrival_delay);

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.processed_delay_idx,
                    STATE->processed_delay);

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.max_numcountries_perpfx_idx,
                    STATE->max_numcountries_perpfx);

  if(STATE->num_visible_pfx > 0)
    {
      STATE->avg_numcountries_perpfx =
        STATE->avg_numcountries_perpfx / (double) STATE->num_visible_pfx;
    }

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.avg_numcountries_perpfx_idx,
                    STATE->avg_numcountries_perpfx);

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.num_visible_pfx_idx,
                    STATE->num_visible_pfx);

  timeseries_kp_set(STATE->kp_gen, STATE->gen_metrics.max_records_perpfx_idx,
                    STATE->max_records_perpfx);

  STATE->cache_misses_cnt = 0;
  STATE->cache_hits_cnt = 0;
  STATE->arrival_delay = 0;
  STATE->processed_delay = 0;
  STATE->max_numcountries_perpfx = 0;
  STATE->avg_numcountries_perpfx = 0;
  STATE->num_visible_pfx = 0;
  // we do not reset STATE->max_records_perpfx

}

static void dump_v4table(bwc_t *consumer)
{
  khiter_t k;
  pergeo_info_t *info;

  for (k = kh_begin(STATE->countrycode_pfxs);
       k != kh_end(STATE->countrycode_pfxs); ++k)
    {
      if (kh_exist(STATE->countrycode_pfxs, k))
	{
          info = &kh_val(STATE->countrycode_pfxs, k);
          timeseries_kp_set(STATE->kp_v4, info->v4_idx, info->v4pfxs_cnt);

          // No IPv6 supported at this moment
	  // timeseries_kp_set(STATE->kp, info->v6_idx, info->v6pfxs_cnt);

	  // Reset counters
	  info->v4pfxs_cnt = 0;
	  // No IPv6 supported at this moment
          // info->v6pfxs_cnt = 0;
	}
    }
}


KHASH_INIT(country_k_set /* name */,
	   uint32_t  /* khkey_t */,
	   char /* khval_t */,
	   0  /* kh_is_set */,
	   kh_int_hash_func /*__hash_func */,
	   kh_int_hash_equal /* __hash_equal */);


static void geotag_v4table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{

  /* fprintf(stderr, "geotag_v4table START: \n"); */

  bgpstream_pfx_t *pfx;
  bgpstream_peer_id_t peerid;
  int fullfeed_cnt;

  pergeo_info_t *geo_info;
  ipmeta_record_t *rec;
  uint32_t num_ips;
  khiter_t k;
  int num_records;

  int khret;
  khash_t(country_k_set) *cck_set = NULL;
  khiter_t idk;
  uint32_t cck;
  khiter_t setk;

  for(bgpwatcher_view_iter_first_pfx(it, BGPSTREAM_ADDR_VERSION_IPV4, BGPWATCHER_VIEW_FIELD_ACTIVE);
      bgpwatcher_view_iter_has_more_pfx(it);
      bgpwatcher_view_iter_next_pfx(it))
    {

      // WARNING we do not geolocate ipv6 prefixes
      assert(pfx->address.version == BGPSTREAM_ADDR_VERSION_IPV4);


      /* get the current v4 prefix */
      pfx = bgpwatcher_view_iter_pfx_get_pfx(it);

      /* only consider pfxs with peers_cnt >= pfx_vis_threshold */
      if(bgpwatcher_view_iter_pfx_get_peer_cnt(it, BGPWATCHER_VIEW_FIELD_ACTIVE)
	 < BWC_GET_CHAIN_STATE(consumer)->pfx_vis_peers_threshold)
	{
	  continue;
	}
     
      /* only consider ipv4 prefixes whose mask is shorter than a /6 */
      if(pfx->mask_len < BWC_GET_CHAIN_STATE(consumer)->pfx_vis_mask_len_threshold)
        {
          continue;
        }

      fullfeed_cnt = 0;
      /* iterate over the peers for the current pfx */
      for(bgpwatcher_view_iter_pfx_first_peer(it, BGPWATCHER_VIEW_FIELD_ACTIVE);
          bgpwatcher_view_iter_pfx_has_more_peer(it);
          bgpwatcher_view_iter_pfx_next_peer(it))
        {
           /* only consider peers that are full-feed */
          peerid = bgpwatcher_view_iter_peer_get_peer_id(it);
          if(bgpstream_id_set_exists(BWC_GET_CHAIN_STATE(consumer)->v4ff_peerids,
                                     peerid) == 0)
            {
              continue;
            }
          // else increment the full feed count
          fullfeed_cnt++;
          if(fullfeed_cnt >=
             BWC_GET_CHAIN_STATE(consumer)->pfx_vis_peers_threshold)
            {
              // we don't need to know all the full feed peers
              // that contributed to the threshold
              break;
            }        
        }
      
      // if the prefix is ROUTED, then it can be geotagged
      if(fullfeed_cnt < BWC_GET_CHAIN_STATE(consumer)->pfx_vis_peers_threshold)
	{
          continue;
        }

      STATE->num_visible_pfx++;

      // First we check if this prefix has been already
      // geotagged in previous iterations

      cck_set = (khash_t(country_k_set) *) bgpwatcher_view_iter_pfx_get_user(it);

      // if the set is null, then we need to initialize the set
      // and proceed with the geolocation
      if(cck_set == NULL)
        {
          STATE->cache_misses_cnt++;

          // a new set has to be created, with the geolocation
          // information
          if( (cck_set = kh_init(country_k_set)) == NULL)
            {
              fprintf(stderr, "Error: cannot create country_k_set\n");
              return;
            }

          // then we link this set to the appropriate user ptr
          bgpwatcher_view_iter_pfx_set_user(it, (void *) cck_set);

          // geolocation
          ipmeta_lookup(STATE->provider, (uint32_t) ((bgpstream_ipv4_pfx_t *)pfx)->address.ipv4.s_addr,
                        pfx->mask_len, STATE->records);
          ipmeta_record_set_rewind(STATE->records);
          num_records = 0;
          while ( (rec = ipmeta_record_set_next(STATE->records, &num_ips)) )
            {
              num_records++;

              // check that we already had this country in our dataset
              if((k = kh_get(cc_pfxs, STATE->countrycode_pfxs,
                             rec->country_code)) ==
                 kh_end(STATE->countrycode_pfxs))
                {
                  fprintf(stderr, "Warning: country (%s) not found",
                          rec->country_code);
                }
              else
                {
                  // insert k in the country-code-k set
                  if((setk = kh_get(country_k_set, cck_set, k)) == kh_end(cck_set))
                    {
                      setk = kh_put(country_k_set, cck_set, k, &khret);                              
                    }                  
                }
            }

          if(num_records > STATE->max_records_perpfx)
            {
              STATE->max_records_perpfx = num_records;
            }
        }
      else
        {
          STATE->cache_hits_cnt++;
        }


      // Whether the cck_set already existed, or it has just been created,
      // we update the geo counters

      // geolocation already performed, then proceed and increment
      // the counters for each country
      // cc_k_set contains the k position of a country in the
      // countrycode_pfxs hash map

      for(idk = kh_begin(cck_set);
          idk != kh_end(cck_set); ++idk)
        {
          if (kh_exist(cck_set, idk))
            {
              cck = kh_key(cck_set, idk);
              geo_info = &kh_value(STATE->countrycode_pfxs, cck);
              geo_info->v4pfxs_cnt++;
              STATE->avg_numcountries_perpfx++;
            }
        }

      if(kh_size(cck_set) > STATE->max_numcountries_perpfx)
        {
          STATE->max_numcountries_perpfx = kh_size(cck_set);
        }

    } /* end per-pfx loop */
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

  // change it! (str -> set(pfxs))
  if((state->countrycode_pfxs = kh_init(cc_pfxs)) == NULL)
    {
      fprintf(stderr, "Error: Unable to create cc visibility map\n");
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

  /* initialize ipmeta and provider */
  if(init_ipmeta(consumer) != 0)
    {
      goto err;
    }

  /* create a timeseries metric for each country */
  if(create_per_cc_metrics(consumer) != 0)
    {
      goto err;
    }

  /* create the top-level general metrics, and meta metrics */
  if(create_gen_metrics(consumer) != 0)
    {
      goto err;
    }

  return 0;

 err:
  bwc_pergeovisibility_destroy(consumer);
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

  timeseries_kp_free(&state->kp_gen);
  timeseries_kp_free(&state->kp_v4);

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
  bgpwatcher_view_iter_t *it;

  if(BWC_GET_CHAIN_STATE(consumer)->visibility_computed == 0)
    {
      fprintf(stderr,
              "ERROR: The Per-AS Visibility requires the Visibility consumer "
              "to be run first\n");
      return -1;
    }

  // compute arrival delay
  STATE->arrival_delay = zclock_time()/1000- bgpwatcher_view_get_time(view);

  /* create a new iterator */
  if((it = bgpwatcher_view_iter_create(view)) == NULL)
    {
      return -1;
    }

  if(BWC_GET_CHAIN_STATE(consumer)->v4_usable != 0)
    {
      /* analyze v4 table */
      geotag_v4table(consumer, it);

      dump_v4table(consumer);

      /* now flush the kp */
      if(timeseries_kp_flush(STATE->kp_v4, bgpwatcher_view_get_time(view)) != 0)
        {
          return -1;
        }
    }

  // IPV6: also if(bgpstream_id_set_size(kh_STATE->v6ff_peerids) > ROUTED_PFX_PEERCNT)

  // compute processed delay (must come prior to dump_gen_metrics)
  STATE->processed_delay = zclock_time()/1000- bgpwatcher_view_get_time(view);
  /* dump metrics and tables */
  dump_gen_metrics(consumer);

  /* now flush the kp */
  if(timeseries_kp_flush(STATE->kp_gen, bgpwatcher_view_get_time(view)) != 0)
    {
      return -1;
    }

  return 0;
}
