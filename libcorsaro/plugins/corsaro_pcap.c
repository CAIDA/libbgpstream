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

#include "libtrace.h"

#include "utils.h"

#include "corsaro_io.h"
#include "corsaro_file.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#include "corsaro_pcap.h"

/** @file
 *
 * @brief Corsaro raw pcap pass-through plugin
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "PCAP" */
#define CORSARO_PCAP_MAGIC 0x50434150

/** The name of this plugin */
#define PLUGIN_NAME "pcap"

/** The number of output file pointers to support non-blocking close at the end
    of an interval. If the wandio buffers are large enough that it takes more
    than 1 interval to drain the buffers, consider increasing this number */
#define OUTFILE_POINTERS 2

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_pcap_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_PCAP,                      /* id */
  CORSARO_PCAP_MAGIC,                          /* magic */
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_pcap),  /* func ptrs */
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct corsaro_pcap_state_t {
  /** The outfile for the plugin */
  corsaro_file_t *outfile;
    /** A set of pointers to outfiles to support non-blocking close */
  corsaro_file_t *outfile_p[OUTFILE_POINTERS];
  /** The current outfile */
  int outfile_n;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, pcap, CORSARO_PLUGIN_ID_PCAP))
/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_PCAP))

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_pcap_alloc(corsaro_t *corsaro)
{
  return &corsaro_pcap_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_pcap_probe_filename(const char *fname)
{
  /* cannot read raw pcap files using corsaro_in */
  return 0;
}

/** Implements the probe_magic function of the plugin API */
int corsaro_pcap_probe_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  /* cannot read raw pcap files using corsaro_in */
  return 0;
}

/** Implements the init_output function of the plugin API */
int corsaro_pcap_init_output(corsaro_t *corsaro)
{
  struct corsaro_pcap_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  assert(plugin != NULL);
 
  if((state = malloc_zero(sizeof(struct corsaro_pcap_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		"could not malloc corsaro_pcap_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* defer opening the output file until we start the first interval */

  return 0;

 err:
  corsaro_pcap_close_output(corsaro);
  return -1;
}

/** Implements the init_output function of the plugin API */
int corsaro_pcap_init_input(corsaro_in_t *corsaro)
{
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_pcap_close_input(corsaro_in_t *corsaro)
{
  return -1;
}

/** Implements the close_output function of the plugin API */
int corsaro_pcap_close_output(corsaro_t *corsaro)
{
  int i;
  struct corsaro_pcap_state_t *state = STATE(corsaro);

  if(state != NULL)
    {
      /* close all the outfile pointers */
      for(i = 0; i < OUTFILE_POINTERS; i++)
	{
	  if(state->outfile_p[i] != NULL)
	    {
	      corsaro_file_close(corsaro, state->outfile_p[i]);
	      state->outfile_p[i] = NULL;
	    }
	}
      state->outfile = NULL;

      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }
  
  return 0;
}

/** Implements the read_record function of the plugin API */
off_t corsaro_pcap_read_record(struct corsaro_in *corsaro, 
			       corsaro_in_record_type_t *record_type, 
			       corsaro_in_record_t *record)
{
  /* This plugin can't read it's data back. just use libtrace */
  corsaro_log_in(__func__, corsaro, "pcap files are simply trace files."
		 " use libtrace instead of corsaro");
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_pcap_read_global_data_record(struct corsaro_in *corsaro, 
			      enum corsaro_in_record_type *record_type, 
			      struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_pcap_start_interval(corsaro_t *corsaro, corsaro_interval_t *int_start)
{
  if(STATE(corsaro)->outfile == NULL)
    {
      /* open the output file */
      if((
	  STATE(corsaro)->outfile_p[STATE(corsaro)->outfile_n] = 
	  corsaro_io_prepare_file_full(corsaro, 
				       PLUGIN(corsaro)->name,
				       int_start,
				       CORSARO_FILE_MODE_TRACE,
				       corsaro->compress,
				       corsaro->compress_level,
				       0)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "could not open %s output file", 
		      PLUGIN(corsaro)->name);
	  return -1;
	}
      STATE(corsaro)->outfile = STATE(corsaro)->
	outfile_p[STATE(corsaro)->outfile_n];
    }
  return 0;
}

/** Implements the end_interval function of the plugin API */
int corsaro_pcap_end_interval(corsaro_t *corsaro, corsaro_interval_t *int_end)
{
  struct corsaro_pcap_state_t *state = STATE(corsaro);

  /* if we are rotating, now is when we should do it */
  if(corsaro_is_rotate_interval(corsaro))
    {
      /* leave the current file to finish draining buffers */
      assert(state->outfile != NULL);

      /* move on to the next output pointer */
      state->outfile_n = (state->outfile_n+1) % 
	OUTFILE_POINTERS;

      if(state->outfile_p[state->outfile_n] != NULL)
	{
	  /* we're gonna have to wait for this to close */
	  corsaro_file_close(corsaro, 
		   state->outfile_p[state->outfile_n]);
	  state->outfile_p[state->outfile_n] =  NULL;
	}

      state->outfile = NULL;
    }
  return 0;
}

/** Implements the process_packet function of the plugin API */
int corsaro_pcap_process_packet(corsaro_t *corsaro, 
				corsaro_packet_t *packet)
{
  if(corsaro_file_write_packet(corsaro, STATE(corsaro)->outfile, 
			       LT_PKT(packet)) <= 0)
    {
      corsaro_log(__func__, corsaro, "could not write packet");
      return -1;
    }
  return 0;
}



