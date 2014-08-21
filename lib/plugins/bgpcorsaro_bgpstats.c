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

#include "bgpcorsaro_bgpstats.h"

/** @file
 *
 * @brief Bgpcorsaro BgpStats plugin implementation
 *
 * @author Chiara Orsini
 *
 */

/** The number of output file pointers to support non-blocking close at the end
    of an interval. If the wandio buffers are large enough that it takes more
    than 1 interval to drain the buffers, consider increasing this number */
#define OUTFILE_POINTERS 2

/** The name of this plugin */
#define PLUGIN_NAME "bgpstats"

/** The version of this plugin */
#define PLUGIN_VERSION "0.1"

/** Common plugin information across all instances */
static bgpcorsaro_plugin_t bgpcorsaro_bgpstats_plugin = {
  PLUGIN_NAME,                                          /* name */
  PLUGIN_VERSION,                                       /* version */
  BGPCORSARO_PLUGIN_ID_BGPSTATS,                        /* id */
  BGPCORSARO_PLUGIN_GENERATE_PTRS(bgpcorsaro_bgpstats), /* func ptrs */
  BGPCORSARO_PLUGIN_GENERATE_TAIL,
};


struct bgpcorsaro_bgpstats_bgpdatainfo_t {
  /** number of records read for each type */
  int num_records[BGPSTREAM_RECORD_TYPE_MAX];
};



/** Holds the state for an instance of this plugin */
struct bgpcorsaro_bgpstats_state_t {
  /** The outfile for the plugin */
  iow_t *outfile;
  /** A set of pointers to outfiles to support non-blocking close */
  iow_t *outfile_p[OUTFILE_POINTERS];
  /** The current outfile */
  int outfile_n;
  /** bgp information related to the current interval */ 
  struct bgpcorsaro_bgpstats_bgpdatainfo_t * bgpdatainfo;
};

/** Extends the generic plugin state convenience macro in bgpcorsaro_plugin.h */
#define STATE(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_STATE(bgpcorsaro, bgpstats, BGPCORSARO_PLUGIN_ID_BGPSTATS))
/** Extends the generic plugin plugin convenience macro in bgpcorsaro_plugin.h */
#define PLUGIN(bgpcorsaro)						\
  (BGPCORSARO_PLUGIN_PLUGIN(bgpcorsaro, BGPCORSARO_PLUGIN_ID_BGPSTATS))

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
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);
  int opt;
  //TODO modify this

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


/* == STATIC struct bgpdatainfo_t RELATED FUNCTIOS == */

/** struct bgpdatainfo_t create */
static struct bgpcorsaro_bgpstats_bgpdatainfo_t * bgpcorsaro_bgpstats_bgpdatainfo_create() 
{
  struct bgpcorsaro_bgpstats_bgpdatainfo_t * info = malloc_zero(sizeof(struct bgpcorsaro_bgpstats_bgpdatainfo_t));
  return info;
}

/** struct bgpdatainfo_t reset (at beginning of interval) */
static void bgpcorsaro_bgpstats_bgpdatainfo_reset(struct bgpcorsaro_bgpstats_bgpdatainfo_t * info) 
{
  assert(info != NULL);
  memset(info->num_records, 0, sizeof(info->num_records));
}

/** struct bgpdatainfo_t update (inside the interval) */
static void bgpcorsaro_bgpstats_bgpdatainfo_update(struct bgpcorsaro_bgpstats_bgpdatainfo_t * info, bgpcorsaro_record_t * record) 
{
  assert(info != NULL);
  assert(record != NULL);
  info->num_records[BS_REC(record)->status] +=1;
}

/** struct bgpdatainfo_t destroy */
static void bgpcorsaro_bgpstats_bgpdatainfo_destroy(struct bgpcorsaro_bgpstats_bgpdatainfo_t * info) 
{
  if(info != NULL)
    {
      free(info);
    }
}


/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
bgpcorsaro_plugin_t *bgpcorsaro_bgpstats_alloc(bgpcorsaro_t *bgpcorsaro)
{
  return &bgpcorsaro_bgpstats_plugin;
}

/** Implements the init_output function of the plugin API */
int bgpcorsaro_bgpstats_init_output(bgpcorsaro_t *bgpcorsaro)
{
  struct bgpcorsaro_bgpstats_state_t *state;
  bgpcorsaro_plugin_t *plugin = PLUGIN(bgpcorsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct bgpcorsaro_bgpstats_state_t))) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not malloc bgpcorsaro_bgpstats_state_t");
      goto err;
    }

  if((state->bgpdatainfo = bgpcorsaro_bgpstats_bgpdatainfo_create()) == NULL)
    {
      bgpcorsaro_log(__func__, bgpcorsaro,
		     "could not malloc bgpcorsaro_bgpstats_bgpdatainfo_t in bgpcorsaro_bgpstats_state_t");
      goto err;
    }

  bgpcorsaro_plugin_register_state(bgpcorsaro->plugin_manager, plugin, state);

  /* parse the arguments */
  if(parse_args(bgpcorsaro) != 0)
    {
      return -1;
    }

  /* defer opening the output file until we start the first interval */

  return 0;

 err:
  bgpcorsaro_bgpstats_close_output(bgpcorsaro);
  return -1;
}

/** Implements the close_output function of the plugin API */
int bgpcorsaro_bgpstats_close_output(bgpcorsaro_t *bgpcorsaro)
{
  int i;
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);

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
      bgpcorsaro_bgpstats_bgpdatainfo_destroy(state->bgpdatainfo);
      bgpcorsaro_plugin_free_state(bgpcorsaro->plugin_manager, PLUGIN(bgpcorsaro));
    }
  return 0;
}

/** Implements the start_interval function of the plugin API */
int bgpcorsaro_bgpstats_start_interval(bgpcorsaro_t *bgpcorsaro,
				   bgpcorsaro_interval_t *int_start)
{
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);
  bgpcorsaro_bgpstats_bgpdatainfo_reset(state->bgpdatainfo);
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

  return 0;
}

/** Implements the end_interval function of the plugin API */
int bgpcorsaro_bgpstats_end_interval(bgpcorsaro_t *bgpcorsaro,
				 bgpcorsaro_interval_t *int_end)
{
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);

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
int bgpcorsaro_bgpstats_process_record(bgpcorsaro_t *bgpcorsaro,
				       bgpcorsaro_record_t *record)
{
  struct bgpcorsaro_bgpstats_state_t *state = STATE(bgpcorsaro);
  
  /* no point carrying on if a previous plugin has already decided we should
     ignore this record */
  if((record->state.flags & BGPCORSARO_RECORD_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }

  assert(state);
  bgpcorsaro_bgpstats_bgpdatainfo_update(state->bgpdatainfo, record);

  return 0;
}
