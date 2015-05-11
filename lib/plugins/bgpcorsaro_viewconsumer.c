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

#include "bgpcorsaro_viewconsumer.h"

#include <time.h>       /* time */

/* this must be all we include from bgpwatcher */
#include "bgpwatcher_view.h"
#include "bgpwatcher_common.h"
#include "bgpwatcher_consumer_manager.h"


/** @file
 *
 * @brief Bgpcorsaro ViewConsumer plugin implementation
 *
 * @author Chiara Orsini
 *
 */

/** The name of this plugin */
#define PLUGIN_NAME "viewconsumer"

/** The version of this plugin */
#define PLUGIN_VERSION "0.1"

#define BUFFER_LEN 1024
#define DEFAULT_METRIC_PREFIX "bgp"
#define DEFAULT_INTEREST BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL


/** Common plugin information across all instances */
static bgpcorsaro_plugin_t bgpcorsaro_viewconsumer_plugin = {
  PLUGIN_NAME,                                               /* name */
  PLUGIN_VERSION,                                            /* version */
  BGPCORSARO_PLUGIN_ID_VIEWCONSUMER,                        /* id */
  BGPCORSARO_PLUGIN_GENERATE_PTRS(bgpcorsaro_viewconsumer), /* func ptrs */
  BGPCORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct bgpcorsaro_viewconsumer_state_t {
  /* plugin custom variables */
  bw_consumer_manager_t *manager;
  bgpwatcher_view_t *shared_view;

  /** metric prefix for consumer manager */
  char metric_prefix[BUFFER_LEN];
};

/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, viewconsumer, BGPCORSARO_PLUGIN_ID_VIEWCONSUMER))

/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_VIEWCONSUMER))


/** Print usage information to stderr */
static void consumer_usage(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_viewconsumer_state_t *state = STATE(bgpcorsaro);

  assert(state->manager != NULL);
  bwc_t **avail_consumers = NULL;
  int i;

  /* get the available consumers from the manager */
  avail_consumers = bw_consumer_manager_get_all_consumers(state->manager);

  fprintf(stderr,
	  "                               available consumers:\n");
  for(i = 0; i < BWC_ID_LAST; i++)
    {
      /* skip unavailable consumers */
      if(avail_consumers[i] == NULL)
	{
	  continue;
	}

      assert(bwc_get_name(avail_consumers[i]));
      fprintf(stderr,
	      "                                - %s\n",
	      bwc_get_name(avail_consumers[i]));
    }
}


/** Print usage information to stderr */
static void usage(bgpcorsaro_t *bgpcorsaro)
{
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_viewconsumer_state_t *state = STATE(bgpcorsaro);

  fprintf(stderr,
          "plugin usage: %s [<options>]\n"
	  "       -m <prefix>        metric prefix (default: %s)\n"
          "       -c <consumer>      Consumer to active (can be used multiple times)\n"
          , // end of options
          plugin->argv[0],
          state->metric_prefix);
  consumer_usage(bgpcorsaro);

}

/** Parse the arguments given to the plugin */
static int parse_args(bgpcorsaro_t *bgpcorsaro)
{
  int opt;
  int prevoptind;

  /* to store command line argument values */
  char *consumer_cmds[BWC_ID_LAST];
  int consumer_cmds_cnt = 0;
  int i;

  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  struct bgpcorsaro_viewconsumer_state_t *state = STATE(bgpcorsaro);

  if(plugin->argc <= 0)
    {
      return 0;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  /* parsing args */
  while(prevoptind = optind,
        (opt = getopt(plugin->argc, plugin->argv, ":c:m:?")) >= 0)
    {
      if (optind == prevoptind + 2 && *optarg == '-' ) {
        opt = ':';
        -- optind;
      }
      switch(opt)
	{
	case ':':
	  fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
	  usage(bgpcorsaro);
	  return -1;
	  break;

        case 'm':
	  strcpy(state->metric_prefix,optarg);
	  break;

	case 'c':
	  if(consumer_cmds_cnt >= BWC_ID_LAST)
	    {
	      fprintf(stderr, "ERROR: At most %d consumers can be enabled\n",
		      BWC_ID_LAST);
	      usage(bgpcorsaro);
	      return -1;
	    }
	  consumer_cmds[consumer_cmds_cnt++] = optarg;
	  break;

	case '?':
	  usage(bgpcorsaro);
	  return -1;
	  break;

	default:
	  usage(bgpcorsaro);
	  return -1;
	  break;
	}
    }

  bw_consumer_manager_set_metric_prefix(state->manager, state->metric_prefix);

  if(consumer_cmds_cnt == 0)
    {
      fprintf(stderr,
	      "ERROR: Consumer(s) must be specified using -c\n");
      usage(bgpcorsaro);
      return -1;
    }

  for(i=0; i<consumer_cmds_cnt; i++)
    {
      assert(consumer_cmds[i] != NULL);
      if(bw_consumer_manager_enable_consumer_from_str(state->manager,
						      consumer_cmds[i]) == NULL)
        {
          usage(bgpcorsaro);
          return -1;
        }
    }

  return 0;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
bgpcorsaro_plugin_t *bgpcorsaro_viewconsumer_alloc(bgpcorsaro_t *bgpcorsaro)
{
  return &bgpcorsaro_viewconsumer_plugin;
}

/** Implements the init_output function of the plugin API */
int bgpcorsaro_viewconsumer_init_output(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_viewconsumer_state_t *state;
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  assert(plugin != NULL);

  if((state =
      malloc_zero(sizeof(struct bgpcorsaro_viewconsumer_state_t))) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not malloc bgpcorsaro_viewconsumer_state_t");
      goto err;
    }

  /** initialize plugin custom variables */

    /* better just grab a pointer to the manager */
  if((state->manager =
      bw_consumer_manager_create(bgpcorsaro->timeseries)) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not initialize consumer manager");
      goto err;
    }

  strcpy(state->metric_prefix, DEFAULT_METRIC_PREFIX);
  state->shared_view = NULL;

  bgpcorsaro_plugin_register_state(bgpcorsaro->plugin_manager, plugin, state);

  /* parse the arguments and update state */
  if(parse_args(bgpcorsaro) != 0)
    {
      goto err;
    }

  return 0;

 err:
  bgpcorsaro_viewconsumer_close_output(bgpcorsaro);
  return -1;
}

/** Implements the close_output function of the plugin API */
int bgpcorsaro_viewconsumer_close_output(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_viewconsumer_state_t *state = STATE(bgpcorsaro);

  if(state != NULL)
    {
      /** destroy plugin custom variables */
      bw_consumer_manager_destroy(&state->manager);
      state->shared_view = NULL;
      bgpcorsaro_plugin_free_state(bgpcorsaro->plugin_manager,
                                   PLUGIN(bgpcorsaro));
    }
  return 0;
}

/** Implements the start_interval function of the plugin API */
int bgpcorsaro_viewconsumer_start_interval(bgpcorsaro_t *bgpcorsaro,
                                            bgpcorsaro_interval_t *int_start)
{
  /* no operation required, this plugin works
   * at the end of the interval  */
  return 0;
}

/** Implements the end_interval function of the plugin API */
int bgpcorsaro_viewconsumer_end_interval(bgpcorsaro_t *bgpcorsaro,
                                         bgpcorsaro_interval_t *int_end)
{
  struct bgpcorsaro_viewconsumer_state_t *state = STATE(bgpcorsaro);

  bgpcorsaro_log(__func__, bgpcorsaro, "Computing stats for interval %d",
		 int_end->number);

  /** plugin end of interval operations */
  if(state->shared_view == 0)
    {
      return 0;
    }

  if(bw_consumer_manager_process_view(state->manager,
                                      DEFAULT_INTEREST,
                                      state->shared_view) != 0)
    {
      // an error occurred during the interval_end operations
      bgpcorsaro_log(__func__, bgpcorsaro,
                     "could not end interval for %s plugin, time %d",
                     PLUGIN(bgpcorsaro)->name,
                     bgpwatcher_view_get_time(state->shared_view));
      return -1;
    }

  return 0;
}

/** Implements the process_record function of the plugin API */
int bgpcorsaro_viewconsumer_process_record(bgpcorsaro_t *bgpcorsaro,
                                           bgpcorsaro_record_t *record)
{
  struct bgpcorsaro_viewconsumer_state_t *state = STATE(bgpcorsaro);
  state->shared_view = record->state.shared_view_ptr;

  /* no other operation required, this plugin works
   * at the end of the interval  */

  return 0;
}
