/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2015 The Regents of the University of California.
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
#include "czmq.h"

#include "bl_pfx_set.h"
#include "bl_id_set.h"
#include "bl_id_set_int.h"

#include "bgpwatcher_consumer_interface.h"

#include "bwc_pergeovisibility.h"

#define BUFFER_LEN 1024

#define NAME "per-geo-visibility"

#define METRIC_PREFIX               "bgp.visibility.geo.netacuity"

#define METRIC_CC_V4PFX_FORMAT      METRIC_PREFIX".%s.%s.ipv4_pfx_cnt"
#define METRIC_CC_V6PFX_FORMAT      METRIC_PREFIX".%s.%s.ipv6_pfx_cnt"

#define META_METRIC_PREFIX           "bgp.meta.bgpwatcher.consumer.geo"
#define METRIC_CACHE_MISS_CNT        META_METRIC_PREFIX".cache_miss_cnt"
#define METRIC_CACHE_HITS_CNT        META_METRIC_PREFIX".cache_hit_cnt"
#define METRIC_ARRIVAL_DELAY         META_METRIC_PREFIX".arrival_delay"
#define METRIC_PROCESSED_DELAY       META_METRIC_PREFIX".processed_delay"
#define METRIC_MAXCOUNTRIES_PERPFX   META_METRIC_PREFIX".max_numcountries_perpfx"
#define METRIC_AVGCOUNTRIES_PERPFX   META_METRIC_PREFIX".avg_numcountries_perpfx"
#define METRIC_VISIBLE_PFXS          META_METRIC_PREFIX".visible_pfxs_cnt"
#define METRIC_MAXRECS_PERPFXS       META_METRIC_PREFIX".max_records_perpfx"

#define ROUTED_PFX_PEERCNT 10
#define IPV4_FULLFEED_SIZE 400000
#define IPV6_FULLFEED_SIZE 10000

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

  /** Set of v4 full-feed peers */
  bl_id_set_t *v4ff_peerids;

  /** Set of v6 full-feed peers */
  bl_id_set_t *v6ff_peerids;

  int v4_peer_cnt;
  int v6_peer_cnt;

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
  
  /** Prefix visibility threshold */
  int pfx_vis_threshold;

  /** netacq-edge files */
  char blocks_file[BUFFER_LEN];
  char locations_file[BUFFER_LEN];
  char countries_file[BUFFER_LEN];

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
	  "       -c <file>     country decode file (mandatory option)\n"
	  "       -b <file>     blocks file (mandatory option)\n"
	  "       -l <file>     locations file (mandatory option)\n"
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
  while((opt = getopt(argc, argv, ":p:b:c:l:?")) >= 0)
    {
      switch(opt)
	{
	case 'p':
	  STATE->pfx_vis_threshold = atoi(optarg);
	  break;
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


static int create_gen_metrics(bwc_t *consumer)
{
  if((STATE->gen_metrics.cache_misses_cnt_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_CACHE_MISS_CNT)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.cache_hits_cnt_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_CACHE_HITS_CNT)) == -1)
    {
      return -1;
    }
    
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

  if((STATE->gen_metrics.max_numcountries_perpfx_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_MAXCOUNTRIES_PERPFX)) == -1)
    {
      return -1;
    }

  if((STATE->gen_metrics.avg_numcountries_perpfx_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_AVGCOUNTRIES_PERPFX)) == -1)
    {
      return -1;
    }

    if((STATE->gen_metrics.num_visible_pfx_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_VISIBLE_PFXS)) == -1)
    {
      return -1;
    }

    if((STATE->gen_metrics.max_records_perpfx_idx =
      timeseries_kp_add_key(STATE->kp, METRIC_MAXRECS_PERPFXS)) == -1)
    {
      return -1;
    }

    return 0;
}


static void find_ff_peers(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{
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
          /* add to the v4 fullfeed set */
	  bl_id_set_insert(STATE->v4ff_peerids, peerid);
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
	  bl_id_set_insert(STATE->v6ff_peerids, peerid);
        }
    } 
}


static void dump_gen_metrics(bwc_t *consumer)
{

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.cache_misses_cnt_idx,
                    STATE->cache_misses_cnt);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.cache_hits_cnt_idx,
                    STATE->cache_hits_cnt);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.arrival_delay_idx,
                    STATE->arrival_delay);
  
  timeseries_kp_set(STATE->kp, STATE->gen_metrics.processed_delay_idx,
                    STATE->processed_delay);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.max_numcountries_perpfx_idx,
                    STATE->max_numcountries_perpfx);

  if(STATE->num_visible_pfx > 0)
    {
      STATE->avg_numcountries_perpfx = STATE->avg_numcountries_perpfx / (double) STATE->num_visible_pfx;
    }
    
  timeseries_kp_set(STATE->kp, STATE->gen_metrics.avg_numcountries_perpfx_idx,
                    STATE->avg_numcountries_perpfx);

  timeseries_kp_set(STATE->kp, STATE->gen_metrics.num_visible_pfx_idx,
                    STATE->num_visible_pfx);
  
  timeseries_kp_set(STATE->kp, STATE->gen_metrics.max_records_perpfx_idx,
                    STATE->max_records_perpfx);

  // reset counters
  bl_id_set_reset(STATE->v4ff_peerids);
  bl_id_set_reset(STATE->v6ff_peerids);

  STATE->v4_peer_cnt = 0;
  STATE->v6_peer_cnt = 0;
  
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
  
  for (k = kh_begin(STATE->countrycode_pfxs); k != kh_end(STATE->countrycode_pfxs); ++k)
    {
      if (kh_exist(STATE->countrycode_pfxs, k))
	{
          info = &kh_val(STATE->countrycode_pfxs, k);
          timeseries_kp_set(STATE->kp, info->v4_idx, info->v4pfxs_cnt);

          // No IPv6 supported at this moment
	  // timeseries_kp_set(STATE->kp, info->v6_idx, info->v6pfxs_cnt);

	  // Reset counters
	  info->v4pfxs_cnt = 0;
	  // No IPv6 supported at this moment
          // info->v6pfxs_cnt = 0;
	}
    }
}



static void geotag_v4table(bwc_t *consumer, bgpwatcher_view_iter_t *it)
{

  /* fprintf(stderr, "geotag_v4table START: \n"); */

  bwc_pergeovisibility_state_t *state = STATE;

  bl_ipv4_pfx_t *v4pfx;
  bl_peerid_t peerid;
  int fullfeed_cnt;
  bl_id_set_t *cck_set;
  
  pergeo_info_t *geo_info;
  ipmeta_record_t *rec;
  uint32_t num_ips;
  khiter_t k;
  int num_records;

  khiter_t idk;
  uint32_t cck;

  for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX);
      !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX);
      bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX))
    {      
      
      /* get the current v4 prefix */
      v4pfx = bgpwatcher_view_iter_get_v4pfx(it);      

      /* we do not process prefixes whose mask is shorted than
	 a /6 */
      if(v4pfx->mask_len < 6)
      	{
      	  continue;
      	}

      
      /* exclude prefixes that are not seen by at least threshold peers
       * no matter if they are full feed or not */
      if(bgpwatcher_view_iter_size(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER)
	 < state->pfx_vis_threshold)
	{
	  continue;
	}      

      fullfeed_cnt = 0;
      /* iterate over the peers for the current v4pfx */
      for(bgpwatcher_view_iter_first(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER);
	  !bgpwatcher_view_iter_is_end(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER);
	  bgpwatcher_view_iter_next(it, BGPWATCHER_VIEW_ITER_FIELD_V4PFX_PEER))
	{
          /* only consider peers that are full-feed */
          peerid = bgpwatcher_view_iter_get_v4pfx_peerid(it);
	  if(bl_id_set_exists(state->v4ff_peerids, peerid) == 0)
            {
	      continue;
	    }
	  // else increment the full feed count
	  fullfeed_cnt++;
	  if(fullfeed_cnt >= state->pfx_vis_threshold)
	    { // we don't need to know all the full feed peers
	      // that contributed to the threshold
	      break;
	    }      
	}      

      // if the prefix is ROUTED, then it can be geotagged
      if(fullfeed_cnt >= state->pfx_vis_threshold)
	{
	  state->num_visible_pfx++;

	  // First we check if this prefix has been already
	  // geotagged in previous iterations

	  cck_set = (bl_id_set_t *) bgpwatcher_view_iter_get_v4pfx_user(it);

	  // if the set is null, then we need to initialize the set
	  // and proceed with the geolocation
	  if(cck_set == NULL)
	    {

	      state->cache_misses_cnt++;
	      
	      // a new set has to be created, with the geolocation
	      // information
	      if( (cck_set = bl_id_set_create()) == NULL)
		{
		  fprintf(stderr, "Error: cannot bl_id_set_create()\n");
		  return;
		}

	      // then we link this set to the appropriate user ptr
	      bgpwatcher_view_iter_set_v4pfx_user(it, (void *) cck_set);

	      // geolocation
	      ipmeta_lookup(state->provider, (uint32_t) v4pfx->address.ipv4.s_addr,
			    v4pfx->mask_len, state->records);
	      ipmeta_record_set_rewind(state->records);
	      num_records = 0;
	      while ( (rec = ipmeta_record_set_next(state->records, &num_ips)) )	  
		{
		  num_records++;
		  
		  // check that we already had this country in our dataset
		  if((k = kh_get(cc_pfxs, state->countrycode_pfxs, rec->country_code)) == kh_end(state->countrycode_pfxs))
		    {
		      fprintf(stderr, "Warning: country (%s) not found", rec->country_code);
		    }
		  else
		    {
		      bl_id_set_insert(cck_set, k);		      
		    }
		}

	      if(num_records > state->max_records_perpfx)
		{
		  state->max_records_perpfx = num_records;
		}
	    }
	  else
	    {
	      state->cache_hits_cnt++;
	    }

	  
	  // Whether the cck_set already existed, or it has just been created,
	  // we update the geo counters  

	  // geolocation already performed, then proceed and increment
	  // the counters for each country
	  // cc_k_set contains the k position of a country in the
	  // countrycode_pfxs hash map
	  
	  for(idk = kh_begin(cck_set->hash);
	      idk != kh_end(cck_set->hash); ++idk)
	    {
	      if (kh_exist(cck_set->hash, idk))
		{
		  cck = kh_key(cck_set->hash, idk);
		  geo_info = &kh_value(state->countrycode_pfxs, cck);
		  geo_info->v4pfxs_cnt++;
		  state->avg_numcountries_perpfx++;
		}
	    }	     

	  if(bl_id_set_size(cck_set) > state->max_numcountries_perpfx)
	    {
	      state->max_numcountries_perpfx = bl_id_set_size(cck_set);
	    }
	  
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

  // change it! (str -> set(pfxs))
  if((state->countrycode_pfxs = kh_init(cc_pfxs)) == NULL)
    {
      fprintf(stderr, "Error: Unable to create cc visibility map\n");
      goto err;
    }
  
  if((state->v4ff_peerids = bl_id_set_create()) == NULL)
    {
      fprintf(stderr, "Error: unable to create full-feed peers (v4)\n");
      goto err;
    }
  if((state->v6ff_peerids = bl_id_set_create()) == NULL)
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

  /* initialize blocks,countries, locations files
   * these files 
   */
  state->blocks_file[0] = '\0';
  state->countries_file[0] = '\0';
  state->locations_file[0] = '\0';
  
  /* parse the command line args */
  if(parse_args(consumer, argc, argv) != 0)
    {
      goto err;
    }
  
  /* lookup the provider using the name  */
  if((state->provider = ipmeta_get_provider_by_name(state->ipmeta, GEO_PROVIDER_NAME)) == NULL)
    {
      fprintf(stderr, "ERROR: Invalid provider name: %s\n", GEO_PROVIDER_NAME);
      goto err;
    }

  /* enable the provider  */
  char provider_options[BUFFER_LEN];
  provider_options[0] = '\0';

  snprintf(provider_options, BUFFER_LEN, "-b %s -l %s -c %s -D intervaltree",
	   state->blocks_file,
	   state->locations_file,
	   state->countries_file);	
  
  if(ipmeta_enable_provider(state->ipmeta,
			    state->provider,
			    provider_options,
			    IPMETA_PROVIDER_DEFAULT_YES) != 0)
    {
      fprintf(stderr, "ERROR: Could not enable provider %s\n", GEO_PROVIDER_NAME);
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
  char buffer[BUFFER_LEN];

  for(i=0; i < num_countries; i++)
    {
      // Warning: we assume netacq returns a set of unique countries
      // then we don't need to check if these iso2 are already
      // present in the countrycode map
      k = kh_put(cc_pfxs, state->countrycode_pfxs, 
		 strdup(countries[i]->iso2), &khret);
      
      geo_info = &kh_value(state->countrycode_pfxs, k);

      // initialize properly geo_info and create ipvX metrics id for kp
      geo_info->v4pfxs_cnt = 0;
      
      snprintf(buffer, BUFFER_LEN, METRIC_CC_V4PFX_FORMAT, countries[i]->continent, countries[i]->iso2);
      if((geo_info->v4_idx = timeseries_kp_add_key(STATE->kp, buffer)) == -1)
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
  if(state->v4ff_peerids != NULL)
    {
      bl_id_set_destroy( state->v4ff_peerids);
      state->v4ff_peerids = NULL;
    }
  if(state->v6ff_peerids != NULL)
    {
      bl_id_set_destroy(state->v6ff_peerids);
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

  // compute arrival delay
  state->arrival_delay = zclock_time()/1000- bgpwatcher_view_time(view);
  
  /* find the full-feed peers */
  find_ff_peers(consumer, it);

  if(bl_id_set_size(state->v4ff_peerids) < ROUTED_PFX_PEERCNT)
    {
      return 0;
    }

  // IPV6: also if(bl_id_set_size(kh_state->v6ff_peerids) > ROUTED_PFX_PEERCNT)
  
  /* analyze v4 table */
  geotag_v4table(consumer, it);

  // compute processed delay
  state->processed_delay = zclock_time()/1000- bgpwatcher_view_time(view);

  /* dump metrics and tables */
  dump_gen_metrics(consumer);
  dump_v4table(consumer);

  /* now flush the kp */
  if(timeseries_kp_flush(STATE->kp, bgpwatcher_view_time(view)) != 0)
    {
      return -1;
    }

  return 0;
}
