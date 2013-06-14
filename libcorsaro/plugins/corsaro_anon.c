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

/** The default anonymization key - REMOVE THIS */
#define ANON_KEY "WoF0jynnmWzSGOi2tM72yNEGPwXqOYLS"

/** Anonymize the Source IP by default? */
#define ANON_SOURCE 1

/** Anonymize the Destination IP by default? */
#define ANON_DEST 1

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

#if 0
/** Holds the state for an instance of this plugin */
struct corsaro_anon_state_t {
  
};
#endif

#if 0
/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, pcap, CORSARO_PLUGIN_ID_ANON))
#endif

/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_ANON))

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
  /** @todo replace these with things given by options */
  corsaro_anon_init(ANON_ENC_TYPE, ANON_KEY);
  return 0;
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
  libtrace_ip_t *iphdr = trace_get_ip(LT_PKT(packet));
  
  if(iphdr != NULL && (ANON_SOURCE || ANON_DEST))
    {
      corsaro_anon_ip_header(iphdr, ANON_SOURCE, ANON_DEST);
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
