/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "bgpcorsaro_int.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "bgpstream_lib.h"
#include "bgpdump_util.h"

#include "utils.h"
#include "wandio_utils.h"

#include "bgpcorsaro_io.h"
#include "bgpcorsaro_log.h"
#include "bgpcorsaro_plugin.h"

#include "khash.h"
#include "bgpcorsaro_filtervis.h"

#include "bl_bgp_utils.h"
#include "bl_peersign_map.h"
#include "bl_pfx_set.h"
#include "bl_pfx_set_int.h"
#include "bl_id_set.h"


#define FV_IPV4_FULLFEED_SIZE 400000
#define FV_IPV6_FULLFEED_SIZE 10000



/** @file
 *
 * @brief Bgpcorsaro FilterVis plugin implementation
 *
 * @author Chiara Orsini
 *
 */

/** The number of output file pointers to support non-blocking close at the end
    of an interval. If the wandio buffers are large enough that it takes more
    than 1 interval to drain the buffers, consider increasing this number */
#define OUTFILE_POINTERS 2

/** The name of this plugin */
#define PLUGIN_NAME "filtervis"

/** The version of this plugin */
#define PLUGIN_VERSION "0.1"

/** Common plugin information across all instances */
static bgpcorsaro_plugin_t bgpcorsaro_filtervis_plugin = {
  PLUGIN_NAME,                                          /* name */
  PLUGIN_VERSION,                                       /* version */
  BGPCORSARO_PLUGIN_ID_FILTERVIS,                       /* id */
  BGPCORSARO_PLUGIN_GENERATE_PTRS(bgpcorsaro_filtervis), /* func ptrs */
  BGPCORSARO_PLUGIN_GENERATE_TAIL,
};



/** Filter visibility core functions and structures */

KHASH_INIT(peer_ipv4prefix_map /* name */, 
	   uint16_t /* khkey_t */, 
	   bl_ipv4_pfx_set_t * /* khval_t */, 
	   1  /* kh_is_map */, 
	   kh_int_hash_func /*__hash_func */,  
	   kh_int_hash_equal /* __hash_equal */);

typedef khash_t(peer_ipv4prefix_map) peer_ipv4prefix_map_t;

KHASH_INIT(peer_ipv6prefix_map /* name */, 
	   uint16_t /* khkey_t */, 
	   bl_ipv6_pfx_set_t * /* khval_t */, 
	   1  /* kh_is_map */, 
	   kh_int_hash_func /*__hash_func */,  
	   kh_int_hash_equal /* __hash_equal */);

typedef khash_t(peer_ipv6prefix_map) peer_ipv6prefix_map_t;

typedef struct struct_peer_breakdown_t {
  uint16_t full_feed_peers_cnt;
  uint16_t all_peers_cnt;
} peer_breakdown_t;


KHASH_INIT(ipv4prefix_peer_map /* name */, 
	   bl_ipv4_pfx_t /* khkey_t */, 
	   peer_breakdown_t /* khval_t */, 
	   1  /* kh_is_map */, 
	   bl_ipv4_pfx_hash_func /*__hash_func */,  
	   bl_ipv4_pfx_hash_equal /* __hash_equal */);

typedef khash_t(ipv4prefix_peer_map) ipv4prefix_peer_map_t;

KHASH_INIT(ipv6prefix_peer_map /* name */, 
	   bl_ipv6_pfx_t /* khkey_t */, 
	   peer_breakdown_t /* khval_t */, 
	   1  /* kh_is_map */, 
	   bl_ipv6_pfx_hash_func /*__hash_func */,  
	   bl_ipv6_pfx_hash_equal /* __hash_equal */);

typedef khash_t(ipv6prefix_peer_map) ipv6prefix_peer_map_t;



typedef struct struct_filter_vis_t {
  int start_time;
  int end_time;
  bl_peersign_map_t *ps_map;
  peer_ipv4prefix_map_t *ipv4_vis;
  peer_ipv6prefix_map_t *ipv6_vis;
  // thresholds
  int min_ipv4_mask_len;
  int max_ipv4_mask_len;
  int max_ipv6_mask_len ;
  int ipv4_full_feed_th;
  int ipv6_full_feed_th;
  // show flags
  uint8_t show_ipv4;
  uint8_t show_ipv6;
  // incremental flag
  uint8_t incremental;
} filter_vis_t;

static filter_vis_t *filter_vis_create(){
  filter_vis_t *fv = NULL;
  fv = (filter_vis_t*)malloc_zero(sizeof(filter_vis_t));
  if(fv != NULL)
    {
      fv->start_time = 0;
      fv->end_time = 0;
      fv->ps_map = bl_peersign_map_create();
      assert(fv->ps_map != NULL);
      fv->ipv4_vis = kh_init(peer_ipv4prefix_map);
      assert(fv->ipv4_vis != NULL);
      fv->ipv6_vis = kh_init(peer_ipv6prefix_map);
      assert(fv->ipv6_vis != NULL);
      // thresholds
      fv->min_ipv4_mask_len = 7;
      fv->max_ipv4_mask_len = 24;
      fv->max_ipv6_mask_len = 64;
      fv->ipv4_full_feed_th = FV_IPV4_FULLFEED_SIZE;
      fv->ipv6_full_feed_th = FV_IPV6_FULLFEED_SIZE;
      // show flags
      fv->show_ipv4 = 0;
      fv->show_ipv6 = 0;
      // incremental flag
      fv->incremental = 0;
    }
  return fv;
}


static void filter_vis_start(filter_vis_t *fv, int start_time)
{
  fv->start_time = start_time;
}

static void insert_into_ipv4(peer_ipv4prefix_map_t *vis_map, uint16_t peer_id, bl_ipv4_pfx_t *prefix)
{
  int khret;
  khiter_t k;
  bl_ipv4_pfx_set_t *set;
  if((k = kh_get(peer_ipv4prefix_map, vis_map, peer_id)) == kh_end(vis_map))
    {
      k = kh_put(peer_ipv4prefix_map, vis_map, peer_id, &khret);
      kh_value(vis_map,k) = bl_ipv4_pfx_set_create();      
    }
  set = kh_value(vis_map,k);
  assert(set != NULL);
  bl_ipv4_pfx_set_insert(set,*prefix);  
}

static void insert_into_ipv6(peer_ipv6prefix_map_t *vis_map, uint16_t peer_id, bl_ipv6_pfx_t *prefix)
{
  int khret;
  khiter_t k;
  bl_ipv6_pfx_set_t *set;
  if((k = kh_get(peer_ipv6prefix_map, vis_map, peer_id)) == kh_end(vis_map))
    {
      k = kh_put(peer_ipv6prefix_map, vis_map, peer_id, &khret);
      kh_value(vis_map,k) = bl_ipv6_pfx_set_create();      
    }
  set = kh_value(vis_map,k);
  assert(set != NULL);
  bl_ipv6_pfx_set_insert(set,*prefix);  
}

static int filter_vis_update(filter_vis_t *fv, bgpstream_record_t *bs_record)
{
  
  bl_elem_t *bs_elem_queue;
  bl_elem_t *bs_iterator;

  uint16_t peer_id;
  bl_ipv4_pfx_t *ipv4_prefix;
  bl_ipv6_pfx_t *ipv6_prefix;
	  
  if(bs_record->status == VALID_RECORD) 
    {
      bs_elem_queue = bgpstream_get_elem_queue(bs_record);
      bs_iterator = bs_elem_queue;
      while(bs_iterator != NULL)
	{
	  if(bs_iterator->type == BL_ANNOUNCEMENT_ELEM || bs_iterator->type == BL_RIB_ELEM)
	    {
	      // getting peer_id	      
	      peer_id = bl_peersign_map_set_and_get(fv->ps_map, bs_record->attributes.dump_collector,
						    &(bs_iterator->peer_address));

	      if(bs_iterator->prefix.address.version == BL_ADDR_IPV4 && fv->show_ipv4 == 1)
		{
		  if(bs_iterator->prefix.mask_len < fv->min_ipv4_mask_len ||
		     bs_iterator->prefix.mask_len > fv->max_ipv4_mask_len)
		    {
		      bs_iterator = bs_iterator->next;
		      continue;
		    }

		  ipv4_prefix = bl_pfx_storage2ipv4(&(bs_iterator->prefix));
		  insert_into_ipv4(fv->ipv4_vis, peer_id, ipv4_prefix);
		}
	      
	      if(bs_iterator->prefix.address.version == BL_ADDR_IPV6 && fv->show_ipv6 == 1)
		{
		  if(bs_iterator->prefix.mask_len > fv->max_ipv6_mask_len)
		    {
		      bs_iterator = bs_iterator->next;
		      continue;
		    }

		  ipv6_prefix = bl_pfx_storage2ipv6(&(bs_iterator->prefix));
		  insert_into_ipv6(fv->ipv6_vis, peer_id, ipv6_prefix);
		}	     
	    }
	  bs_iterator = bs_iterator->next;
	}
      bgpstream_destroy_elem_queue( bs_elem_queue);	    
    }
  return 0;  
}


static void insert_ipv4_pfxpeer_pair(ipv4prefix_peer_map_t *ipv4_pfx_visinfo, bl_ipv4_pfx_t *ipv4_pfx,
			      uint16_t peer_id, bl_id_set_t *ipv4_full_feed)
{
  khiter_t k;
  int khret;  
  peer_breakdown_t pbd;
  if((k = kh_get(ipv4prefix_peer_map, ipv4_pfx_visinfo, *ipv4_pfx)) == kh_end(ipv4_pfx_visinfo))
    {
      k = kh_put(ipv4prefix_peer_map, ipv4_pfx_visinfo, *ipv4_pfx, &khret);
      pbd.full_feed_peers_cnt = 0;
      pbd.all_peers_cnt = 0;
      kh_value(ipv4_pfx_visinfo,k) = pbd;      
    }
  
  pbd = kh_value(ipv4_pfx_visinfo,k);
  // all peers cnt
  pbd.all_peers_cnt++;
  if(bl_id_set_exists(ipv4_full_feed, peer_id) == 1 )
    {
      // full feed peers count
      pbd.full_feed_peers_cnt++;
    }
  kh_value(ipv4_pfx_visinfo,k) = pbd;      
}


static void insert_ipv6_pfxpeer_pair(ipv6prefix_peer_map_t *ipv6_pfx_visinfo, bl_ipv6_pfx_t *ipv6_pfx,
			      uint16_t peer_id, bl_id_set_t *ipv6_full_feed)
{
  khiter_t k;
  int khret;
  peer_breakdown_t pbd;
  if((k = kh_get(ipv6prefix_peer_map, ipv6_pfx_visinfo, *ipv6_pfx)) == kh_end(ipv6_pfx_visinfo))
    {
      k = kh_put(ipv6prefix_peer_map, ipv6_pfx_visinfo, *ipv6_pfx, &khret);
      pbd.full_feed_peers_cnt = 0;
      pbd.all_peers_cnt = 0;
      kh_value(ipv6_pfx_visinfo,k) = pbd;      
    }
  
  pbd = kh_value(ipv6_pfx_visinfo,k);
  // all peers cnt
  pbd.all_peers_cnt++;
  if(bl_id_set_exists(ipv6_full_feed, peer_id) == 1 )
    {
      // full feed peers count
      pbd.full_feed_peers_cnt++;
    }
  kh_value(ipv6_pfx_visinfo,k) = pbd;      
}

static void filter_vis_end(filter_vis_t *fv, int end_time)
{

  fv->end_time = end_time;

  int khret;
  khiter_t k;
  uint16_t peer_id;
  
  bl_id_set_t *ipv4_full_feed = bl_id_set_create();
  assert(ipv4_full_feed);

  bl_id_set_t *ipv6_full_feed = bl_id_set_create();
  assert(ipv6_full_feed);
  
  // Step 1: get full feed
  
  bl_ipv4_pfx_set_t *ipv4_set;
  bl_ipv6_pfx_set_t *ipv6_set;
  if(fv->show_ipv4 == 1)
    {

      for (k = kh_begin(fv->ipv4_vis); k != kh_end(fv->ipv4_vis); ++k)
	{
	  if (kh_exist(fv->ipv4_vis, k))
	    {
	      peer_id = kh_key(fv->ipv4_vis, k);
	      ipv4_set = kh_value(fv->ipv4_vis, k);
	      if(bl_ipv4_pfx_set_size(ipv4_set) > fv->ipv4_full_feed_th)
		{
		  bl_id_set_insert(ipv4_full_feed, peer_id);
		}
	    }
	}
    }

  if(fv->show_ipv6 == 1)
    {
      for (k = kh_begin(fv->ipv6_vis); k != kh_end(fv->ipv6_vis); ++k)
	{
	  if (kh_exist(fv->ipv6_vis, k))
	    {
	      peer_id = kh_key(fv->ipv6_vis, k);
	      ipv6_set = kh_value(fv->ipv6_vis, k);
	      if(bl_ipv6_pfx_set_size(ipv6_set) > fv->ipv6_full_feed_th)
		{
		  bl_id_set_insert(ipv6_full_feed, peer_id);
		}
	    }
	}
    }
  
  // Step 2: for each prefix count the number of peers (total and full)
  ipv4prefix_peer_map_t *ipv4_pfx_visinfo = kh_init(ipv4prefix_peer_map);
  ipv6prefix_peer_map_t *ipv6_pfx_visinfo = kh_init(ipv6prefix_peer_map);
  bl_ipv4_pfx_t *ipv4_pfx;
  bl_ipv6_pfx_t *ipv6_pfx;
  khiter_t inn_k;
  // fill ipv4 structure
  if(fv->show_ipv4 == 1)
    {
      for (k = kh_begin(fv->ipv4_vis); k != kh_end(fv->ipv4_vis); ++k)
	{
	  if (kh_exist(fv->ipv4_vis, k))
	    {
	      peer_id = kh_key(fv->ipv4_vis, k);
	      ipv4_set = kh_value(fv->ipv4_vis, k);
	      for (inn_k = kh_begin(ipv4_set->hash); inn_k != kh_end(ipv4_set->hash); ++inn_k)
		{
		  if (kh_exist(ipv4_set->hash, inn_k))
		    {
		      ipv4_pfx = &kh_key(ipv4_set->hash, inn_k);
		      insert_ipv4_pfxpeer_pair(ipv4_pfx_visinfo, ipv4_pfx, peer_id, ipv4_full_feed);
		    }
		}
	    }
	}
    }
  // fill ipv6 structure
  if(fv->show_ipv6 == 1)
    {
      for (k = kh_begin(fv->ipv6_vis); k != kh_end(fv->ipv6_vis); ++k)
	{
	  if (kh_exist(fv->ipv6_vis, k))
	    {
	      peer_id = kh_key(fv->ipv6_vis, k);
	      ipv6_set = kh_value(fv->ipv6_vis, k);
	      for (inn_k = kh_begin(ipv6_set->hash); inn_k != kh_end(ipv6_set->hash); ++inn_k)
		{
		  if (kh_exist(ipv6_set->hash, inn_k))
		    {
		      ipv6_pfx = &kh_key(ipv6_set->hash, inn_k);
		      insert_ipv6_pfxpeer_pair(ipv6_pfx_visinfo, ipv6_pfx, peer_id, ipv6_full_feed);
		    }
		}
	    }
	}
    }

  // Step 3: print results

  peer_breakdown_t pbd;
  char *ip_str;
  
  if(fv->show_ipv4 == 1)
    {
      // printing ipv4 prefixes (all)
      for (k = kh_begin(ipv4_pfx_visinfo); k != kh_end(ipv4_pfx_visinfo); ++k)
	{
	  if (kh_exist(ipv4_pfx_visinfo, k))
	    {
	      ipv4_pfx = &kh_key(ipv4_pfx_visinfo, k);
	      pbd = kh_value(ipv4_pfx_visinfo, k);
	      ip_str = bl_print_ipv4_addr(&ipv4_pfx->address);
	      printf("%d\t%s/%d\t%u\t%u\n", fv->start_time, ip_str,
		     ipv4_pfx->mask_len, pbd.full_feed_peers_cnt, pbd.all_peers_cnt);
	      free(ip_str);
	    }
	}
    }

  if(fv->show_ipv6 == 1)
    {
      // printing ipv6 prefixes (all)
      for (k = kh_begin(ipv6_pfx_visinfo); k != kh_end(ipv6_pfx_visinfo); ++k)
	{
	  if (kh_exist(ipv6_pfx_visinfo, k))
	    {
	      ipv6_pfx = &kh_key(ipv6_pfx_visinfo, k);
	      pbd = kh_value(ipv6_pfx_visinfo, k);
	      ip_str = bl_print_ipv6_addr(&ipv6_pfx->address);
	      printf("%d\t%s/%d\t%u\t%u\n", fv->start_time, ip_str,
		     ipv6_pfx->mask_len, pbd.full_feed_peers_cnt, pbd.all_peers_cnt);
	      free(ip_str);
	    }
	}
    }

  // destroy tmp structures
  bl_id_set_destroy(ipv4_full_feed);
  bl_id_set_destroy(ipv6_full_feed);
  kh_destroy(ipv4prefix_peer_map, ipv4_pfx_visinfo);
  kh_destroy(ipv6prefix_peer_map, ipv6_pfx_visinfo);
  
  // reset persistent structure
  if(fv->incremental == 0)
    {
      for (k = kh_begin(fv->ipv4_vis); k != kh_end(fv->ipv4_vis); ++k)
	{
	  if (kh_exist(fv->ipv4_vis, k))
	    {
	      ipv4_set = kh_value(fv->ipv4_vis, k);
	      bl_ipv4_pfx_set_destroy(ipv4_set);
	    }
	}
      kh_clear(peer_ipv4prefix_map, fv->ipv4_vis);

      for (k = kh_begin(fv->ipv6_vis); k != kh_end(fv->ipv6_vis); ++k)
	{
	  if (kh_exist(fv->ipv6_vis, k))
	    {
	      ipv6_set = kh_value(fv->ipv6_vis, k);
	      bl_ipv6_pfx_set_destroy(ipv6_set);
	    }
	}
      kh_clear(peer_ipv6prefix_map, fv->ipv6_vis);
    }
}

static void filter_vis_destroy(filter_vis_t *fv)
{
  int khret;
  khiter_t k;
  bl_ipv4_pfx_set_t *ipv4_set;
  bl_ipv6_pfx_set_t *ipv6_set;

  if(fv != NULL)
    {
      if(fv->ps_map != NULL)
	{
	  bl_peersign_map_destroy(fv->ps_map);
	  fv->ps_map = NULL;
	}
      
      if(fv->ipv4_vis != NULL)
	{
	  for (k = kh_begin(fv->ipv4_vis); k != kh_end(fv->ipv4_vis); ++k)
	    {
	      if (kh_exist(fv->ipv4_vis, k))
		{
		  ipv4_set = kh_value(fv->ipv4_vis, k);
		  bl_ipv4_pfx_set_destroy(ipv4_set);
		}
	    }
	  kh_destroy(peer_ipv4prefix_map, fv->ipv4_vis);
	}
      fv->ipv4_vis = NULL;
      
      if(fv->ipv6_vis != NULL)
	{
	  for (k = kh_begin(fv->ipv6_vis); k != kh_end(fv->ipv6_vis); ++k)
	    {
	      if (kh_exist(fv->ipv6_vis, k))
		{
		  ipv6_set = kh_value(fv->ipv6_vis, k);
		  bl_ipv6_pfx_set_destroy(ipv6_set);
		}
	    }
	  kh_destroy(peer_ipv6prefix_map, fv->ipv6_vis);
	}
      fv->ipv6_vis = NULL;

      // finally free fv
      free(fv);  
    }
  
}

/** ************************************************** */


/** Holds the state for an instance of this plugin */
struct bgpcorsaro_filtervis_state_t {

  /** The outfile for the plugin */
  iow_t *outfile;
  /** A set of pointers to outfiles to support non-blocking close */
  iow_t *outfile_p[OUTFILE_POINTERS];
  /** The current outfile */
  int outfile_n;
  
  filter_vis_t *filter_vis;  /// plugin-related structure  
};


/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, filtervis, BGPCORSARO_PLUGIN_ID_FILTERVIS))

/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_FILTERVIS))


/** Print usage information to stderr */
static void usage(bgpcorsaro_plugin_t *plugin)
{
  fprintf(stderr,
	  "plugin usage: %s [-46i] [-f ipv4_ff_size] [-F ipv6_ff_size]\n"
	  "       -4         print ipv4 prefixes visibility (default: both version on)\n"
	  "       -6         print ipv6 prefixes visibility (default: both version on)\n"
	  "       -f <num>   set the full feed threshold for ipv4 peers (default: %d)\n"
	  "       -F <num>   set the full feed threshold for ipv6 peers (default: %d)\n"
	  "       -i         incremental output (default: off)\n",
	  plugin->argv[0], FV_IPV4_FULLFEED_SIZE, FV_IPV6_FULLFEED_SIZE);
}

/** Parse the arguments given to the plugin */
static int parse_args(bgpcorsaro_t *bgpcorsaro)
{
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_filtervis_state_t *state = STATE(bgpcorsaro);
  int opt;

  if(plugin->argc <= 0)
    {
      return 0;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, "46f:F:i?")) >= 0)
    {
      switch(opt)
	{
	case '4':
	  state->filter_vis->show_ipv4 = 1;
	  break;
	case 'f':
	  state->filter_vis->ipv4_full_feed_th = atoi(optarg);
	  break;
	case '6':
	  state->filter_vis->show_ipv6 = 1;
	  break;
	case 'F':
	  state->filter_vis->ipv6_full_feed_th = atoi(optarg);
	  break;
	case 'i':
	  state->filter_vis->incremental = 1;
	  break;
	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  // if both of them are set or none of them (i.e. default)
  if(state->filter_vis->show_ipv4 == state->filter_vis->show_ipv6)
    {
      state->filter_vis->show_ipv4 = 1;
      state->filter_vis->show_ipv6 = 1;
    }
    
  /* dump doesn't take any arguments */
  if(optind != plugin->argc)
    {
      usage(plugin);
      return -1;
    }

  return 0;
}


/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
bgpcorsaro_plugin_t *bgpcorsaro_filtervis_alloc(bgpcorsaro_t *bgpcorsaro)
{
  return &bgpcorsaro_filtervis_plugin;
}


/** Implements the init_output function of the plugin API */
int bgpcorsaro_filtervis_init_output(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_filtervis_state_t *state;
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct bgpcorsaro_filtervis_state_t))) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not malloc bgpcorsaro_filtervis_state_t");
      goto err;
    }

  /** plugin initialization */
  if((state->filter_vis = filter_vis_create()) == NULL)
   {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not create filter_vis in bgpcorsaro_filtervis_state_t");
      goto err;
    }
  
  bgpcorsaro_plugin_register_state(bgpcorsaro->plugin_manager, plugin, state);

  /* defer opening the output file until we start the first interval */

  /* parse the arguments */
  if(parse_args(bgpcorsaro) != 0)
    {
      return -1;
    }
  
  return 0;

 err:
  bgpcorsaro_filtervis_close_output(bgpcorsaro);
  return -1;
}


/** Implements the close_output function of the plugin API */
int bgpcorsaro_filtervis_close_output(bgpcorsaro_t *bgpcorsaro)
{
  int i;
  khiter_t k;
  struct bgpcorsaro_filtervis_state_t *state = STATE(bgpcorsaro);

  if(state != NULL)
    {
      /* close all the outfile pointers */
      for(i = 0; i < OUTFILE_POINTERS; i++)
	{
	  if(state->outfile_p[i] != NULL)
	    {
	      wandio_wdestroy(state->outfile_p[i]);
	      state->outfile_p[i] = NULL;
	    }
	}
      state->outfile = NULL;

      /** plugin cleanup */
      free(state->filter_vis);
      state->filter_vis = NULL;
      
      bgpcorsaro_plugin_free_state(bgpcorsaro->plugin_manager,
				   PLUGIN(bgpcorsaro));
    }
  return 0;
}


/** Implements the start_interval function of the plugin API */
int bgpcorsaro_filtervis_start_interval(bgpcorsaro_t *bgpcorsaro,
					bgpcorsaro_interval_t *int_start)
{
  struct bgpcorsaro_filtervis_state_t *state = STATE(bgpcorsaro);
  if(state->outfile == NULL)
    {
      if((
	  state->outfile_p[state->outfile_n] =
	  bgpcorsaro_io_prepare_file(bgpcorsaro,
				     PLUGIN(bgpcorsaro)->name,
				     int_start)) == NULL)
	{
	  bgpcorsaro_log(__func__, bgpcorsaro, "could not open %s output file",
			 PLUGIN(bgpcorsaro)->name);
	  return -1;
	}
      state->outfile = state->outfile_p[state->outfile_n];
    }

  /** plugin interval start operations */
  filter_vis_start(state->filter_vis, int_start->time);

  bgpcorsaro_io_write_interval_start(bgpcorsaro, state->outfile, int_start);

  return 0;
}


/** Implements the end_interval function of the plugin API */
int bgpcorsaro_filtervis_end_interval(bgpcorsaro_t *bgpcorsaro,
				      bgpcorsaro_interval_t *int_end)
{
  struct bgpcorsaro_filtervis_state_t *state = STATE(bgpcorsaro);

  bgpcorsaro_log(__func__, bgpcorsaro, "Dumping stats for interval %d",
		 int_end->number);

  /** plugin interval end operations */
  filter_vis_end(state->filter_vis, int_end->time);

  bgpcorsaro_io_write_interval_end(bgpcorsaro, state->outfile, int_end);

  /* if we are rotating, now is when we should do it */
  if(bgpcorsaro_is_rotate_interval(bgpcorsaro))
    {
      /* leave the current file to finish draining buffers */
      assert(state->outfile != NULL);

      /* move on to the next output pointer */
      state->outfile_n = (state->outfile_n+1) %
	OUTFILE_POINTERS;

      if(state->outfile_p[state->outfile_n] != NULL)
	{
	  /* we're gonna have to wait for this to close */
	  wandio_wdestroy(state->outfile_p[state->outfile_n]);
	  state->outfile_p[state->outfile_n] =  NULL;
	}

      state->outfile = NULL;
    }
  return 0;
}


/** Implements the process_record function of the plugin API */
int bgpcorsaro_filtervis_process_record(bgpcorsaro_t *bgpcorsaro,
					bgpcorsaro_record_t *record)
{
  struct bgpcorsaro_filtervis_state_t *state = STATE(bgpcorsaro);

  /* no point carrying on if a previous plugin has already decided we should
     ignore this record */
  if((record->state.flags & BGPCORSARO_RECORD_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }

  assert(state != NULL);
  assert(record != NULL);
  bgpstream_record_t * bs_record = BS_REC(record);
  assert(bs_record != NULL);
  
  /** plugin operations related to a single record*/
  return filter_vis_update(state->filter_vis, bs_record);
}

