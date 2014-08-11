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

#include "libtrace.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "utils.h"

#include "corsaro_log.h"
#include "corsaro_plugin.h"

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

#include "corsaro_ipmeta.h"

/** @file
 *
 * @brief Corsaro libipmeta wrapper plugin
 *
 * This plugin provides a lightweight wrapper around the libipmeta lookup
 * library. It allows a set of providers to be configured, and then performs a
 * "lookup" operation on the source address of each packet, caching the results
 * for other plugins to retrieve using the corsaro_ipmeta_get_record function.
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "IPMT" */
#define CORSARO_IPMETA_MAGIC 0x49504D54

/** The name of this plugin */
#define PLUGIN_NAME "ipmeta"

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_ipmeta_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_IPMETA,                      /* id */
  CORSARO_IPMETA_MAGIC,                          /* magic */
#ifdef WITH_PLUGIN_SIXT
  CORSARO_PLUGIN_GENERATE_PTRS_FT(corsaro_ipmeta),  /* func ptrs */
#else
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_ipmeta),
#endif
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct corsaro_ipmeta_state_t {
  /** Pointer to a libipmeta instance */
  ipmeta_t *ipmeta;

  /** Array of providers that we use to perform lookups for each packet */
  ipmeta_provider_t *enabled_providers[IPMETA_PROVIDER_MAX];

  /** Number of providers in the enabled_providers array */
  int enabled_providers_cnt;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)							\
  (CORSARO_PLUGIN_STATE(corsaro, ipmeta, CORSARO_PLUGIN_ID_IPMETA))

/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_IPMETA))

/** Print usage information to stderr */
static void usage(corsaro_t *corsaro)
{
  assert(STATE(corsaro)->ipmeta != NULL);
  ipmeta_provider_t **providers = NULL;
  int i;

  fprintf(stderr,
	  "plugin usage: %s -p provider [-p \"provider arg1...argn\"]\n"
	  "       -p <provider> enable the given provider,\n"
	  "                     -p can be used multiple times\n"
	  "                     available providers:\n",
	  PLUGIN(corsaro)->argv[0]);

  /* get the available plugins from ipmeta */
  providers = ipmeta_get_all_providers(STATE(corsaro)->ipmeta);
  for(i = 0; i < IPMETA_PROVIDER_MAX; i++)
    {
      assert(providers[i] != NULL);
      assert(ipmeta_get_provider_name(providers[i]));
      fprintf(stderr, "                      - %s\n",
	      ipmeta_get_provider_name(providers[i]));
    }
}

/** Parse the arguments given to the plugin
 * @todo add option to choose datastructure
 */
static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_ipmeta_state_t *state = STATE(corsaro);
  int opt;

  char *default_provider_name = NULL;
  ipmeta_provider_default_t is_default;

  char *provider_names[IPMETA_PROVIDER_MAX];
  int provider_names_cnt = 0;
  char *provider_arg_ptr = NULL;
  ipmeta_provider_t *provider = NULL;
  int i;

  assert(plugin->argc > 0 && plugin->argv != NULL);

  if(plugin->argc == 1)
    {
      goto err;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, ":d:p:?")) >= 0)
    {
      switch(opt)
	{
	case 'd':
	  default_provider_name = strdup(optarg);
	  break;

	case 'p':
	  provider_names[provider_names_cnt++] = strdup(optarg);
	  break;

	case '?':
	case ':':
	default:
	  usage(corsaro);
	  return -1;
	}
    }

  /* ensure there is at least one provider given */
  if(provider_names_cnt == 0)
    {
      fprintf(stderr,
	      "ERROR: At least one provider must be selected using -p\n");
      goto err;
    }

  for(i=0;i<provider_names_cnt;i++)
    {
      /* the string at provider_names[i] will contain the name of the plugin,
	 optionally followed by a space and then the arguments to pass to the
	 plugin */
      if((provider_arg_ptr = strchr(provider_names[i], ' ')) != NULL)
	{
	  /* set the space to a nul, which allows provider_names[i] to be used
	     for the provider name, and then increment plugin_arg_ptr to point
	     to the next character, which will be the start of the arg string
	     (or at worst case, the terminating \0 */
	  *provider_arg_ptr = '\0';
	  provider_arg_ptr++;
	}

      /* lookup the provider using the name given */
      if((provider =
	  ipmeta_get_provider_by_name(state->ipmeta,
				      provider_names[i])) == NULL)
	{
	  fprintf(stderr, "ERROR: Invalid provider name (%s)\n",
		  provider_names[i]);
	  goto err;
	}

      if(default_provider_name != NULL &&
	 strcmp(default_provider_name, provider_names[i]) == 0)
	{
	  is_default = IPMETA_PROVIDER_DEFAULT_YES;
	}
      else
	{
	  is_default = IPMETA_PROVIDER_DEFAULT_NO;
	}

      if(ipmeta_enable_provider(state->ipmeta, provider,
				IPMETA_DS_DEFAULT,
				provider_arg_ptr,
				is_default) != 0)
	{
	  fprintf(stderr, "ERROR: Could not enable plugin %s\n",
		  provider_names[i]);
	  goto err;
	}

      free(provider_names[i]);
      provider_names[i] = NULL;
      state->enabled_providers[state->enabled_providers_cnt++] = provider;
    }

  if(default_provider_name != NULL)
    {
      free(default_provider_name);
    }

  return 0;

 err:
  if(default_provider_name != NULL)
    {
      free(default_provider_name);
    }
  for(i=0; i <provider_names_cnt; i++)
    {
      if(provider_names[i] != NULL)
	{
	  free(provider_names[i]);
	}
    }
  usage(corsaro);
  return -1;
}

/** Common code between process_packet and process_flowtuple */
static int process_generic(corsaro_t *corsaro, corsaro_packet_state_t *state,
			   uint32_t src_ip)
{
  struct corsaro_ipmeta_state_t *plugin_state = STATE(corsaro);
  ipmeta_provider_t *provider = NULL;
  ipmeta_record_t *record = NULL;
  int i;

  /* ask each enabled provider to do a lookup on this ip */
  for(i = 0; i < plugin_state->enabled_providers_cnt; i++)
    {
      provider = plugin_state->enabled_providers[i];
      assert(provider != NULL);
      record = ipmeta_lookup(provider, src_ip);

      /* set the record in the cache for this provider */
      state->ipmeta_records[ipmeta_get_provider_id(provider)-1] = record;

      /* if this provider is the default, set the cached record accordingly */
      if(provider == ipmeta_get_default_provider(plugin_state->ipmeta))
	{
	  state->ipmeta_record_default = record;
	}
    }

  return 0;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_ipmeta_alloc(corsaro_t *corsaro)
{
  return &corsaro_ipmeta_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_ipmeta_probe_filename(const char *fname)
{
  /* this writes no files! */
  return 0;
}

/** Implements the probe_magic function of the plugin API */
int corsaro_ipmeta_probe_magic(corsaro_in_t *corsaro,
			      corsaro_file_in_t *file)
{
  /* this writes no files! */
  return 0;
}

/** Implements the init_output function of the plugin API */
int corsaro_ipmeta_init_output(corsaro_t *corsaro)
{
  struct corsaro_ipmeta_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);

  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_ipmeta_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro,
		  "could not malloc corsaro_maxmind_state_t");
      return -1;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* initialize libipmeta */
  /* this MUST be done before anything calls usage */
  if((state->ipmeta = ipmeta_init()) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not initialize libipmeta");
      return -1;
    }

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      return -1;
    }

  /* just for sanity */
  assert(state->enabled_providers_cnt > 0);

  /* we're all locked and loaded. the providers are initialized, and we have a
     nice array of the enabled ones that we can spin through for each packet */

  return 0;
}

/** Implements the init_input function of the plugin API */
int corsaro_ipmeta_init_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_ipmeta_close_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_output function of the plugin API */
int corsaro_ipmeta_close_output(corsaro_t *corsaro)
{
  struct corsaro_ipmeta_state_t *state = STATE(corsaro);
  if(state != NULL)
    {
      if(state->ipmeta != NULL)
	{
	  ipmeta_free(state->ipmeta);
	  state->ipmeta = NULL;
	}

      state->enabled_providers_cnt = 0;

      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }
  return 0;
}

/** Implements the read_record function of the plugin API */
off_t corsaro_ipmeta_read_record(struct corsaro_in *corsaro,
				corsaro_in_record_type_t *record_type,
				corsaro_in_record_t *record)
{
  assert(0);
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_ipmeta_read_global_data_record(corsaro_in_t *corsaro,
				     corsaro_in_record_type_t *record_type,
				     corsaro_in_record_t *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_ipmeta_start_interval(corsaro_t *corsaro,
				 corsaro_interval_t *int_start)
{
  /* we don't care */
  return 0;
}

/** Implements the end_interval function of the plugin API */
int corsaro_ipmeta_end_interval(corsaro_t *corsaro,
			       corsaro_interval_t *int_end)
{
  /* we don't care */
  return 0;
}

/** Implements the process_packet function of the plugin API */
int corsaro_ipmeta_process_packet(corsaro_t *corsaro,
				 corsaro_packet_t *packet)
{
  libtrace_packet_t *ltpacket = LT_PKT(packet);
  libtrace_ip_t  *ip_hdr  = NULL;

  /* no point carrying on if a previous plugin has already decided we should
     ignore this packet */
  if((packet->state.flags & CORSARO_PACKET_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }

  /* check for ipv4 */
  if((ip_hdr = trace_get_ip(ltpacket)) == NULL)
    {
      /* not an ip packet */
      return 0;
    }

  return process_generic(corsaro, &packet->state, ip_hdr->ip_src.s_addr);
}

#ifdef WITH_PLUGIN_SIXT
/** Implements the process_flowtuple function of the plugin API */
int corsaro_ipmeta_process_flowtuple(corsaro_t *corsaro,
				    corsaro_flowtuple_t *flowtuple,
				    corsaro_packet_state_t *state)
{
  /* no point carrying on if a previous plugin has already decided we should
     ignore this tuple */
  if((state->flags & CORSARO_PACKET_STATE_FLAG_IGNORE) != 0)
    {
      return 0;
    }

  return process_generic(corsaro, state,
			 corsaro_flowtuple_get_source_ip(flowtuple));
}

/** Implements the process_flowtuple_class_start function of the plugin API */
int corsaro_ipmeta_process_flowtuple_class_start(corsaro_t *corsaro,
			            corsaro_flowtuple_class_start_t *class)
{
  /* we dont care about these */
  return 0;
}

/** Implements the process_flowtuple_class_end function of the plugin API */
int corsaro_ipmeta_process_flowtuple_class_end(corsaro_t *corsaro,
				     corsaro_flowtuple_class_end_t *class)
{
  /* dont care */
  return 0;
}

/* ========== PUBLIC HELPER FUNCTIONS BELOW HERE ========== */
inline
ipmeta_record_t *corsaro_ipmeta_get_record(struct corsaro_packet_state *pkt_state,
					   ipmeta_provider_id_t provider_id)
{
  assert(pkt_state != NULL);
  assert(provider_id > 0 && provider_id <= IPMETA_PROVIDER_MAX);
  return pkt_state->ipmeta_records[provider_id-1];
}

inline
ipmeta_record_t *corsaro_ipmeta_get_default_record(
				     struct corsaro_packet_state *pkt_state)
{
  assert(pkt_state != NULL);
  return pkt_state->ipmeta_record_default;
}

ipmeta_provider_t *corsaro_ipmeta_get_provider(corsaro_t *corsaro,
					       ipmeta_provider_id_t provider_id)
{
  assert(corsaro != NULL);
  assert(provider_id > 0 && provider_id <= IPMETA_PROVIDER_MAX);

  if(STATE(corsaro) == NULL)
    {
      corsaro_log(__func__, corsaro, "ipmeta plugin not enabled");
      return NULL;
    }

  return ipmeta_get_provider_by_id(STATE(corsaro)->ipmeta, provider_id);
}

#endif
