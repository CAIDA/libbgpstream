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

#include "corsaro_libanon.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

#include "corsaro_anon.h"

/** @file
 *
 * @brief Corsaro IP anonymization plugin
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "ANON" */
#define CORSARO_ANON_MAGIC 0x414E4F4E

/** The name of this plugin */
#define PLUGIN_NAME "anon"

/** The default anonymization type */
#define ANON_ENC_TYPE CORSARO_ANON_ENC_CRYPTOPAN

/** The configuration string for the CORSARO_ANON_ENC_CRYPTOPAN type */
#define ENC_TYPE_CRYPTOPAN "cryptopan"

/** The configuration string for the CORSARO_ANON_ENC_PREFIX type */
#define ENC_TYPE_PREFIX "prefix"

/** Anonymize the Source IP by default? */
#define ANON_SOURCE 0

/** Anonymize the Destination IP by default? */
#define ANON_DEST 0

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_anon_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_ANON,                      /* id */
  CORSARO_ANON_MAGIC,                          /* magic */
#ifdef WITH_PLUGIN_SIXT
  CORSARO_PLUGIN_GENERATE_PTRS_FT(corsaro_anon),  /* func ptrs */
#else
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_anon),
#endif
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct corsaro_anon_state_t {
  /** The encryption type to use */
  corsaro_anon_enc_type_t encryption_type;
  /** The CryptoPAn encryption key or prefix to use */
  char *encryption_key;
  /** Should source addresses be encrypted? */
  int encrypt_source;
  /** Should destination addresses be encrypted? */
  int encrypt_destination;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, anon, CORSARO_PLUGIN_ID_ANON))

/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_ANON))

static void usage(corsaro_plugin_t *plugin)
{
  fprintf(stderr,
	  "plugin usage: %s [-sd] [-t encryption_type] encryption_key[prefix]\n"
	  "       -d            enable destination address encryption\n"
	  "       -s            enable source address encryption\n"
	  "       -t            encryption type (default: %s)\n"
	  "                     must be either '%s', or '%s'\n",
	  plugin->argv[0],
	  ENC_TYPE_CRYPTOPAN,
	  ENC_TYPE_CRYPTOPAN,
	  ENC_TYPE_PREFIX);
}

static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_anon_state_t *state = STATE(corsaro);
  int opt;

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, ":dst:?")) >= 0)
    {
      switch(opt)
	{
	case 'd':
	  state->encrypt_destination = 1;
	  break;

	case 's':
	  state->encrypt_source = 1;
	  break;

	case 't':
	  if(strcasecmp(optarg, ENC_TYPE_CRYPTOPAN) == 0)
	    {
	      state->encryption_type = CORSARO_ANON_ENC_CRYPTOPAN;
	    }
	  else if(strcasecmp(optarg, ENC_TYPE_PREFIX) == 0)
	    {
	      state->encryption_type = CORSARO_ANON_ENC_PREFIX_SUBSTITUTION;
	    }
	  else
	    {
	      fprintf(stderr, "ERROR: invalid encryption type (%s)\n",
		      optarg);
	      usage(plugin);
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

  /* the last (and only required argument) must be the key */
  if(optind != (plugin->argc - 1))
    {
      fprintf(stderr, "ERROR: missing encryption key\n");
      usage(plugin);
      return -1;
    }

  state->encryption_key = plugin->argv[optind];

  if(state->encrypt_source == 0 && state->encrypt_destination == 0)
    {
      fprintf(stderr,
	      "WARNING: anon plugin is encrypting nothing\n");
    }

  return 0;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

corsaro_plugin_t *corsaro_anon_alloc(corsaro_t *corsaro)
{
  return &corsaro_anon_plugin;
}

int corsaro_anon_probe_filename(const char *fname)
{
  /* this writes no files! */
  return 0;
}

int corsaro_anon_probe_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  /* this writes no files! */
  return 0;
}

int corsaro_anon_init_output(corsaro_t *corsaro)
{
  struct corsaro_anon_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);

  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_anon_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro,
		"could not malloc corsaro_anon_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* set the defaults */
  state->encryption_type = CORSARO_ANON_ENC_CRYPTOPAN;
  state->encrypt_source = ANON_SOURCE;
  state->encrypt_destination = ANON_DEST;

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      return -1;
    }

  assert(state->encryption_key != NULL);

  corsaro_anon_init(state->encryption_type, state->encryption_key);
  return 0;

 err:
  corsaro_anon_close_output(corsaro);
  return -1;
}

int corsaro_anon_init_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

int corsaro_anon_close_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

int corsaro_anon_close_output(corsaro_t *corsaro)
{  
  return 0;
}

off_t corsaro_anon_read_record(struct corsaro_in *corsaro, 
			       corsaro_in_record_type_t *record_type, 
			       corsaro_in_record_t *record)
{
  assert(0);
  return -1;
}

off_t corsaro_anon_read_global_data_record(struct corsaro_in *corsaro, 
			      enum corsaro_in_record_type *record_type, 
			      struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

int corsaro_anon_start_interval(corsaro_t *corsaro, 
				corsaro_interval_t *int_start)
{
  /* we don't care */
  return 0;
}

int corsaro_anon_end_interval(corsaro_t *corsaro, 
			      corsaro_interval_t *int_end)
{
  /* we don't care */
  return 0;
}

int corsaro_anon_process_packet(corsaro_t *corsaro, 
				corsaro_packet_t *packet)
{
  struct corsaro_anon_state_t *state = STATE(corsaro);
  libtrace_ip_t *iphdr = trace_get_ip(LT_PKT(packet));
  
  if(iphdr != NULL && (state->encrypt_source || state->encrypt_destination))
    {
      corsaro_anon_ip_header(iphdr, state->encrypt_source, 
			     state->encrypt_destination);
    }

  return 0;
}

#ifdef WITH_PLUGIN_SIXT
int corsaro_anon_process_flowtuple(corsaro_t *corsaro,
				   corsaro_flowtuple_t *flowtuple,
				   corsaro_packet_state_t *state)
{
  uint32_t src_ip = corsaro_flowtuple_get_source_ip(flowtuple);
  uint32_t dst_ip = corsaro_flowtuple_get_destination_ip(flowtuple);

  uint32_t src_ip_anon = corsaro_anon_ip(ntohl(src_ip));
  uint32_t dst_ip_anon = corsaro_anon_ip(ntohl(dst_ip));

  flowtuple->src_ip = htonl(src_ip_anon);
  CORSARO_FLOWTUPLE_IP_TO_SIXT(htonl(dst_ip_anon), flowtuple);
  
  return 0;
}

int corsaro_anon_process_flowtuple_class_start(corsaro_t *corsaro,
				   corsaro_flowtuple_class_start_t *class)
{
  /* we dont care about these */
  return 0;
}

int corsaro_anon_process_flowtuple_class_end(corsaro_t *corsaro,
				   corsaro_flowtuple_class_end_t *class)
{
  /* dont care */
  return 0;
}
#endif
