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

#include "bgpstream.h"

#include "utils.h"
#include "wandio_utils.h"

#include "bgpcorsaro_io.h"
#include "bgpcorsaro_log.h"
#include "bgpcorsaro_plugin.h"

#include "bgpcorsaro_routingtables.h"
#include "routingtables.h"

/** @file
 *
 * @brief Bgpcorsaro RoutingTables plugin implementation
 *
 * @author Chiara Orsini
 *
 */

/** The number of output file pointers to support non-blocking close at the end
    of an interval. If the wandio buffers are large enough that it takes more
    than 1 interval to drain the buffers, consider increasing this number */
#define OUTFILE_POINTERS 2

/** The name of this plugin */
#define PLUGIN_NAME "routingtables"

/** The version of this plugin */
#define PLUGIN_VERSION "0.1"

/** Common plugin information across all instances */
static bgpcorsaro_plugin_t bgpcorsaro_routingtables_plugin = {
  PLUGIN_NAME,                                               /* name */
  PLUGIN_VERSION,                                            /* version */
  BGPCORSARO_PLUGIN_ID_ROUTINGTABLES,                        /* id */
  BGPCORSARO_PLUGIN_GENERATE_PTRS(bgpcorsaro_routingtables), /* func ptrs */
  BGPCORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct bgpcorsaro_routingtables_state_t {
  
  /** The outfile for the plugin */
  iow_t *outfile;

  /** A set of pointers to outfiles to support non-blocking close */
  iow_t *outfile_p[OUTFILE_POINTERS];

  /** The current outfile */
  int outfile_n;

  /* plugin custom variables */
  
  /** routing tables instance */
  routingtables_t *routing_tables;

  /** prefix used for outputed metrics */
  char *metric_prefix;

  /** ipv4 full feed size threshold */
  int ipv4_fullfeed_th;

  /** ipv6 full feed size threshold */
  int ipv6_fullfeed_th;

#ifdef WITH_BGPWATCHER
  /** Transmission of bgp views flag */
  uint8_t watcher_tx;

  /** Watcher server uri */
  char *watcher_server_uri;

  /** Client identity */
  char *watcher_client_id;

  /** Send partial feed  */
  uint8_t send_partial_feed;
#endif
};

/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, routingtables, BGPCORSARO_PLUGIN_ID_ROUTINGTABLES))

/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_ROUTINGTABLES))


/** Print usage information to stderr */
static void usage(bgpcorsaro_t *bgpcorsaro)
{
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_routingtables_state_t *state = STATE(bgpcorsaro);

  fprintf(stderr,
          "plugin usage: %s [<options>]\n"
	  "       -m <prefix>                  metric prefix (default: %s)\n"
	  "       -f <fullfeed-ipv4-th>        set the IPv4 full feed threshold  (default: %d)\n"
	  "       -F <fullfeed-ipv6-th>        set the IPv6 full feed threshold  (default: %d)\n"
#ifdef WITH_BGPWATCHER
          "       -w                           enables bgpwatcher transmission (default: off)\n"
	  "       -u <server-uri>              0MQ-style URI to connect to server (default: tcp://*:6300)\n"
	  "       -c <client-identity>         set client identity name (default: randomly choosen)\n"
	  "       -a                           send full feed and partial tables to the watcher (default: full feed only)\n"
#endif
          , // end of options
          plugin->argv[0],
          routingtables_get_metric_prefix(state->routing_tables),
          routingtables_get_fullfeed_threshold(state->routing_tables, BGPSTREAM_ADDR_VERSION_IPV4),
          routingtables_get_fullfeed_threshold(state->routing_tables, BGPSTREAM_ADDR_VERSION_IPV6));
}

/** Parse the arguments given to the plugin */
static int parse_args(bgpcorsaro_t *bgpcorsaro)
{
  int opt;
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_routingtables_state_t *state = STATE(bgpcorsaro);

  if(plugin->argc <= 0)
    {
      return 0;
    }
  
  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;
  
  /* parsing args */

  while((opt = getopt(plugin->argc, plugin->argv,
                      ":m:f:F:"
#ifdef WITH_BGPWATCHER
                      "wa46u:c:"
#endif
                      "?")) >= 0)
    {
      switch(opt)
	{
    	case 'm':
	  state->metric_prefix = strdup(optarg);
	  break;
    	case 'f':
	  state->ipv4_fullfeed_th = atoi(optarg);
	  break;
    	case 'F':
	  state->ipv6_fullfeed_th = atoi(optarg);
	  break;
#ifdef WITH_BGPWATCHER
    	case 'w':
	  state->watcher_tx = 1;
	  break;
    	case 'a':
	  state->send_partial_feed = 1;
	  break;          
        case 'u':
	  state->watcher_server_uri = strdup(optarg);
	  break;
        case 'c':
	  state->watcher_client_id = strdup(optarg);
	  break;
#endif
	case '?':
	case ':':
	default:
	  usage(bgpcorsaro);
	  return -1;
	}
    }

#ifdef WITH_BGPWATCHER

  
#endif
  
  return 0;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
bgpcorsaro_plugin_t *bgpcorsaro_routingtables_alloc(bgpcorsaro_t *bgpcorsaro)
{
  return &bgpcorsaro_routingtables_plugin;
}

/** Implements the init_output function of the plugin API */
int bgpcorsaro_routingtables_init_output(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_routingtables_state_t *state;
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct bgpcorsaro_routingtables_state_t))) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not malloc bgpcorsaro_routingtables_state_t");
      goto err;
    }

  /** initialize plugin custom variables */
  if((state->routing_tables = routingtables_create(PLUGIN(bgpcorsaro)->argv[0], bgpcorsaro->timeseries)) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not create routingtables in routingtables plugin");
      goto err;      
    }
  state->metric_prefix = NULL;
  state->ipv4_fullfeed_th = -1; // default: not set
  state->ipv6_fullfeed_th = -1; // default: not set
#ifdef WITH_BGPWATCHER
  state->watcher_tx = 0; // default: don't send data
  state->watcher_server_uri = NULL;
  state->watcher_client_id = NULL;
  state->send_partial_feed = 0; // default: full feeds only
#endif

  bgpcorsaro_plugin_register_state(bgpcorsaro->plugin_manager, plugin, state);
    
  /* parse the arguments */
  if(parse_args(bgpcorsaro) != 0)
    {
      goto err;
    }
  
  // update state with parsed args
  if(state->metric_prefix != NULL)
    {
      routingtables_set_metric_prefix(state->routing_tables, state->metric_prefix);
    }
  
  if(state->ipv4_fullfeed_th != -1)
    {
      routingtables_set_fullfeed_threshold(state->routing_tables,
                                           BGPSTREAM_ADDR_VERSION_IPV4,
                                           state->ipv4_fullfeed_th);
    }
  
  if(state->ipv6_fullfeed_th != -1)
    {
      routingtables_set_fullfeed_threshold(state->routing_tables,
                                           BGPSTREAM_ADDR_VERSION_IPV6,
                                           state->ipv6_fullfeed_th);
    }

  
#ifdef WITH_BGPWATCHER
  if(state->watcher_tx)
    {
      if(routingtables_activate_watcher_tx(state->routing_tables,
                                           state->watcher_client_id,
                                           state->watcher_server_uri) < 0)
        {
          goto err;
        }
      bgpcorsaro_log(__func__, NULL,
                     "BGP watcher connection setup successful");

      if(state->send_partial_feed == 1)
        {
          routingtables_activate_partial_feed_tx(state->routing_tables); 
        }

    }
#endif

  /* defer opening the output file until we start the first interval */

  return 0;

 err:
  bgpcorsaro_routingtables_close_output(bgpcorsaro);
  return -1;
}

/** Implements the close_output function of the plugin API */
int bgpcorsaro_routingtables_close_output(bgpcorsaro_t *bgpcorsaro)
{
  int i;
  struct bgpcorsaro_routingtables_state_t *state = STATE(bgpcorsaro);

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

        /** destroy plugin custom variables */
      routingtables_destroy(state->routing_tables);
      state->routing_tables = NULL;
      if(state->metric_prefix != NULL)
        {
          free(state->metric_prefix);
        }
      state->metric_prefix = NULL;
#ifdef WITH_BGPWATCHER
      if(state->watcher_server_uri != NULL)
        {
          free(state->watcher_server_uri);
        }
      state->watcher_server_uri = NULL;
      if(state->watcher_client_id != NULL)
        {
          free(state->watcher_client_id);
        }
      state->watcher_client_id = NULL;
#endif
  
      bgpcorsaro_plugin_free_state(bgpcorsaro->plugin_manager, PLUGIN(bgpcorsaro));
    }
  return 0;
}

/** Implements the start_interval function of the plugin API */
int bgpcorsaro_routingtables_start_interval(bgpcorsaro_t *bgpcorsaro,
                                            bgpcorsaro_interval_t *int_start)
{
  struct bgpcorsaro_routingtables_state_t *state = STATE(bgpcorsaro);

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
  if(routingtables_interval_start(state->routing_tables, int_start->time) < 0)
    {
      // an error occurred during the interval_end operations
      bgpcorsaro_log(__func__, bgpcorsaro, "could not start interval for %s plugin",
		     PLUGIN(bgpcorsaro)->name);      
      return -1;
    }

  bgpcorsaro_io_write_interval_start(bgpcorsaro, state->outfile, int_start);

  return 0;
}

/** Implements the end_interval function of the plugin API */
int bgpcorsaro_routingtables_end_interval(bgpcorsaro_t *bgpcorsaro,
                                          bgpcorsaro_interval_t *int_end)
{
  struct bgpcorsaro_routingtables_state_t *state = STATE(bgpcorsaro);
  
  bgpcorsaro_log(__func__, bgpcorsaro, "Dumping stats for interval %d",
		 int_end->number);
  
  /** plugin end of interval operations */
  if(routingtables_interval_end(state->routing_tables, int_end->time) < 0)
    {
      // an error occurred during the interval_end operations
      bgpcorsaro_log(__func__, bgpcorsaro, "could not end interval for %s plugin",
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
int bgpcorsaro_routingtables_process_record(bgpcorsaro_t *bgpcorsaro,
                                            bgpcorsaro_record_t *record)
{
  struct bgpcorsaro_routingtables_state_t *state = STATE(bgpcorsaro);
  bgpstream_record_t * bs_record = BS_REC(record);

  /* no point carrying on if a previous plugin has already decided we should
     ignore this record */
  if((record->state.flags & BGPCORSARO_RECORD_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }
  record->state.shared_view_ptr = routingtables_get_view_ptr(state->routing_tables);
    
  return routingtables_process_record(state->routing_tables, bs_record);
}
