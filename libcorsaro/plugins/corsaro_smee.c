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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libtrace.h"

#include "iat-smee.h"

#include "utils.h"

#include "corsaro_io.h"
#include "corsaro_file.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#include "corsaro_smee.h"

/** @file
 *
 * @brief Corsaro plugin wrapper for the iat-smee 'library'
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "SMEE" */
#define CORSARO_SMEE_MAGIC 0x534D4545

/** The name of this plugin */
#define PLUGIN_NAME "smee"

/** The name for the stat file */
#define CORSARO_SMEE_STATFILE PLUGIN_NAME"-stat"
/** The name for the sum file */
#define CORSARO_SMEE_SUMFILE PLUGIN_NAME"-sum"
/** The name for the src file */
#define CORSARO_SMEE_SRCFILE PLUGIN_NAME"-sources"

/** Default max lifetime for source to stay in hashtable */
#define CORSARO_SMEE_MX_LIFETIME        3600
/** Default memory size allocated for source hash table (in KB) */
#define CORSARO_SMEE_MX_SOURCES         4000000
/** Default interval in seconds to write summary files */
#define CORSARO_SMEE_TIME_REC_INTERVAL  3600

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_smee_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_SMEE,                         /* id */
  CORSARO_SMEE_MAGIC,                             /* magic */
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_smee),       /* func ptrs */
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct corsaro_smee_state_t {
  /** Should we rotate the files when the next interval starts? */
  int rotate;
  /** The outfile for the plugin */
  corsaro_file_t *statfile;
  corsaro_file_t *sumfile;
  corsaro_file_t *srcfile;

  /** has smee been initialized yet? */
  int smee_started;

  /* some options */
  /** local prefixes */
  struct ip_address *local_addresses;
  /** local prefixes count */
  int local_addresses_cnt;
  /** meter location */
  const char *meter_location;
  /** max lifetime for source to stay in hashtable */
  int max_lifetime;
  /** memory size allocated for source hash table (in KB) */
  int max_sources;
  /** interval in seconds to write summary files */
  int time_rec_interval;
  /** write IAT distributions to a file */
  int save_distributions;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, smee, CORSARO_PLUGIN_ID_SMEE))
/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_SMEE))

/** Parse a local address prefix */
static int parse_local_address(corsaro_t *corsaro, char *address_str)
{
  struct corsaro_smee_state_t *state = STATE(corsaro);
  struct ip_address *la = NULL;

  char *tok = NULL;

  /* re-alloc the prefix list to hold it */	  
  if((state->local_addresses = 
      realloc(state->local_addresses, 
	      sizeof(struct ip_address) * (state->local_addresses_cnt+1))
      ) == NULL)
    {
      fprintf(stderr, "ERROR: could not re-alloc address list\n");
      return -1;
    }

la = &(state->local_addresses[state->local_addresses_cnt++]);
  la->ver = 4;
  
  if((tok = strsep(&address_str, "/")) == NULL)
    {
      fprintf(stderr, "ERROR: Invalid local address (%s)\n", address_str);
      return -1;
    }
  
  /* tok should be the address */
  if((la->a.v4 = inet_addr(tok)) == -1)
    {
      /* this sanity check will cause problems the day someone needs to use
	 255.255.255.255, but for now it is useful. */
      fprintf(stderr, "ERROR: Invalid local address (%s)\n", tok);
      return -1;
    }

  /* address_str should be the prefix length */
  if((la->len = atoi(address_str)) > 32)
    {
      fprintf(stderr, "ERROR: Invalid local address mask (%s)\n", address_str);
      return -1;
    }
  if(la->len == 0)
    {
      fprintf(stderr, "WARNING: Local address mask of 0 (%s)\n", address_str);
    }

  return 0;
}

/** Print plugin usage to stderr */
static void usage(corsaro_t *corsaro)
{
  fprintf(stderr, 
	  "plugin usage: %s [-s] [-i interval] [-l meter_loc] [-L max_src_life] -a prefix\n"
	  "       -a            local prefix "
	  "(-a can be specified multiple times)\n"
	  "       -i            interval between writing summary files (secs) "
	  "(default: %d)\n"
	  "       -l            meter location (default: %s)\n"

	  "       -L            max lifetime for source to stay in hashtable "
	  "(secs) (default: %d)\n"
	  "       -m            memory size allocated for source hash table "
	  "(in KB) (default: %d)\n"
	  "       -s            write the source tables to a file "
	  "(disables summary tables)\n",	  
	  PLUGIN(corsaro)->argv[0],
	  CORSARO_SMEE_TIME_REC_INTERVAL,
	  corsaro_get_monitorname(corsaro),
	  CORSARO_SMEE_MX_LIFETIME,
	  CORSARO_SMEE_MX_SOURCES
	  );
}

/** Parse the arguments given to the plugin
 * @todo upgrade the address parsing to support IPv6 
 */
static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_smee_state_t *state = STATE(corsaro);
  int opt;

  assert(plugin->argc > 0 && plugin->argv != NULL);

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, ":a:i:l:L:m:s?")) >= 0)
    {
      switch(opt)
	{
	case 'a':
	  /* add this prefix to the list */
	  if(parse_local_address(corsaro, optarg) != 0)
	    {
	      usage(corsaro);
	      return -1;
	    }
	  break;

	case 'i':
	  state->time_rec_interval = atoi(optarg);
	  break;

	case 'l':
	  state->meter_location = strdup(optarg);
	  break;

	case 'L':
	  state->max_lifetime = atoi(optarg);
	  break;

	case 'm':
	  state->max_sources = atoi(optarg);
	  break;

	case 's':
	  state->save_distributions = 1;
	  break;

	case '?':
	case ':':
	default:
	  usage(corsaro);
	  return -1;
	}
    }

  if(state->local_addresses_cnt == 0)
    {
      fprintf(stderr, 
	      "ERROR: At least one local prefix must be specified using -a\n");
      usage(corsaro);
      return -1;
    }

  return 0;
}

/** Called by smee to log messages */
static void smee_log_callback(void *user_data, int priority, int die, 
			      const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  corsaro_log_va("libsmee", (corsaro_t*)user_data, fmt, ap);
  va_end(ap);

  if(die == 1)
    {
      abort();
    }
}

/** Helper macro for IO callbacks to write to a Corsaro file */
#define CORSARO_SMEE_FPRINTF(outfile)			\
  va_list ap;						\
  corsaro_t *corsaro = (corsaro_t*)user_data;		\
  va_start(ap, fmt);					\
  corsaro_file_vprintf(corsaro, (outfile), fmt, ap);	\
  /* smee expects a newline to be written.		\
     this is a little hacky, but... */			\
  corsaro_file_printf(corsaro, (outfile), "\n");	\
  va_end(ap);						\
  return 0;

/** Called by smee to write stats lines */
static int smee_stat_callback(void *user_data, const char *fmt, ...)
{
  CORSARO_SMEE_FPRINTF(STATE(corsaro)->statfile)
}

/** Called by smee to populate the summary file */
static int smee_sum_callback(void *user_data, const char *fmt, ...)
{
  CORSARO_SMEE_FPRINTF(STATE(corsaro)->sumfile)
}

/** Called by smee to populate the sources file */
static int smee_sources_callback(void *user_data, const char *fmt, ...)
{
  CORSARO_SMEE_FPRINTF(STATE(corsaro)->srcfile)
}

/** Called by smee to determine the number of dropped packets */
static uint64_t smee_pkt_drops(void *user_data)
{
  return corsaro_get_dropped_packets((corsaro_t*)user_data);
  return 0;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_smee_alloc(corsaro_t *corsaro)
{
  return &corsaro_smee_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_smee_probe_filename(const char *fname)
{
  /* look for 'corsaro_smee' in the name */
  return corsaro_plugin_probe_filename(fname, &corsaro_smee_plugin);
}

/** Implements the probe_magic function of the plugin API */
int corsaro_smee_probe_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  /* once we know what the binary output will look like...*/
  return -1;
}

/** Implements the init_output function of the plugin API */
int corsaro_smee_init_output(corsaro_t *corsaro)
{
  struct corsaro_smee_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);

  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_smee_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		"could not malloc corsaro_smee_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* set some sane default values (for ucsd-nt anyway) */
  state->meter_location = corsaro_get_monitorname(corsaro);
  state->max_lifetime = CORSARO_SMEE_MX_LIFETIME;
  state->max_sources = CORSARO_SMEE_MX_SOURCES;
  state->time_rec_interval = CORSARO_SMEE_TIME_REC_INTERVAL;
  state->save_distributions = 0;

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      return -1;
    }
  
  /* files opened at first interval */

  /*
    Parameter values:

    c_meter_loc
      'SAN'  or 'AKL'

    c_mx_life
      Max lifetime for a source to stay in hash table.
      -m option to iat-monitor.rb.
      = mx_lifetime = 3600 (SAN) or 172800 (AKL, seconds in two days)

    c_mx_sources = 4000000  or  800000  # 4M on thor, 800k on nevil-res3
      Specifies size of memory chunks allocated by iatmon for
      its source hash table.  MX_EXTRA_BLOCKS (defined  2 in iat.h)
      may be allocated each time the available space is used up.

    c_time_rec_interval
      Interval time in seconds to write summary files
      -c option to iat-monitor.rb, = Hours_reqd*3600 from iat-config.rb

    c_local_addrs, c_n_local_addrs = 'XX.0.0.0/8'  # (CAIDA)  or
      '130.216/16, 202.36.245/24, 2001:0df0::/47' # (U Auckland)


    [ STATS_RECORDING_INTERVAL
      Intervals to write statistics (#Stats: ) lines
      defined 60 s in iat.h ]

    save_distributions
      true to write iat distributions to a file
      (so that one can analyse their statistics offline).
  */

  /* there is a 'feature' in libsmee which means that if smee_sources_callback
     is given, then smee_sum_callback will never be called. We should detect if
     this is the case and not open the sum file. */
  
  iat_init(corsaro_get_traceuri(corsaro), /* traceuri */
	   state->meter_location, /* meterloc */
	   state->max_lifetime, /* mx_lifetime */
	   state->max_sources, /*c_mx_sources */
	   state->time_rec_interval, /* c_time_rec_interval */
	   state->local_addresses,
	   state->local_addresses_cnt,
	   corsaro, /* c_user_data */
	   smee_log_callback, /* c_log_msg */
	   smee_stat_callback, /* c_stat_printf */
	   (state->save_distributions == 0 ? smee_sum_callback : NULL),
	   (state->save_distributions == 0 ? NULL : smee_sources_callback),
	   smee_pkt_drops /* c_pkt_drops */
	   );

  return 0;

 err:
  corsaro_smee_close_output(corsaro);
  return -1;
}

/** Implements the init_input function of the plugin API */
int corsaro_smee_init_input(corsaro_in_t *corsaro)
{
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_smee_close_input(corsaro_in_t *corsaro)
{
  return -1;
}

/** Implements the close_output function of the plugin API */
int corsaro_smee_close_output(corsaro_t *corsaro)
{
  struct corsaro_smee_state_t *state = STATE(corsaro);

  /* smee is not smart enough to ignore this if it hasn't been init'd yet */
  if(state->smee_started != 0)
    {
      iat_process_packet(NULL, SM_DUMMY);
      state->smee_started = 0;
    }

  if(state != NULL)
    {
      if(state->statfile != NULL)
	{
	  corsaro_file_close(corsaro, state->statfile);
	  state->statfile = NULL;
	}
      if(state->sumfile != NULL)
	{
	  corsaro_file_close(corsaro, state->sumfile);
	  state->sumfile = NULL;
	}
      if(state->srcfile != NULL)
	{
	  corsaro_file_close(corsaro, state->srcfile);
	  state->srcfile = NULL;
	}

      if(state->local_addresses_cnt > 0 && state->local_addresses != NULL)
	{
	  free(state->local_addresses);
	  state->local_addresses = NULL;
	  state->local_addresses_cnt = 0;
	}

      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }
  
  return 0;
}

/** Implements the read_record function of the plugin API */
off_t corsaro_smee_read_record(struct corsaro_in *corsaro, 
			  corsaro_in_record_type_t *record_type, 
			  corsaro_in_record_t *record)
{
  corsaro_log_in(__func__, corsaro, "not yet implemented");
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_smee_read_global_data_record(struct corsaro_in *corsaro, 
			      enum corsaro_in_record_type *record_type, 
			      struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_smee_start_interval(corsaro_t *corsaro, corsaro_interval_t *int_start)
{
  struct corsaro_smee_state_t *state = STATE(corsaro);

  if(state->rotate == 1)
    {
      if(state->statfile != NULL)
	{
	  corsaro_file_close(corsaro, state->statfile);
	  state->statfile = NULL;
	}
      if(state->sumfile != NULL)
	{
	  corsaro_file_close(corsaro, state->sumfile);
	  state->sumfile = NULL;
	}
      if(state->srcfile != NULL)
	{
	  corsaro_file_close(corsaro, state->srcfile);
	  state->srcfile = NULL;
	}

      state->rotate = 0;
    }

  /* open the stats output file */
  if(state->statfile == NULL &&
     (state->statfile = corsaro_io_prepare_file(corsaro, CORSARO_SMEE_STATFILE,
						int_start)) 
     == NULL)
    {
      corsaro_log(__func__, corsaro, "could not open %s output file", 
		CORSARO_SMEE_STATFILE);
      goto err;
    }

  /* open the sum output file (only if save_distributions is disabled) */
  if(state->save_distributions == 0 &&
     state->sumfile == NULL &&
     (state->sumfile = corsaro_io_prepare_file(corsaro, CORSARO_SMEE_SUMFILE,
					       int_start)) 
     == NULL)
    {
      corsaro_log(__func__, corsaro, "could not open %s output file", 
		CORSARO_SMEE_SUMFILE);
      goto err;
    }

  /* open the srcs output file */
  if(state->save_distributions != 0 &&
     state->srcfile == NULL && 
     (state->srcfile = corsaro_io_prepare_file(corsaro, CORSARO_SMEE_SRCFILE,
					       int_start)) 
     == NULL)
    {
      corsaro_log(__func__, corsaro, "could not open %s output file", 
		CORSARO_SMEE_SRCFILE);
      goto err;
    }

  /* now smee is fully started and can be used */
  state->smee_started = 1;

  return 0;

 err:
  corsaro_smee_close_output(corsaro);
  return -1;
}

/** Implements the end_interval function of the plugin API */
int corsaro_smee_end_interval(corsaro_t *corsaro, corsaro_interval_t *int_end)
{
  /* smee only supports ascii output right now, so be a little rude and
     ignore the corsaro output mode */
  iat_process_packet(NULL, SM_RECORD_REQ);

  /* because of how smee dumps the summary file, we cant close our output
     files here, it will try and write to them once we start shutting down.

     go look in _start_interval to see where we rotate the files
  */
  if(corsaro_is_rotate_interval(corsaro) == 1)
    {
      STATE(corsaro)->rotate = 1;
    }

  return 0;
}

/** Implements the process_packet function of the plugin API */
int corsaro_smee_process_packet(corsaro_t *corsaro, 
			     corsaro_packet_t *packet)
{
  libtrace_packet_t *ltpacket = LT_PKT(packet);
  int rc; 

  assert(STATE(corsaro)->smee_started != 0);

  /* pass the packet on to smee */
  rc = iat_process_packet(ltpacket, SM_PACKET);

  if(rc != SM_OK && rc != SM_RECORD_INTERVAL)
    {
      corsaro_log(__func__, corsaro, "iat_process_packet returned %d", rc);
      return -1;
    }
  return 0;
}



