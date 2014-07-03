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

#include "utils.h"

#include "corsaro_io.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"
#include "corsaro_tag.h"

#include "corsaro_filterbpf.h"

/** @file
 *
 * @brief Corsaro BPF filter plugin
 *
 * While the corsaro tool supports filtering packets using a BPF, this plugin
 * allows packets to be filtered part-way through a chain. For example, this
 * could be used to write all packets out to a flowtuple file, and then only a
 * subset are sent the reporting plugin by doing something like: `corsaro -p
 * flowtuple -p filterbpf -p report`.
 *
 * @note this plugin does not support processing flowtuple files
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "FBPF" */
#define CORSARO_ANON_MAGIC 0x46425046

/** The name of this plugin */
#define PLUGIN_NAME "filterbpf"

/** The max number of BPF which can be supplied on the command line,
    if you have more than this, go away... */
#define MAX_COMMAND_LINE_BPF 100

#define BPF(x) (libtrace_filter_t *)((x)->user)

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_filterbpf_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_FILTERBPF,                      /* id */
  CORSARO_ANON_MAGIC,                          /* magic */
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_filterbpf),
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct corsaro_filterbpf_state_t {
  /** The BPFs explicitly given on the command line*/
  corsaro_tag_t *cmd_bpf[MAX_COMMAND_LINE_BPF];

  /** The number of BPF given on the command line */
  int cmd_bpf_cnt;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, filterbpf, CORSARO_PLUGIN_ID_FILTERBPF))

/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_FILTERBPF))

/** Print usage information to stderr */
static void usage(corsaro_plugin_t *plugin)
{
  fprintf(stderr,
	  "plugin usage: %s -f filter [-f filter]\n"
	  "       -f            BPF filter to apply.\n"
	  "                     -f can be used up to %d times.\n"
	  "                     If more than one filter is supplied, filters "
	  "must be given\n"
	  "                     a unique identifier by prepending the filter "
	  "string with\n"
	  "                     '[<group>.]<name>:'. For example, the filter 'tcp or "
	  "udp' becomes\n"
	  "                     'my_group.my_filter:tcp or udp'\n",
	  plugin->argv[0],
	  MAX_COMMAND_LINE_BPF);
}

static int create_filter(corsaro_t *corsaro,
			 struct corsaro_filterbpf_state_t *state,
			 char *filter_str)
{
  corsaro_tag_group_t *group = NULL;
  corsaro_tag_t *tag = NULL;
  libtrace_filter_t *bpf_filter = NULL;
  char *filter_str_group = NULL;
  char *filter_str_name = NULL;
  char *filter_str_bpf = NULL;

  /* first, check if we were given a name */
  if((filter_str_bpf = strchr(filter_str, ':')) != NULL)
    {
      *filter_str_bpf = '\0';
      filter_str_bpf++;
      /* now, check if we were given a group */
      if((filter_str_name = strchr(filter_str, '.')) != NULL)
	{
	  *filter_str_name = '\0';
	  filter_str_name++;
	  filter_str_group = filter_str;
	}
      else
	{
	  /* no group */
	  filter_str_name = filter_str;
	}
    }
  else
    {
      /* we concoct a name, but only once! */
      filter_str_name = "filterbpf";
      filter_str_bpf = filter_str;
    }

  assert(strlen(filter_str_name) > 0);
  assert(strlen(filter_str_bpf) > 0);

  corsaro_log(__func__, corsaro, "creating tag with group '%s', name '%s' and bpf '%s'",
	      filter_str_group, filter_str_name, filter_str_bpf);

  /* if the group string is not null, then we need to either create a group, or
     get an existing group with that name.  luckily we can just ask for a new
     group and we will be given the old group if it exists */
  if(filter_str_group != NULL &&
     (group = corsaro_tag_group_init(corsaro, filter_str_group,
				     CORSARO_TAG_GROUP_MATCH_MODE_ANY, NULL))
     == NULL)
    {
      fprintf(stderr, "ERROR: could not create group for %s.\n", filter_str_group);
      return -1;
    }

  bpf_filter = trace_create_filter(filter_str_bpf);
  assert(bpf_filter != NULL);
  if((tag = corsaro_tag_init(corsaro, filter_str_name, bpf_filter)) == NULL)
    {
      fprintf(stderr, "ERROR: could not allocate tag for %s.\n", filter_str_bpf);
      fprintf(stderr, "ERROR: ensure all filters are uniquely named\n");
      return -1;
    }
  state->cmd_bpf[state->cmd_bpf_cnt] = tag;
  state->cmd_bpf_cnt++;

  if(group != NULL &&
     corsaro_tag_group_add_tag(group, tag) != 0)
    {
      fprintf(stderr, "ERROR: could not add tag '%s' to group '%s'.\n",
	      filter_str_name, filter_str_group);
      return -1;
    }

  return 0;
}

/** Parse the arguments given to the plugin */
static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_filterbpf_state_t *state = STATE(corsaro);
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
	  if(state->cmd_bpf_cnt == MAX_COMMAND_LINE_BPF)
	    {
	      fprintf(stderr, "ERROR: A maximum of %d filters can be "
		      "specified using the -f option.\n",
		      MAX_COMMAND_LINE_BPF);
	      usage(plugin);
	      return -1;
	    }

	  if(create_filter(corsaro, state, optarg) != 0)
	    {
	      return -1;
	    }
	  break;

	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  if(state->cmd_bpf_cnt == 0)
    {
      fprintf(stderr, "ERROR: %s requires a filter to be specified using '-f'\n",
	      plugin->argv[0]);
      usage(plugin);
      return -1;
    }

  return 0;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_filterbpf_alloc(corsaro_t *corsaro)
{
  return &corsaro_filterbpf_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_filterbpf_probe_filename(const char *fname)
{
  /* this does not write files */
  return 0;
}

/** Implements the probe_magic function of the plugin API */
int corsaro_filterbpf_probe_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  /* this does not write files */
  return 0;
}

/** Implements the init_output function of the plugin API */
int corsaro_filterbpf_init_output(corsaro_t *corsaro)
{
  struct corsaro_filterbpf_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);

  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_filterbpf_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro,
		"could not malloc corsaro_filterbpf_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      /* parse args calls usage itself, so do not goto err here */
      return -1;
    }

  /* just to be safe */
  assert(state->cmd_bpf_cnt > 0);

  return 0;

 err:
  corsaro_filterbpf_close_output(corsaro);
  return -1;
}

/** Implements the init_input function of the plugin API */
int corsaro_filterbpf_init_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_filterbpf_close_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_output function of the plugin API */
int corsaro_filterbpf_close_output(corsaro_t *corsaro)
{
  int i;
  struct corsaro_filterbpf_state_t *state = STATE(corsaro);

  assert(state != NULL);

  if(state->cmd_bpf != NULL)
    {
      for(i=0; i<state->cmd_bpf_cnt; i++)
	{
	  if(state->cmd_bpf[i] != NULL)
	    {
	      trace_destroy_filter(state->cmd_bpf[i]->user);
	      state->cmd_bpf[i]->user = NULL;

	      /* the tag manger will free the tags and groups for us */
	      state->cmd_bpf[i] = NULL;
	    }
	}

      state->cmd_bpf_cnt = 0;
    }

  corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));

  return 0;
}

/** Implements the read_record function of the plugin API */
off_t corsaro_filterbpf_read_record(struct corsaro_in *corsaro,
			       corsaro_in_record_type_t *record_type,
			       corsaro_in_record_t *record)
{
  assert(0);
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_filterbpf_read_global_data_record(struct corsaro_in *corsaro,
			      enum corsaro_in_record_type *record_type,
			      struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_filterbpf_start_interval(corsaro_t *corsaro,
				corsaro_interval_t *int_start)
{
  /* we do not care */
  return 0;
}

/** Implements the end_interval function of the plugin API */
int corsaro_filterbpf_end_interval(corsaro_t *corsaro,
				corsaro_interval_t *int_end)
{
  /* we do not care */
  return 0;
}

/** Implements the process_packet function of the plugin API */
int corsaro_filterbpf_process_packet(corsaro_t *corsaro,
				     corsaro_packet_t *packet)
{
  struct corsaro_filterbpf_state_t *fb_state = STATE(corsaro);
  int i;
  int rc;
  int ignore = 1;

  /* now that other plugins can differentiate between tags that we apply, we
     must apply all filters to every packet. we then ask the tag framework
     to record those that we have matches for */

  for(i=0; i<fb_state->cmd_bpf_cnt; i++)
    {
      assert(fb_state->cmd_bpf[i] != NULL &&
	     BPF(fb_state->cmd_bpf[i]) != NULL);
      rc = trace_apply_filter(BPF(fb_state->cmd_bpf[i]), LT_PKT(packet));
      if(rc < 0)
	{
	  corsaro_log(__func__, corsaro, "invalid bpf filter (%s)",
		      fb_state->cmd_bpf[i]);
	  return -1;
	}
      if(rc > 0)
	{
	  /* mark this filter as a match */
	  corsaro_tag_set_match(&packet->state, fb_state->cmd_bpf[i], rc);
	  /* turn off the ignore flag */
	  ignore = 0;
	}
    }

  /* flip on the ignore bit if any of the filters match */
  if(ignore != 0)
    {
      packet->state.flags |= CORSARO_PACKET_STATE_FLAG_IGNORE;
    }

  return 0;
}
