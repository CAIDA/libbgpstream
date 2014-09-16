/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#include "bgpstream_lib.h"
#include "bgpdump_util.h"

#include "utils.h"
#include "wandio_utils.h"

#include "bgpcorsaro_io.h"
#include "bgpcorsaro_log.h"
#include "bgpcorsaro_plugin.h"

#include "khash.h"
#include "bgpcorsaro_bgpribs.h"

#include "bgpribs_lib.h"


/** @file
 *
 * @brief Bgpcorsaro BgpRibs plugin implementation
 *
 * @author Chiara Orsini
 *
 */

/** The number of output file pointers to support non-blocking close at the end
    of an interval. If the wandio buffers are large enough that it takes more
    than 1 interval to drain the buffers, consider increasing this number */
#define OUTFILE_POINTERS 2

/** The name of this plugin */
#define PLUGIN_NAME "bgpribs"

/** The version of this plugin */
#define PLUGIN_VERSION "0.1"

/** Common plugin information across all instances */
static bgpcorsaro_plugin_t bgpcorsaro_bgpribs_plugin = {
  PLUGIN_NAME,                                          /* name */
  PLUGIN_VERSION,                                       /* version */
  BGPCORSARO_PLUGIN_ID_BGPRIBS,                        /* id */
  BGPCORSARO_PLUGIN_GENERATE_PTRS(bgpcorsaro_bgpribs), /* func ptrs */
  BGPCORSARO_PLUGIN_GENERATE_TAIL,
};

#define METRIC_PREFIX "bgp"



/** Holds the state for an instance of this plugin */
struct bgpcorsaro_bgpribs_state_t {
  /** The outfile for the plugin */
  iow_t *outfile;
  /** A set of pointers to outfiles to support non-blocking close */
  iow_t *outfile_p[OUTFILE_POINTERS];
  /** The current outfile */
  int outfile_n;

  /** interval start time */
  int interval_start;
  /** Hash of collector string to collector data */
  collectors_table_wrapper_t * collectors_table; 
};


/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, bgpribs, BGPCORSARO_PLUGIN_ID_BGPRIBS))
/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_BGPRIBS))

#if 0
/** Print usage information to stderr */
static void usage(bgpcorsaro_plugin_t *plugin)
{
  //TODO modify usage
  fprintf(stderr,
	  "plugin usage: %s [-HmM] [-t mode]\n"
	  "       -H         multi-line, human-readable (default)\n"
	  "       -m         one-line per entry with unix timestamps\n"
	  "       -M         one-line per entry with human readable timestamps (and some other differences that no human could ever comprehend)\n"
	  "       -t dump    timestamps for RIB dumps reflect the time of the dump (default)\n"
	  "       -t change  timestamps for RIB dumps reflect the last route modification\n",
	  plugin->argv[0]);
}

/** Parse the arguments given to the plugin */
static int parse_args(bgpcorsaro_t *bgpcorsaro)
{
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_bgpribs_state_t *state = STATE(bgpcorsaro);
  int opt;
  //TODO modify this: right now it doesn't do anything

  if(plugin->argc <= 0)
    {
      return 0;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, ":t:HmM?")) >= 0)
    {
      switch(opt)
	{
	case 'H':
	  break;

	case 'm':
	  break;

	case 'M':
	  break;

	case 't':
	  break;

	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  /* dump doesn't take any arguments */
  if(optind != plugin->argc)
    {
      usage(plugin);
      return -1;
    }

  return 0;
}
#endif


/** saves the beginning of the interval in the state */
static void stats_init(struct bgpcorsaro_bgpribs_state_t *state,
		       bgpcorsaro_interval_t *int_start) {
  assert(state != NULL);
  state->interval_start = int_start->time;
}


/** struct bgpdatainfo_t update (inside the interval) */
static int stats_update(struct bgpcorsaro_bgpribs_state_t *state,
			bgpcorsaro_record_t * record)
{
  assert(state != NULL);
  assert(record != NULL);


  return 0;
}

/** dump metrics at end of interval*/
static void stats_dump(struct bgpcorsaro_bgpribs_state_t *state)
{
  assert(state != NULL);
  khiter_t k;
  collectordata_t *collector_data;

}


/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
bgpcorsaro_plugin_t *bgpcorsaro_bgpribs_alloc(bgpcorsaro_t *bgpcorsaro)
{
  return &bgpcorsaro_bgpribs_plugin;
}

/** Implements the init_output function of the plugin API */
int bgpcorsaro_bgpribs_init_output(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_bgpribs_state_t *state;
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct bgpcorsaro_bgpribs_state_t))) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not malloc bgpcorsaro_bgpribs_state_t");
      goto err;
    }

  if((state->collectors_table = collectors_table_create()) == NULL) 
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not create collectors_table in bgpcorsaro_bgpribs_state_t");
      goto err;
    }

  bgpcorsaro_plugin_register_state(bgpcorsaro->plugin_manager, plugin, state);

#if 0
  /* parse the arguments */
  if(parse_args(bgpcorsaro) != 0)
    {
      return -1;
    }
#endif

  /* defer opening the output file until we start the first interval */

  return 0;

 err:
  bgpcorsaro_bgpribs_close_output(bgpcorsaro);
  return -1;
}

/** Implements the close_output function of the plugin API */
int bgpcorsaro_bgpribs_close_output(bgpcorsaro_t *bgpcorsaro)
{
  int i;
  khiter_t k;
  struct bgpcorsaro_bgpribs_state_t *state = STATE(bgpcorsaro);

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

      if(state->collectors_table != NULL)
	{
	  collectors_table_destroy(state->collectors_table);
	}

      bgpcorsaro_plugin_free_state(bgpcorsaro->plugin_manager,
				   PLUGIN(bgpcorsaro));
    }
  return 0;
}

/** Implements the start_interval function of the plugin API */
int bgpcorsaro_bgpribs_start_interval(bgpcorsaro_t *bgpcorsaro,
				       bgpcorsaro_interval_t *int_start)
{
  struct bgpcorsaro_bgpribs_state_t *state = STATE(bgpcorsaro);
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
      state->outfile = state->
	outfile_p[state->outfile_n];
    }

  stats_init(state, int_start);

  bgpcorsaro_io_write_interval_start(bgpcorsaro, state->outfile, int_start);

  return 0;
}

/** Implements the end_interval function of the plugin API */
int bgpcorsaro_bgpribs_end_interval(bgpcorsaro_t *bgpcorsaro,
				    bgpcorsaro_interval_t *int_end)
{
  struct bgpcorsaro_bgpribs_state_t *state = STATE(bgpcorsaro);

  bgpcorsaro_log(__func__, bgpcorsaro, "Dumping stats for interval %d",
		 int_end->number);
  /* dump statistics */
  stats_dump(state);

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
int bgpcorsaro_bgpribs_process_record(bgpcorsaro_t *bgpcorsaro,
				       bgpcorsaro_record_t *record)
{
  struct bgpcorsaro_bgpribs_state_t *state = STATE(bgpcorsaro);

  /* no point carrying on if a previous plugin has already decided we should
     ignore this record */
  if((record->state.flags & BGPCORSARO_RECORD_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }

  assert(state != NULL);
  return stats_update(state, record);
}



// ###################################################################################################
// 
// ###################################################################################################

/**  check if these functions still make sense START */
#if 0

static void ribs_table_reset(ribs_table_t *ribs_table) 
{  
  assert(ribs_table != NULL);
  /** we remove prefixes from each set (ipv4 and ipv6) 
   *  without deallocating memory  */
  kh_clear(ipv4_rib_t, ribs_table->ipv4_rib);
  kh_clear(ipv6_rib_t, ribs_table->ipv6_rib);
}

static void ases_table_reset(ases_table_wrapper_t *ases_table) 
{  
  assert(ases_table != NULL);
  /** we remove ases without deallocating memory  */
  kh_clear(ases_table_t, ases_table->table);
}

static void prefixes_table_reset(prefixes_table_t *prefixes_table) 
{  
  assert(prefixes_table != NULL);
  /** we remove prefixes from each set (ipv4 and ipv6) 
   *  without deallocating memory  */
  kh_clear(ipv4_prefixes_table_t, prefixes_table->ipv4_prefixes_table);
  kh_clear(ipv6_prefixes_table_t, prefixes_table->ipv6_prefixes_table);
}

static void peerdata_update(bgpstream_elem_t *elem,
			    peerdata_t *peer_data)
{
  assert(elem);
  assert(peer_data);
  assert(peer_data->ribs_table);
  assert(peer_data->prefixes_table->ipv4_prefixes_table);
  assert(peer_data->prefixes_table->ipv6_prefixes_table);
  khiter_t k;
  int khret;

  peer_data->num_elem[elem->type]++;

  /* check if it is a state message, and if it is 
   * a peer down message, then clear the tables */
  if(elem->type == BST_STATE) {
    if(elem->new_state != BST_ESTABLISHED) {
      /* we remove all the prefixes from the ribs */
      ribs_table_reset(peer_data->ribs_table);
    }
    return;
  }

  // creating prefixdata and populating the structure
  prefixdata_t pd;
  pd.origin_as = 0;
  if( (elem->type == BST_RIB || elem->type == BST_ANNOUNCEMENT) &&
      elem->aspath.hop_count > 0 && 
      elem->aspath.type == BST_UINT32_ASPATH ) {
      pd.origin_as = elem->aspath.numeric_aspath[(elem->aspath.hop_count-1)];
  }

  /* Updating the ribs  */
  if(elem->prefix.number.type == BST_IPV4) { // ipv4 prefix
    k = kh_get(ipv4_rib_t, peer_data->ribs_table->ipv4_rib,
	       elem->prefix);
    // update prefix into ipv4 prefixes table
    if(elem->type == BST_RIB || elem->type == BST_ANNOUNCEMENT)
      {
	// insert key if it doesn't exist
	if(k == kh_end(peer_data->ribs_table->ipv4_rib))
	  {
	    k = kh_put(ipv4_rib_t, peer_data->ribs_table->ipv4_rib, 
		       elem->prefix, &khret);
	  }
	// updating the prefixdata structure
	kh_value(peer_data->ribs_table->ipv4_rib, k) = pd;
      }
    else
      {
	if(elem->type == BST_WITHDRAWAL) 
	  {
	    // remove if it exists
	    if(k != kh_end(peer_data->ribs_table->ipv4_rib))
	      {
		kh_del(ipv4_rib_t, peer_data->ribs_table->ipv4_rib, k);
	      }  
	  }
      }
  }
  else { // ipv6 prefix
    // update prefix into ipv6 prefixes table
    k = kh_get(ipv6_rib_t, peer_data->ribs_table->ipv6_rib,
	       elem->prefix);
    // update prefix into ipv4 prefixes table
    if(elem->type == BST_RIB || elem->type == BST_ANNOUNCEMENT)
      {
	// insert if it doesn't exist
	if(k == kh_end(peer_data->ribs_table->ipv6_rib))
	  {
	    k = kh_put(ipv6_rib_t, peer_data->ribs_table->ipv6_rib, 
		       elem->prefix, &khret);
	  }
	// updating the prefixdata structure
	kh_value(peer_data->ribs_table->ipv6_rib, k) = pd;
      }
    else
      {
	if(elem->type == BST_WITHDRAWAL) 
	  {
	    // remove if it exists
	    if(k != kh_end(peer_data->ribs_table->ipv6_rib))
	      {
		kh_del(ipv6_rib_t, peer_data->ribs_table->ipv6_rib, k);
	      }  
	  }
      }
  }

  /* Updating affected_prefixes_table */
  if(elem->prefix.number.type == BST_IPV4) { // ipv4 prefix
    k = kh_get(ipv4_prefixes_table_t, peer_data->affected_prefixes_table->ipv4_prefixes_table,
	       elem->prefix);
    // update prefix into ipv4 affected prefixes table
    if(elem->type == BST_ANNOUNCEMENT || elem->type == BST_WITHDRAWAL)
      {
	// insert if it doesn't exist
	if(k == kh_end(peer_data->affected_prefixes_table->ipv4_prefixes_table))
	  {
	    k = kh_put(ipv4_prefixes_table_t, peer_data->affected_prefixes_table->ipv4_prefixes_table, 
		       elem->prefix, &khret);
	  }
      }
  }
  else { // ipv6 prefix
    // update prefix into ipv6 affected prefixes table
    k = kh_get(ipv6_prefixes_table_t, peer_data->affected_prefixes_table->ipv6_prefixes_table,
	       elem->prefix);
    // update prefix into ipv4 prefixes table
    if(elem->type == BST_ANNOUNCEMENT || elem->type == BST_WITHDRAWAL)
      {
	// insert if it doesn't exist
	if(k == kh_end(peer_data->affected_prefixes_table->ipv6_prefixes_table))
	  {
	    k = kh_put(ipv6_prefixes_table_t, peer_data->affected_prefixes_table->ipv6_prefixes_table, 
		       elem->prefix, &khret);
	  }
      }
  }

  // updating affected ases and "internal/non_std" updates count
  uint32_t origin_as;    
  if(elem->type == BST_ANNOUNCEMENT) 
    {
      if(elem->aspath.hop_count == 0)
	{ // received an internal update 
	  peer_data->empty_origin_as_cnt++;
	}
      else
	{
	  // update origin ASes announcing a change in this interval
	  if(elem->aspath.type == BST_UINT32_ASPATH && elem->aspath.hop_count > 0) 
	    { 
	      origin_as = elem->aspath.numeric_aspath[(elem->aspath.hop_count-1)];
	      k = kh_get(ases_table_t, peer_data->announcing_ases_table->table, origin_as);
	      if(k == kh_end(peer_data->announcing_ases_table->table))
		{
		  k = kh_put(ases_table_t, peer_data->announcing_ases_table->table, 
			     origin_as, &khret);
		}
	    }
	  else
	    { 
	      // if the path is a string and the hop count is zero, then it is non_std
	      // aspath
	      if(elem->aspath.type == BST_STRING_ASPATH && elem->aspath.hop_count > 0) 
		{
		  peer_data->non_std_origin_as_cnt++;
		}
	    }
	}
    }

}

// get prefixes and ases all in one round - global and local
static void ribs_table_parser(prefixes_table_t *global_prefixes, 
			      ases_table_wrapper_t *local_ases, ases_table_wrapper_t *global_ases, 
			      ribs_table_t * to_read)
{
  khiter_t k;
  khiter_t k_check;
  int khret;
  bgpstream_prefix_t prefix;
  prefixdata_t pd; 
  // update ipv4 prefixes table
  for(k = kh_begin(to_read->ipv4_rib);
      k != kh_end(to_read->ipv4_rib); ++k)
    {
      if (kh_exist(to_read->ipv4_rib, k))
	{
	  // get prefix from "to_read" table
	  prefix = kh_key(to_read->ipv4_rib, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ipv4_prefixes_table_t, global_prefixes->ipv4_prefixes_table,
			       prefix)) == kh_end(global_prefixes->ipv4_prefixes_table))
	    {
	      k_check = kh_put(ipv4_prefixes_table_t, global_prefixes->ipv4_prefixes_table, 
			       prefix, &khret);
	    }
	  // get prefixdata from "to read" table
	  pd = kh_value(to_read->ipv4_rib, k);
	  if( pd.origin_as != 0)  // 0 means no "standard" as origin
	    {
	      // insert if it does not exist in local and global ases
	      if((k_check = kh_get(ases_table_t, local_ases->table,
				   pd.origin_as)) == kh_end(local_ases->table))
		{
		  k_check = kh_put(ases_table_t, local_ases->table, pd.origin_as, &khret);
		}
	      if((k_check = kh_get(ases_table_t, global_ases->table,
				   pd.origin_as)) == kh_end(global_ases->table))
		{
		  k_check = kh_put(ases_table_t, global_ases->table, pd.origin_as, &khret);
		}	  
	    }
	}
    }
  // update ipv6 prefixes table
  for(k = kh_begin(to_read->ipv6_rib);
      k != kh_end(to_read->ipv6_rib); ++k)
    {
      if (kh_exist(to_read->ipv6_rib, k))
	{
	  // get prefix from "to_read" table
	  prefix = kh_key(to_read->ipv6_rib, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ipv6_prefixes_table_t, global_prefixes->ipv6_prefixes_table,
			       prefix)) == kh_end(global_prefixes->ipv6_prefixes_table))
	    {
	      k_check = kh_put(ipv6_prefixes_table_t, global_prefixes->ipv6_prefixes_table, 
			       prefix, &khret);
	    }
	  // get prefixdata from "to read" table
	  pd = kh_value(to_read->ipv6_rib, k);
	  if( pd.origin_as != 0)  // 0 means no "standard" as origin
	    {
	      // insert if it does not exist in local and global ases
	      if((k_check = kh_get(ases_table_t, local_ases->table,
				   pd.origin_as)) == kh_end(local_ases->table))
		{
		  k_check = kh_put(ases_table_t, local_ases->table, pd.origin_as, &khret);
		}
	      if((k_check = kh_get(ases_table_t, global_ases->table,
				   pd.origin_as)) == kh_end(global_ases->table))
		{
		  k_check = kh_put(ases_table_t, global_ases->table, pd.origin_as, &khret);
		}
	    }
	}
    }
}


static void prefixes_table_union(prefixes_table_t * to_update, prefixes_table_t * to_read)
{
  khiter_t k;
  khiter_t k_check;
  int khret;
  bgpstream_prefix_t prefix; 
  // update ipv4 prefixes table
  for(k = kh_begin(to_read->ipv4_prefixes_table);
      k != kh_end(to_read->ipv4_prefixes_table); ++k)
    {
      if (kh_exist(to_read->ipv4_prefixes_table, k))
	{
	  // get prefix from "to_read" table
	  prefix = kh_key(to_read->ipv4_prefixes_table, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ipv4_prefixes_table_t, to_update->ipv4_prefixes_table,
			       prefix)) == kh_end(to_update->ipv4_prefixes_table))
	    {
	      k_check = kh_put(ipv4_prefixes_table_t, to_update->ipv4_prefixes_table, 
			       prefix, &khret);
	    }
	}
    }
  // update ipv6 prefixes table
  for(k = kh_begin(to_read->ipv6_prefixes_table);
      k != kh_end(to_read->ipv6_prefixes_table); ++k)
    {
      if (kh_exist(to_read->ipv6_prefixes_table, k))
	{
	  // get prefix from "to_read" table
	  prefix = kh_key(to_read->ipv6_prefixes_table, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ipv6_prefixes_table_t, to_update->ipv6_prefixes_table,
			       prefix)) == kh_end(to_update->ipv6_prefixes_table))
	    {
	      k_check = kh_put(ipv6_prefixes_table_t, to_update->ipv6_prefixes_table, 
			       prefix, &khret);
	    }
	}
    }
}


static void ases_table_union(ases_table_wrapper_t * to_update, ases_table_wrapper_t * to_read)
{
  khiter_t k;
  khiter_t k_check;
  int khret;
  uint32_t origin_as;
  // update ases table
  for(k = kh_begin(to_read->table);
      k != kh_end(to_read->table); ++k)
    {
      if (kh_exist(to_read->table, k))
	{
	  // get prefix from "to_read" table
	  origin_as = kh_key(to_read->table, k);
	  // insert if it does not exist in  "to_update"
	  if((k_check = kh_get(ases_table_t, to_update->table,
			       origin_as)) == kh_end(to_update->table))
	    {
	      k_check = kh_put(ases_table_t, to_update->table, 
			       origin_as, &khret);
	    }
	}
    }
}


static void peerdata_new_rib(peerdata_t *peer_data)
{
  assert(peer_data != NULL); 
  assert(peer_data->prefixes_table != NULL); 
  assert(peer_data->ases_table != NULL); 
  /* we remove all the prefixes from the tables */
  ribs_table_reset(peer_data->ribs_table);
  /* we remove all the ASes from the table */
  ases_table_reset(peer_data->ases_table);
}

static void peerdata_end_of_interval(peerdata_t *peer_data)
{
  assert(peer_data != NULL); 
  memset(peer_data->num_elem, 0, sizeof(peer_data->num_elem));
  // we remove all the prefixes affected in the interval
  prefixes_table_reset(peer_data->affected_prefixes_table);
  // we remove all the ases (we recompute them at every interval)
  ases_table_reset(peer_data->ases_table);
  // we remove all the ases affected in the interval
  ases_table_reset(peer_data->announcing_ases_table);
  // we reset the number of non standard origin ASes events
  peer_data->non_std_origin_as_cnt = 0;
  // we reset the number of empty origin ASes events
  peer_data->empty_origin_as_cnt = 0;
}


static void peers_table_update(bgpstream_elem_t * elem,
			       peers_table_t *peers_table) 
{
  assert(peers_table != NULL);
  assert(elem != NULL);
  khiter_t k;
  int khret;
  peerdata_t * peer_data = NULL;

  // ipv4 peer
  if(elem->peer_address.type == BST_IPV4) {
    /* check if this peer is in the hash already */
    if((k = kh_get(ipv4_peers_table_t, peers_table->ipv4_peers_table,
		   elem->peer_address)) ==
       kh_end(peers_table->ipv4_peers_table))
      {
	/* create a new data structure */
	peer_data = peerdata_create();
	/* add it to the hash */
	k = kh_put(ipv4_peers_table_t, peers_table->ipv4_peers_table, 
		   elem->peer_address, &khret);
	kh_value(peers_table->ipv4_peers_table, k) = peer_data;
      }
    else
      {
	/* already exists, just get it */
	peer_data = kh_value(peers_table->ipv4_peers_table, k);
      }
  }
  // ipv6 peer
  if(elem->peer_address.type == BST_IPV6) {
    /* check if this peer is in the hash already */
    if((k = kh_get(ipv6_peers_table_t, peers_table->ipv6_peers_table,
		   elem->peer_address)) ==
       kh_end(peers_table->ipv6_peers_table))
      {
	/* create a new data structure */
	peer_data = peerdata_create();
	/* add it to the hash */
	k = kh_put(ipv6_peers_table_t, peers_table->ipv6_peers_table, 
		   elem->peer_address, &khret);
	kh_value(peers_table->ipv6_peers_table, k) = peer_data;
      }
    else
      {
	/* already exists, just get it */
	peer_data = kh_value(peers_table->ipv6_peers_table, k);
      }    
  }
  
  if(peer_data != NULL) {
    peerdata_update(elem, peer_data);
  }  
}


static void peers_table_new_rib(peers_table_t *peers_table) 
{
  khiter_t k;  
  /* send a new rib signal to all ipv4 peers */
  for (k = kh_begin(peers_table->ipv4_peers_table);
       k != kh_end(peers_table->ipv4_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv4_peers_table, k))
	{
	  /* reset the value */
	  peerdata_new_rib(kh_value(peers_table->ipv4_peers_table, k));
	}
    }   

  /* send a new rib signal to all ipv6 peers */
  for (k = kh_begin(peers_table->ipv6_peers_table);
       k != kh_end(peers_table->ipv6_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv6_peers_table, k))
	{
	  /* reset the value */
	  peerdata_new_rib(kh_value(peers_table->ipv6_peers_table, k));
	}
    }  
}


static void peers_table_end_of_interval(peers_table_t *peers_table) 
{
  /** we do not remove peers from peers_table as they are
   *  a stable entity, we just zeroes their peerdata 
   *  such process is performed by peerdata_end_of_interval
   *  everytime we dump with peerdata_dump*/
}


static void collectordata_update(bgpcorsaro_record_t * record,
				 collectordata_t *collector_data)
{
  assert(collector_data != NULL);
  assert(collector_data->peers_table != NULL);
  bgpstream_record_t * bs_record = BS_REC(record);
  bgpstream_elem_t * bs_elem_queue;
  bgpstream_elem_t * bs_iterator;

  collector_data->num_records[bs_record->status]++;
  if(bs_record->status == VALID_RECORD)
    {
      /* if it is the first record of a RIB
       * send a message to all peers, so they
       * can reset their prefixes tables */
      if(bs_record->attributes.dump_type == BGPSTREAM_RIB &&
	 bs_record->dump_pos == DUMP_START) {
	peers_table_new_rib(collector_data->peers_table);
      }
      bs_elem_queue = bgpstream_get_elem_queue(bs_record);
      bs_iterator = bs_elem_queue;
      while(bs_iterator != NULL)
	{
	  collector_data->num_elem[bs_iterator->type]++;
	  /* update peer information */
	  peers_table_update(bs_iterator, collector_data->peers_table);
	  // other information are computed at dump time
	  bs_iterator = bs_iterator->next;
	}
      bgpstream_destroy_elem_queue(bs_elem_queue);

    }
  return;
}


// ----------------- other dump functions here -------------------------- //


static void peerdata_dump(char *dump_project, char *dump_collector, char *peer_address,
			  int int_start_time, peerdata_t *peer_data, 
			  collectordata_t *collector_data)
{
  graphite_safe(peer_address);
  /* rib_entries */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.rib_entry_cnt %"PRIu64" %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->num_elem[BST_RIB],
	  int_start_time);

  /* announcements */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.announcement_entry_cnt %"PRIu64" %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->num_elem[BST_ANNOUNCEMENT],
	  int_start_time);
  
  /* withdrawals */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.withdrawal_entry_cnt %"PRIu64" %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->num_elem[BST_WITHDRAWAL],
	  int_start_time);

  /* withdrawals */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.state_messages_cnt %"PRIu64" %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->num_elem[BST_STATE],
	  int_start_time);

  /* number of ipv4/ipv6 prefixes */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.ipv4_rib_size %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->ribs_table->ipv4_rib),
	  int_start_time);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.ipv6_rib_size %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->ribs_table->ipv6_rib),
	  int_start_time);

  /* number of ipv4/ipv6 prefixes affected by a change during the current interval */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.ipv4_affected_prefixes %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->affected_prefixes_table->ipv4_prefixes_table),
	  int_start_time);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.ipv6_affected_prefixes %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->affected_prefixes_table->ipv6_prefixes_table),
	  int_start_time);

  /* number of unique ASes (no sets/confeds) announcing at least
   * one new prefix in the current interval */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.announcing_ases %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->announcing_ases_table->table),
	  int_start_time);

  /* number non_std_origin_as occurrencies (updates only) */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.non_std_origin_as_cnt %d %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->non_std_origin_as_cnt,
	  int_start_time);

  /* number empty_origin_as_cnt occurrencies (updates only) */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.empty_origin_as_cnt %d %d\n",
	  dump_project, dump_collector, peer_address,
	  peer_data->empty_origin_as_cnt,
	  int_start_time);  


  /* populate collector_data structures with peer information */ 
  ribs_table_parser(collector_data->prefixes_table, peer_data->ases_table,
		    collector_data->ases_table, peer_data->ribs_table);

  prefixes_table_union(collector_data->affected_prefixes_table, peer_data->affected_prefixes_table);
  ases_table_union(collector_data->announcing_ases_table, peer_data->announcing_ases_table);

  /* number of unique ASes in the ribs */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.%s.origin_ases_in_ribs_cnt %d %d\n",
	  dump_project, dump_collector, peer_address,
	  kh_size(peer_data->ases_table->table),
	  int_start_time);

}


static void peers_table_dump(char *dump_project, char *dump_collector,
			     int int_start_time, peers_table_t *peers_table,
			     collectordata_t *collector_data) 
{
  assert(ch_prefix != NULL);
  assert(peers_table != NULL);

  khiter_t k;
  int khret;
  peerdata_t * peer_data = NULL;
  bgpstream_ip_address_t ip;
  char ip4_str[INET_ADDRSTRLEN];
  char ip6_str[INET6_ADDRSTRLEN];

  // ipv4 peers informations
  for(k = kh_begin(peers_table->ipv4_peers_table);
      k != kh_end(peers_table->ipv4_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv4_peers_table, k))
	{
	  ip = kh_key(peers_table->ipv4_peers_table, k);
	  inet_ntop(AF_INET, &(ip.address.v4_addr), ip4_str, INET_ADDRSTRLEN);
	  peer_data = kh_value(peers_table->ipv4_peers_table, k);
	  peerdata_dump(dump_project, dump_collector, ip4_str, int_start_time, peer_data, collector_data);
	  peerdata_end_of_interval(kh_value(peers_table->ipv4_peers_table, k));
	}      
    }

  // ipv6 peers informations
  for(k = kh_begin(peers_table->ipv6_peers_table);
      k != kh_end(peers_table->ipv6_peers_table); ++k)
    {
      if (kh_exist(peers_table->ipv6_peers_table, k))
	{
	  ip = kh_key(peers_table->ipv6_peers_table, k);	  
	  inet_ntop(AF_INET6, &(ip.address.v6_addr), ip6_str, INET6_ADDRSTRLEN);
	  peer_data = kh_value(peers_table->ipv6_peers_table, k);
	  peerdata_dump(dump_project, dump_collector, ip6_str, int_start_time, peer_data, collector_data);	
	  peerdata_end_of_interval(kh_value(peers_table->ipv6_peers_table, k));
	}
    }
}


// ----------------- other dump functions here -------------------------- //





static void collectordata_dump(collectordata_t *collector_data, int int_start_time)
{
  assert(collector_data);
  /* num_valid_records */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.valid_record_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_records[VALID_RECORD],
	  int_start_time);

  /* rib_entries */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.rib_entry_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_elem[BST_RIB],
	  int_start_time);

  /* announcements */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.announcement_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_elem[BST_ANNOUNCEMENT],
	  int_start_time);

  /* withdrawals */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.withdrawal_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_elem[BST_WITHDRAWAL],
	  int_start_time);

  /* state messages */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.state_messages_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->num_elem[BST_STATE],
	  int_start_time);

  /* peer-related statistics */ 

  /* ipv4 / ipv6 peers */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv4_peers_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->peers_table->ipv4_peers_table),
	  int_start_time);	  

  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv6_peers_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->peers_table->ipv6_peers_table),
	  int_start_time);


  peers_table_dump(collector_data->dump_project, collector_data->dump_collector,
		   int_start_time, collector_data->peers_table, collector_data);

  peers_table_end_of_interval(collector_data->peers_table);


  /* number of ipv4/ipv6 prefixes per collector */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv4_rib_size %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->prefixes_table->ipv4_prefixes_table),
	  int_start_time);	  

  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv6_rib_size %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->prefixes_table->ipv6_prefixes_table),
	  int_start_time);	  

  /* number of ipv4/ipv6 prefixes affected by a change during the current interval */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv4_affected_prefixes %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->affected_prefixes_table->ipv4_prefixes_table),
	  int_start_time);	  

  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.ipv6_affected_prefixes %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->affected_prefixes_table->ipv6_prefixes_table),
	  int_start_time);

  /* number of unique origin ases in ribs per collector */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.origin_ases_in_ribs_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->ases_table->table),
	  int_start_time);
  
  /* number of announcing ases per collector */
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.announcing_ases %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->announcing_ases_table->table),
	  int_start_time);
  
}


static void collectordata_end_of_interval(collectordata_t *collector_data)
{
  memset(collector_data->num_records, 0,
	 sizeof(collector_data->num_records));

  memset(collector_data->num_elem, 0,
	 sizeof(collector_data->num_elem));

  // resetting all the collector information that need
  // to be recomputed at the end of every interval
  prefixes_table_reset(collector_data->prefixes_table);
  prefixes_table_reset(collector_data->affected_prefixes_table);
  ases_table_reset(collector_data->ases_table);
  ases_table_reset(collector_data->announcing_ases_table);

  /* every "piece" take care of his own end of interval 
   * just right after the dump function 
   * this way we reduce the number of loops to reset data
   */
}

#endif

/**  check if these functions still make sense END */

