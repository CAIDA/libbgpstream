/*
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "corsaro_int.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libtrace.h"
#include <libipmeta.h>

#include "khash.h"
#include "utils.h"

#include "corsaro_ipmeta.h"
#include "corsaro_io.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

#include "corsaro_filtergeo.h"

/** @file
 *
 * @brief Corsaro FlowTuple Reporting plugin
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "FGEO" */
#define CORSARO_ANON_MAGIC 0x4647454F

/** The name of this plugin */
#define PLUGIN_NAME "filtergeo"

/** The length of the static line buffer */
#define BUFFER_LEN 1024

/** The max number of countries which can be supplied on the command line,
    if you have more than this, just use a file... */
#define MAX_COMMAND_LINE_COUNTRIES 100

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_filtergeo_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_FILTERGEO,                      /* id */
  CORSARO_ANON_MAGIC,                          /* magic */
#ifdef WITH_PLUGIN_SIXT
  CORSARO_PLUGIN_GENERATE_PTRS_FT(corsaro_filtergeo),  /* func ptrs */
#else
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_filtergeo),
#endif
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/* to contain the list of country codes that we will filter on */
KHASH_SET_INIT_STR(country)

/** Holds the state for an instance of this plugin */
struct corsaro_filtergeo_state_t {
  /** we need a hash of countries that point to maps of source ips etc */
  khash_t(country) *countries;

  /** The file to read country codes from */
  char *country_file;

  /** The countries explicitly given on the command line*/
  char *cmd_countries[MAX_COMMAND_LINE_COUNTRIES];

  /** The number of countries given on the command line */
  int cmd_country_cnt;

  /** Invert the matching. I.e. Only include packets which DO NOT fall into any
      of the prefixes */
  uint8_t invert;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, filtergeo, CORSARO_PLUGIN_ID_FILTERGEO))

/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_FILTERGEO))

/** Print usage information to stderr */
static void usage(corsaro_plugin_t *plugin)
{
  fprintf(stderr,
	  "plugin usage: %s [-di] [-c country [-p country]] [-f country_file]\n"
	  "       -c            country code to match against, -c can be used "
	  "up to %d times\n"
	  "                     Note: use 2 character ISO 3166-1 alpha-2 codes\n"
	  "       -f            read countries from the given file\n"
	  "       -i            invert the matching (default: find matches)\n",
	  plugin->argv[0],
	  MAX_COMMAND_LINE_COUNTRIES);
}

/** Parse the arguments given to the plugin */
static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_filtergeo_state_t *state = STATE(corsaro);
  int opt;

  /* remember the storage for the argv strings belongs to us, we don't need
     to strdup them */

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, "c:f:i?")) >= 0)
    {
      switch(opt)
	{

	case 'f':
	  state->country_file = optarg;
	  break;

	case 'i':
	  state->invert = 1;
	  break;

	case 'c':
	  if(state->cmd_country_cnt == MAX_COMMAND_LINE_COUNTRIES)
	    {
	      fprintf(stderr, "ERROR: A maximum of %d countries can be "
		      "specified using the -c option.\n"
		      "Consider using the -f option instead\n",
		      MAX_COMMAND_LINE_COUNTRIES);
	      usage(plugin);
	      return -1;
	    }
	  state->cmd_countries[state->cmd_country_cnt] = optarg;
	  state->cmd_country_cnt++;
	  break;

	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  if(state->country_file == NULL && state->cmd_country_cnt == 0)
    {
      fprintf(stderr, "ERROR: %s requires either '-f' or '-c' to be specified\n",
	      plugin->argv[0]);
      usage(plugin);
      return -1;
    }

  if(state->country_file != NULL && state->cmd_country_cnt > 0)
    {
      fprintf(stderr,
	      "WARNING: both -f and -c used, all specified countries "
	      "will be used\n");
    }

  return 0;
}

/** Parse a country code string and add it to the hash */
static int add_country(corsaro_t *corsaro, char *cc_str)
{
  int khret;

  /* a country code needs to be two characters long */
  /* lets be kind */
  if(strnlen(cc_str, BUFFER_LEN) != 2)
    {
      corsaro_log(__func__, corsaro, "Invalid country code %s",
		  cc_str);
      return -1;
    }

  /* is it already in the hash ? */
  if(kh_get(country, STATE(corsaro)->countries, cc_str)
     == kh_end(STATE(corsaro)->countries))
    {
      kh_put(country, STATE(corsaro)->countries, strndup(cc_str, 2), &khret);
    }

  return 0;
}

/** Read a file containing a list of country codes */
static int read_country_file(corsaro_t *corsaro, corsaro_file_in_t *file)
{
  char buffer[BUFFER_LEN];

  while(corsaro_file_rgets(file, &buffer, BUFFER_LEN) > 0)
    {
      /* hack off the newline */
      chomp(buffer);

      if(strnlen(buffer, BUFFER_LEN) == 0)
	{
	  continue;
	}

      if(add_country(corsaro, buffer) != 0)
	{
	  return -1;
	}
    }

  return 0;
}

/** Common code between process_packet and process_flowtuple */
static void process_generic(corsaro_t *corsaro, corsaro_packet_state_t *state)
{
  struct corsaro_filtergeo_state_t *fg_state = STATE(corsaro);
  ipmeta_record_t *record = NULL;
  char *country = "--";

  if((record = corsaro_ipmeta_get_default_record(state)) != NULL)
    {
      /* check the country */
      if(record->country_code != NULL)
	{
	  country = record->country_code;
	}
    }

  /* now, country will either be "--", or a country code */

  if(kh_get(country, fg_state->countries, country)
     == kh_end(fg_state->countries))
    {
      /* this country is NOT in the hash */
      if(fg_state->invert == 0)
	{
	  state->flags |= CORSARO_PACKET_STATE_IGNORE;
	}
    }
  else
    {
      /* this country IS in the hash */
      if(fg_state->invert != 0)
	{
	  state->flags |= CORSARO_PACKET_STATE_IGNORE;
	}
    }

  return;
}

/** Free a string (used to clear the hash) */
static inline void str_free(const char *str)
{
  free((char*)str);
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_filtergeo_alloc(corsaro_t *corsaro)
{
  return &corsaro_filtergeo_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_filtergeo_probe_filename(const char *fname)
{
  /* this does not write files */
  return 0;
}

/** Implements the probe_magic function of the plugin API */
int corsaro_filtergeo_probe_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  /* this does not write files */
  return 0;
}

/** Implements the init_output function of the plugin API */
int corsaro_filtergeo_init_output(corsaro_t *corsaro)
{
  struct corsaro_filtergeo_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  corsaro_file_in_t *file = NULL;
  int i;

  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_filtergeo_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro,
		"could not malloc corsaro_filtergeo_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      /* parse args calls usage itself, so do not goto err here */
      return -1;
    }

  state->countries = kh_init(country);

  /* read in countries from country_file (if there is one) */
  if(state->country_file != NULL)
    {
      if((file = corsaro_file_ropen(state->country_file)) == NULL)
	{
	  corsaro_log(__func__, corsaro,
		      "failed to open country file '%s'", state->country_file);

	  goto err;
	}

      if(read_country_file(corsaro, file) != 0)
	{
	  corsaro_log(__func__, corsaro,
		      "failed to read country file '%s'", state->country_file);
	  goto err;
	}

      /* close the country file */
      corsaro_file_rclose(file);
      file = NULL;
    }

  /* add the countries that have been manually specified */
  for(i = 0; i < state->cmd_country_cnt; i++)
    {
      if(add_country(corsaro, state->cmd_countries[i]) != 0)
	{
	  goto err;
	}
    }

  return 0;

 err:
  corsaro_filtergeo_close_output(corsaro);
  return -1;
}

/** Implements the init_input function of the plugin API */
int corsaro_filtergeo_init_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_filtergeo_close_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_output function of the plugin API */
int corsaro_filtergeo_close_output(corsaro_t *corsaro)
{
  struct corsaro_filtergeo_state_t *state = STATE(corsaro);

  if(state != NULL)
    {
      if(state->countries != NULL)
	{
	  kh_free(country, state->countries, str_free);
	  kh_destroy(country, state->countries);
	  state->countries = NULL;
	}

      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }

  return 0;
}

/** Implements the read_record function of the plugin API */
off_t corsaro_filtergeo_read_record(struct corsaro_in *corsaro,
			       corsaro_in_record_type_t *record_type,
			       corsaro_in_record_t *record)
{
  assert(0);
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_filtergeo_read_global_data_record(struct corsaro_in *corsaro,
			      enum corsaro_in_record_type *record_type,
			      struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_filtergeo_start_interval(corsaro_t *corsaro,
				corsaro_interval_t *int_start)
{
  /* we do not care */
  return 0;
}

/** Implements the end_interval function of the plugin API */
int corsaro_filtergeo_end_interval(corsaro_t *corsaro,
				corsaro_interval_t *int_end)
{
  /* we do not care */
  return 0;
}

/** Implements the process_packet function of the plugin API */
int corsaro_filtergeo_process_packet(corsaro_t *corsaro,
				corsaro_packet_t *packet)
{
  process_generic(corsaro, &packet->state);
  return 0;
}

#ifdef WITH_PLUGIN_SIXT
/** Implements the process_flowtuple function of the plugin API */
int corsaro_filtergeo_process_flowtuple(corsaro_t *corsaro,
					corsaro_flowtuple_t *flowtuple,
					corsaro_packet_state_t *state)
{
  process_generic(corsaro, state);
  return 0;
}

/** Implements the process_flowtuple_class_start function of the plugin API */
int corsaro_filtergeo_process_flowtuple_class_start(corsaro_t *corsaro,
				   corsaro_flowtuple_class_start_t *class)
{
  /* we dont care about these */
  return 0;
}

/** Implements the process_flowtuple_class_end function of the plugin API */
int corsaro_filtergeo_process_flowtuple_class_end(corsaro_t *corsaro,
				   corsaro_flowtuple_class_end_t *class)
{
  /* dont care */
  return 0;
}
#endif
