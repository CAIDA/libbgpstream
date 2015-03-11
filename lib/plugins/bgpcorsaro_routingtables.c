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

  // @todo custom variables
};

/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, routingtables, BGPCORSARO_PLUGIN_ID_ROUTINGTABLES))

/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_ROUTINGTABLES))


/** Print usage information to stderr */
static void usage(bgpcorsaro_plugin_t *plugin)
{
  /** @todo modify */
  fprintf(stderr,
	  "plugin usage: %s [-w interval-lenght] -a\n"
	  "       -w interval-lenght  (default: 30s)\n"
	  "       -a                  adaptive (default:off) \n",
	  plugin->argv[0]);
}

/** Parse the arguments given to the plugin */
static int parse_args(bgpcorsaro_t *bgpcorsaro)
{
  /** @todo modify */
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  // struct bgpcorsaro_routingtables_state_t *state = STATE(bgpcorsaro);
  int opt;

  if(plugin->argc <= 0)
    {
      return 0;
    }
  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;
  /* parsing args */
  while((opt = getopt(plugin->argc, plugin->argv, "?")) >= 0)
    {
      switch(opt)
	{
	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }
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
  bgpcorsaro_plugin_register_state(bgpcorsaro->plugin_manager, plugin, state);


  // initializing state
  state = STATE(bgpcorsaro);

  /* parse the arguments */
  if(parse_args(bgpcorsaro) != 0)
    {
      return -1;
    }

  // @todo update state with parsed args
  
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

  bgpcorsaro_io_write_interval_start(bgpcorsaro, state->outfile, int_start);

  // @todo start interval
  
  return 0;
}

/** Implements the end_interval function of the plugin API */
int bgpcorsaro_routingtables_end_interval(bgpcorsaro_t *bgpcorsaro,
				 bgpcorsaro_interval_t *int_end)
{
  struct bgpcorsaro_routingtables_state_t *state = STATE(bgpcorsaro);

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

  // @todo end interval

  return 0;
}

/** Implements the process_record function of the plugin API */
int bgpcorsaro_routingtables_process_record(bgpcorsaro_t *bgpcorsaro,
				   bgpcorsaro_record_t *record)
{

  /* no point carrying on if a previous plugin has already decided we should
     ignore this record */
  if((record->state.flags & BGPCORSARO_RECORD_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }

  // @todo new record available for processing


  return 0;
}
