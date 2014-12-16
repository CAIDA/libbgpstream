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


#define BGPRIBS_METRIC_PREFIX "bgpribs"
#define BGPRIBS_IPV4_FULL_SIZE 450000
#define BGPRIBS_IPV6_FULL_SIZE 10000




/** Holds the state for an instance of this plugin */
struct bgpcorsaro_bgpribs_state_t {

  /** The outfile for the plugin */
  iow_t *outfile;
  /** A set of pointers to outfiles to support non-blocking close */
  iow_t *outfile_p[OUTFILE_POINTERS];
  /** The current outfile */
  int outfile_n;
  
  bgpribs_t *bgp_ribs;  /// plugin-related structure  
};


/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, bgpribs, BGPCORSARO_PLUGIN_ID_BGPRIBS))

/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_BGPRIBS))


/** Print usage information to stderr */
static void usage(bgpcorsaro_plugin_t *plugin)
{
#ifdef WITH_BGPWATCHER
  fprintf(stderr,
	  "plugin usage: %s [-w46] [-u <uri] [-m pfx] \n"
	  "       -w         enables bgpwatcher transmission (default: off)\n"
	  "       -u         0MQ-style URI to connect to server (default: tcp://*:6300)\n"
	  "       -4         when sending ipv4 table to the bgpwatcher, only send full feed (default: off)\n"
	  "       -6         when sending ipv6 table to the bgpwatcher, only send full feed (default: off)\n"
	  "       -f         set the ipv4 full routing table size  (default: %d)\n"
	  "       -F         set the ipv6 full routing table size  (default: %d)\n"
	  "       -m         metric prefix (default: %s)\n",
	  plugin->argv[0], BGPRIBS_IPV4_FULL_SIZE, BGPRIBS_IPV6_FULL_SIZE, BGPRIBS_METRIC_PREFIX);
#else
  fprintf(stderr,
	  "plugin usage: %s [-m pfx]\n"
	  "       -m         metric prefix (default: %s)\n",
	  plugin->argv[0], BGPRIBS_METRIC_PREFIX);
#endif

}

/** Parse the arguments given to the plugin */
static int parse_args(bgpcorsaro_t *bgpcorsaro)
{
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_bgpribs_state_t *state = STATE(bgpcorsaro);
  int opt;

  char *met_pfx = NULL;
  
  if(plugin->argc <= 0)
    {
      return 0;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;
#ifdef WITH_BGPWATCHER
  uint8_t bgpwatcher_on = 0;
  char *server_uri = NULL;
  uint8_t only_ipv4_full_on = 0;
  uint8_t only_ipv6_full_on = 0;
  uint32_t ipv4_full_size = BGPRIBS_IPV4_FULL_SIZE;
  uint32_t ipv6_full_size = BGPRIBS_IPV6_FULL_SIZE;  
  while((opt = getopt(plugin->argc, plugin->argv, ":m:w46f:F:u:?")) >= 0)
#else
    while((opt = getopt(plugin->argc, plugin->argv, ":m:?")) >= 0)
#endif
    {
      switch(opt)
	{
	case 'm':
	  met_pfx = strdup(optarg);
	  break;
#ifdef WITH_BGPWATCHER
	case 'w':
	  bgpwatcher_on = 1;
	  break;
	case '4':
	  only_ipv4_full_on = 1; 
	  break;
	case '6':
	  only_ipv6_full_on = 1; 
	  break;
	case 'f':
	  only_ipv4_full_on = 1; 
	  ipv4_full_size = atoi(optarg); 
	  break;
	case 'F':
	  only_ipv6_full_on = 1; 
	  ipv6_full_size = atoi(optarg); 
	  break;
	case 'u':
	  server_uri = strdup(optarg);
	  break;
#endif
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

  if(met_pfx != NULL)
    {
      bgpribs_set_metric_pfx(state->bgp_ribs, met_pfx);
      free(met_pfx);
      met_pfx = NULL;
    }
  
#ifdef WITH_BGPWATCHER
  if(bgpwatcher_on == 1)
    {
      if(bgpribs_set_watcher(state->bgp_ribs, server_uri) == -1)
	{
	  return -1;
	}
      bgpribs_set_fullfeed_filters(state->bgp_ribs,
				   only_ipv4_full_on, only_ipv6_full_on,
				   ipv4_full_size, ipv6_full_size);
    }
  if(server_uri != NULL)
    {
      free(server_uri);
    }   
#endif
  
  return 0;
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

  /** plugin initialization */
  if((state->bgp_ribs = bgpribs_create(BGPRIBS_METRIC_PREFIX)) == NULL) 
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not create bgpribs in bgpcorsaro_bgpribs_state_t");
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

      /** plugin cleanup */
      bgpribs_destroy(state->bgp_ribs);
      state->bgp_ribs = NULL;

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

  /** plugin interval start operations */
  bgpribs_interval_start(state->bgp_ribs, int_start->time);

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

  /** plugin end of interval operations */
  if(bgpribs_interval_end(state->bgp_ribs, int_end->time) < 0)
    {
      // an error occurred during the interval_end operations
      bgpcorsaro_log(__func__, bgpcorsaro, "could not dump stats for %s plugin",
		     PLUGIN(bgpcorsaro)->name);      
      return -1;
    }

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
  assert(record != NULL);
  bgpstream_record_t * bs_record = BS_REC(record);
  assert(bs_record != NULL);
  /** plugin operations related to a single record*/
  return bgpribs_process_record(state->bgp_ribs, bs_record);

}

