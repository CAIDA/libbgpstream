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

#include <arpa/inet.h>

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libtrace.h"

#include "patricia.h"
#include "utils.h"

#include "corsaro_log.h"
#include "corsaro_plugin.h"

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

#include "corsaro_filterpfx.h"

/** @file
 *
 * @brief Corsaro Prefix Filter plugin
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "FPFX" */
#define CORSARO_FILTERPFX_MAGIC 0x46504658

/** The name of this plugin */
#define PLUGIN_NAME "filterpfx"

/** The length of the static line buffer */
#define BUFFER_LEN 1024

/** The max number of prefixes which can be supplied on the command line,
    if you have more than this, just use a file... */
#define MAX_COMMAND_LINE_PREFIXES 100

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_filterpfx_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_FILTERPFX,                      /* id */
  CORSARO_FILTERPFX_MAGIC,                          /* magic */
#ifdef WITH_PLUGIN_SIXT
  CORSARO_PLUGIN_GENERATE_PTRS_FT(corsaro_filterpfx),  /* func ptrs */
#else
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_filterpfx),
#endif
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct corsaro_filterpfx_state_t {
  /** The patricia trie to support fast lookups of each address */
  patricia_tree_t *trie;
  /** The file to read prefixes from */
  char *pfx_file;
  /** The prefixes explicitly given on the command line */
  char *cmd_prefixes[MAX_COMMAND_LINE_PREFIXES];
  /** The number of prefixes given on the command line */
  int cmd_prefix_cnt;
  /** Match on the destination address rather than the source */
  int destination;
  /** Invert the matching. I.e. Only include packets which DO NOT fall into any
      of the prefixes */
  int invert;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)							\
  (CORSARO_PLUGIN_STATE(corsaro, filterpfx, CORSARO_PLUGIN_ID_FILTERPFX))

/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)							\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_FILTERPFX))

/** Print usage information to stderr */
static void usage(corsaro_plugin_t *plugin)
{
  fprintf(stderr,
	  "plugin usage: %s [-di] [-p pfx [-p pfx]] [-f pfx_file]\n"
	  "       -d            use destination address (default: source)\n"
	  "       -f            read prefixes from the given file\n"
	  "       -i            invert the matching (default: find matches)\n"
	  "       -p            prefix to match against, -p can be used "
	  "up to %d times\n",
	  plugin->argv[0],
	  MAX_COMMAND_LINE_PREFIXES);
}

/** Parse the arguments given to the plugin */
static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_filterpfx_state_t *state = STATE(corsaro);
  int opt;

  /* remember the storage for the argv strings belongs to us, we don't need
     to strdup them */

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, "p:f:di?")) >= 0)
    {
      switch(opt)
	{
	case 'd':
	  state->destination = 1;

	case 'f':
	  state->pfx_file = optarg;
	  break;

	case 'i':
	  state->invert = 1;
	  break;

	case 'p':
	  if(state->cmd_prefix_cnt == MAX_COMMAND_LINE_PREFIXES)
	    {
	      fprintf(stderr, "ERROR: A maximum of %d prefixes can be "
		      "specified using the -p option.\n"
		      "Consider using the -f option instead\n",
		      MAX_COMMAND_LINE_PREFIXES);
	      usage(plugin);
	      return -1;
	    }
	  state->cmd_prefixes[state->cmd_prefix_cnt] = optarg;
	  state->cmd_prefix_cnt++;
	  break;

	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  if(state->pfx_file == NULL && state->cmd_prefix_cnt == 0)
    {
      fprintf(stderr, "ERROR: %s requires either '-f' or '-p' to be specified\n",
	      plugin->argv[0]);
      usage(plugin);
      return -1;
    }

  if(state->pfx_file != NULL && state->cmd_prefix_cnt > 0)
    {
      fprintf(stderr,
	      "WARNING: both -f and -p used, all specified prefixes "
	      "will be used\n");
    }

  return 0;
}

/** Parse a prefix string and add it to the trie */
static int add_prefix(corsaro_t *corsaro, char *pfx_str)
{
  prefix_t *pfx = NULL;
  patricia_node_t *trie_node = NULL;

  /* no sanity checking, let libpatricia sort it out */
  if((pfx = ascii2prefix(AF_INET, pfx_str)) == NULL)
    {
      /* invalid prefix? */
      /* skip it, or explode? */

      /* explode, but only if asserts are on */
      corsaro_log(__func__, corsaro, "failed to parse prefix '%s'",
		  pfx_str);
      assert(0);
    }

  /* shove it in the trie */
  if((trie_node = patricia_lookup(STATE(corsaro)->trie, pfx)) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to insert prefix in trie");
      return -1;
    }

  return 0;
}

/** Read a file containing a list of prefixes */
static int read_pfx_file(corsaro_t *corsaro, corsaro_file_in_t *file)
{
  char buffer[BUFFER_LEN];

  while(corsaro_file_rgets(file, &buffer, BUFFER_LEN) > 0)
    {
      /* hack off the newline */
      chomp(buffer);

      if(add_prefix(corsaro, buffer) != 0)
	{
	  return -1;
	}
    }

  return 0;
}

/** Common code between process_packet and process_flowtuple */
static int process_generic(corsaro_t *corsaro, corsaro_packet_state_t *state,
			   uint32_t ip_addr)
{
  struct corsaro_filterpfx_state_t *plugin_state = STATE(corsaro);
  patricia_node_t *node = NULL;
  prefix_t pfx;
  pfx.family = AF_INET;
  pfx.bitlen = 32;
  pfx.ref_count = 0;
  pfx.add.sin.s_addr = ip_addr;

  if((node = patricia_search_best2(plugin_state->trie,
				   &pfx, 1)) == NULL)
    {
      /* this address is NOT covered by a prefix */
      if(plugin_state->invert == 0)
	{
	  state->flags |= CORSARO_PACKET_STATE_FLAG_IGNORE;
	}
    }
  else
    {
      if(plugin_state->invert != 0)
	{
	  state->flags |= CORSARO_PACKET_STATE_FLAG_IGNORE;
	}
    }

  return 0;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_filterpfx_alloc(corsaro_t *corsaro)
{
  return &corsaro_filterpfx_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_filterpfx_probe_filename(const char *fname)
{
  /* this writes no files! */
  return 0;
}

/** Implements the probe_magic function of the plugin API */
int corsaro_filterpfx_probe_magic(corsaro_in_t *corsaro,
				    corsaro_file_in_t *file)
{
  /* this writes no files! */
  return 0;
}

/** Implements the init_output function of the plugin API */
int corsaro_filterpfx_init_output(corsaro_t *corsaro)
{
  struct corsaro_filterpfx_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  corsaro_file_in_t *file = NULL;
  int i;

  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_filterpfx_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro,
		  "could not malloc corsaro_maxmind_state_t");
      return -1;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      /* parse args calls usage itself, so do not goto err here */
      return -1;
    }

  /* initialize the trie */
  state->trie = New_Patricia(32);
  assert(state->trie != NULL);

  /* read in prefixes from the pfx_file (if there is one) */
  if(state->pfx_file != NULL)
    {
      if((file = corsaro_file_ropen(state->pfx_file)) == NULL)
	{
	  corsaro_log(__func__, corsaro,
		      "failed to open prefix file '%s'", state->pfx_file);

	  goto err;
	}

      if(read_pfx_file(corsaro, file) != 0)
	{
	  corsaro_log(__func__, corsaro,
		      "failed to read prefix file '%s'", state->pfx_file);
	  goto err;
	}

      /* close the prefix file */
      corsaro_file_rclose(file);
      file = NULL;
    }

  /* add the prefixes that have been manually specified */
  for(i = 0; i < state->cmd_prefix_cnt; i++)
    {
      if(add_prefix(corsaro, state->cmd_prefixes[i]) != 0)
	{
	  goto err;
	}
    }

  return 0;

 err:
  if(file != NULL)
    {
      corsaro_file_rclose(file);
    }
  if(state->trie != NULL)
    {
      Destroy_Patricia(state->trie, NULL);
      state->trie = NULL;
    }
  usage(plugin);
  return -1;
}

/** Implements the init_input function of the plugin API */
int corsaro_filterpfx_init_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_filterpfx_close_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_output function of the plugin API */
int corsaro_filterpfx_close_output(corsaro_t *corsaro)
{
  struct corsaro_filterpfx_state_t *state = STATE(corsaro);
  if(state != NULL)
    {
      if(state->trie != NULL)
	{
	  Destroy_Patricia(state->trie, NULL);
	  state->trie = NULL;
	}
      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }
  return 0;
}

/** Implements the read_record function of the plugin API */
off_t corsaro_filterpfx_read_record(struct corsaro_in *corsaro,
				      corsaro_in_record_type_t *record_type,
				      corsaro_in_record_t *record)
{
  assert(0);
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_filterpfx_read_global_data_record(struct corsaro_in *corsaro,
				  enum corsaro_in_record_type *record_type,
			          struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_filterpfx_start_interval(corsaro_t *corsaro,
				       corsaro_interval_t *int_start)
{
  /* we don't care */
  return 0;
}

/** Implements the end_interval function of the plugin API */
int corsaro_filterpfx_end_interval(corsaro_t *corsaro,
				     corsaro_interval_t *int_end)
{
  /* we don't care */
  return 0;
}

/** Implements the process_packet function of the plugin API */
int corsaro_filterpfx_process_packet(corsaro_t *corsaro,
				       corsaro_packet_t *packet)
{
  libtrace_packet_t *ltpacket = LT_PKT(packet);
  libtrace_ip_t  *ip_hdr  = NULL;
  uint32_t ip_addr;

  /* check for ipv4 */
  if((ip_hdr = trace_get_ip(ltpacket)) == NULL)
    {
      /* not an ip packet */
      return 0;
    }

  ip_addr = (STATE(corsaro)->destination == 0) ? ip_hdr->ip_src.s_addr :
    ip_hdr->ip_dst.s_addr;

  return process_generic(corsaro, &packet->state, ip_addr);
}

#ifdef WITH_PLUGIN_SIXT
/** Implements the process_flowtuple function of the plugin API */
int corsaro_filterpfx_process_flowtuple(corsaro_t *corsaro,
					  corsaro_flowtuple_t *flowtuple,
					  corsaro_packet_state_t *state)
{
  uint32_t ip_addr;

  ip_addr = (STATE(corsaro)->destination == 0) ?
    corsaro_flowtuple_get_source_ip(flowtuple) :
    corsaro_flowtuple_get_destination_ip(flowtuple);

  return process_generic(corsaro, state, ip_addr);
}

/** Implements the process_flowtuple_class_start function of the plugin API */
int corsaro_filterpfx_process_flowtuple_class_start(corsaro_t *corsaro,
					 corsaro_flowtuple_class_start_t *class)
{
  /* we dont care about these */
  return 0;
}

/** Implements the process_flowtuple_class_end function of the plugin API */
int corsaro_filterpfx_process_flowtuple_class_end(corsaro_t *corsaro,
					   corsaro_flowtuple_class_end_t *class)
{
  /* dont care */
  return 0;
}
#endif
