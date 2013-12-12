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
#include <string.h>
#include <unistd.h>

#include "libtrace.h"

#include "utils.h"

#include "corsaro_io.h"
#include "corsaro_file.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#include "corsaro_flowtuple.h"

/** @file
 *
 * @brief Corsaro FlowTuple plugin implementation
 *
 * @author Alistair King
 *
 */

/* This magic number is a legacy number from when we used to call it the
   'sixtuple' */
#ifdef CORSARO_SLASH_EIGHT
/** The magic number for this plugin when using /8 opts - "SIXT" */
#define CORSARO_FLOWTUPLE_MAGIC 0x53495854
#else
/** The magic number for this plugin when not using /8 opts - "SIXU" */
#define CORSARO_FLOWTUPLE_MAGIC 0x53495855
#endif

/** Possible states for FlowTuple output sorting */
typedef enum corsaro_flowtuple_sort
  {
    /** FlowTuple output sorting is disabled */
    CORSARO_FLOWTUPLE_SORT_DISABLED = 0,

    /** FlowTuple output sorting is enabled */
    CORSARO_FLOWTUPLE_SORT_ENABLED  = 1,

    /** Default FlowTuple output sorting behavior (enabled) */
    CORSARO_FLOWTUPLE_SORT_DEFAULT  = CORSARO_FLOWTUPLE_SORT_ENABLED,

  } corsaro_flowtuple_sort_t;

/** The number of output file pointers to support non-blocking close at the end
    of an interval. If the wandio buffers are large enough that it takes more
    than 1 interval to drain the buffers, consider increasing this number */
#define OUTFILE_POINTERS 2

/** The name of this plugin */
#define PLUGIN_NAME "flowtuple"

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_flowtuple_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_FLOWTUPLE,                    /* id */
  CORSARO_FLOWTUPLE_MAGIC,                        /* magic */
  CORSARO_PLUGIN_GENERATE_PTRS_FT(corsaro_flowtuple),   /* func ptrs */
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Array of string names for classes */
static const char *class_names[] = {
  "flowtuple_backscatter",
  "flowtuple_icmpreq",
  "flowtuple_other",
};

/** Initialize the sorting functions and datatypes */
KSORT_INIT(sixt, corsaro_flowtuple_t*, corsaro_flowtuple_lt);

/** Initialize the hash functions and datatypes */
/*KHASH_INIT(sixt, corsaro_flowtuple_t*, khint32_t, 1, */
KHASH_INIT(sixt, corsaro_flowtuple_t*, char, 0,
	   corsaro_flowtuple_hash_func, corsaro_flowtuple_hash_equal);

/** Holds the state for an instance of this plugin */
struct corsaro_flowtuple_state_t {
  /** Array of hash tables, one for each corsaro_flowtuple_class_type_t */
  khash_t(sixt) *st_hash[CORSARO_FLOWTUPLE_CLASS_MAX+1];
  /** The current class (if we are reading FT data) */
  uint16_t current_class;
  /** The outfile for the plugin */
  corsaro_file_t *outfile;
  /** A set of pointers to outfiles to support non-blocking close */
  corsaro_file_t *outfile_p[OUTFILE_POINTERS];
  /** The current outfile */
  int outfile_n;
  /** Is output sorting enabled? */
  corsaro_flowtuple_sort_t sort_enabled;
};

/** Holds the state for an instance of this plugin (when reading data) */
struct corsaro_flowtuple_in_state_t {
  /** The expected type of the next record in the file */
  corsaro_in_record_type_t expected_type;
  /** The number of tuples in the current class */
  int tuple_total;
  /** The number of tuples already read in the current class */
  int tuple_cnt;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, flowtuple, CORSARO_PLUGIN_ID_FLOWTUPLE))
/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE_IN(corsaro)						\
  (CORSARO_PLUGIN_STATE(corsaro, flowtuple_in, CORSARO_PLUGIN_ID_FLOWTUPLE))
/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_FLOWTUPLE))

/** Print usage information to stderr */
static void usage(corsaro_plugin_t *plugin)
{
  fprintf(stderr,
	  "plugin usage: %s [-s]\n"
	  "       -s            disable flowtuple output sorting\n",
	  plugin->argv[0]);
}

/** Parse the arguments given to the plugin */
static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_flowtuple_state_t *state = STATE(corsaro);
  int opt;

  if(plugin->argc <= 0)
    {
      return 0;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, "s?")) >= 0)
    {
      switch(opt)
	{
	case 's':
	  state->sort_enabled = CORSARO_FLOWTUPLE_SORT_DISABLED;
	  break;

	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  /* flowtuple doesn't take any arguments */
  if(optind != plugin->argc)
    {
      usage(plugin);
      return -1;
    }

  return 0;
}

/**
 * Determines the traffic class for a packet; possible options are
 * CORSARO_FLOWTUPLE_CLASS_BACKSCATTER, CORSARO_FLOWTUPLE_CLASS_ICMPREQ,
 * CLASS_OTHER
 *
 * This code is ported from crl_attack_flow.c::get_traffic_type
 */
static int flowtuple_classify_packet(corsaro_t *corsaro,
				     libtrace_packet_t *packet)
{
  void *temp = NULL;
  uint8_t proto;
  uint32_t remaining;

  libtrace_tcp_t  *tcp_hdr  = NULL;
  libtrace_icmp_t *icmp_hdr = NULL;

  /* 10/19/12 ak removed check for ipv4 because it is checked in per_packet */

  /* get the transport header */
  if((temp = trace_get_transport(packet, &proto, &remaining)) == NULL)
    {
      /* not enough payload */
      return CORSARO_FLOWTUPLE_CLASS_OTHER;
    }

  /* check for tcp */
  if(proto == TRACE_IPPROTO_TCP && remaining >= 4)
    {
      tcp_hdr = (libtrace_tcp_t *)temp;

      /* look for SYNACK or RST */
      if((tcp_hdr->syn && tcp_hdr->ack) || tcp_hdr->rst)
	{
	  return CORSARO_FLOWTUPLE_CLASS_BACKSCATTER;
	}
      else
	{
	  return CORSARO_FLOWTUPLE_CLASS_OTHER;
	}
    }
  /* check for icmp */
  else if(proto == TRACE_IPPROTO_ICMP && remaining >= 2)
    {
      icmp_hdr = (libtrace_icmp_t *)temp;
      if(icmp_hdr->type == 0  ||
	 icmp_hdr->type == 3  ||
	 icmp_hdr->type == 4  ||
	 icmp_hdr->type == 5  ||
	 icmp_hdr->type == 11 ||
	 icmp_hdr->type == 12 ||
	 icmp_hdr->type == 14 ||
	 icmp_hdr->type == 16 ||
	 icmp_hdr->type == 18)
	{
	  return CORSARO_FLOWTUPLE_CLASS_BACKSCATTER;
	}
      else
	{
	  return CORSARO_FLOWTUPLE_CLASS_ICMPREQ;
	}
    }
  else
    {
      return CORSARO_FLOWTUPLE_CLASS_OTHER;
    }

  return -1;
}

/** Given a st hash, malloc and return a sorted array of pointers */
static int sort_hash(corsaro_t *corsaro, kh_sixt_t *hash,
		     corsaro_flowtuple_t ***sorted)
{
  khiter_t i;
  corsaro_flowtuple_t **ptr;

  if((ptr = malloc(sizeof(corsaro_flowtuple_t*)*kh_size(hash))) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not malloc array for sorted keys");
      return -1;
    }
  *sorted = ptr;

  if(kh_size(hash) == 0)
    {
      /* no need to try and sort an empty hash */
      return 0;
    }

  for(i = kh_begin(hash); i != kh_end(hash); ++i)
    {
      if(kh_exist(hash, i))
	{
	  *ptr = kh_key(hash, i);
	  ptr++;
	}
    }

  ks_introsort(sixt, kh_size(hash), *sorted);
  return 0;
}

/** Dump the given flowtuple to the plugin's outfile in binary format */
static int binary_dump(corsaro_t *corsaro, corsaro_flowtuple_class_type_t dist)
{
  struct corsaro_flowtuple_state_t *state = STATE(corsaro);
  kh_sixt_t *h = state->st_hash[dist];

  int j;
  corsaro_flowtuple_t **sorted_keys;
  khiter_t i = 0;

  uint8_t hbuf[4+2+4];
  uint8_t *hptr = &hbuf[0];

  bytes_htonl(hptr, CORSARO_FLOWTUPLE_MAGIC);
  hptr+=4;

  bytes_htons(hptr, dist);
  hptr+=2;

  bytes_htonl(hptr, kh_size(h));

  if(corsaro_file_write(corsaro, STATE(corsaro)->outfile, &hbuf[0], 10) != 10)
    {
      corsaro_log(__func__, corsaro,
		  "could not dump byte flowtuple header to file");
      return -1;
    }

  if(kh_size(h) > 0)
    {
      if(state->sort_enabled == CORSARO_FLOWTUPLE_SORT_ENABLED)
	{
	  /* sort the hash before dumping */
	  if(sort_hash(corsaro, h, &sorted_keys) != 0)
	    {
	      corsaro_log(__func__, corsaro, "could not sort keys");
	      return -1;
	    }
	  for(j = 0; j < kh_size(h); j++)
	    {
	      if(corsaro_file_write(corsaro, STATE(corsaro)->outfile,
				    sorted_keys[j],
				    CORSARO_FLOWTUPLE_BYTECNT) !=
		 CORSARO_FLOWTUPLE_BYTECNT)
		{
		  corsaro_log(__func__, corsaro,
			      "could not write flowtuple to file");
		  return -1;
		}
	      /* this actually frees the flowtuples themselves */
	      free(sorted_keys[j]);
	    }
	  free(sorted_keys);
	}
      else
	{
	  /* do not sort the hash */
	  for(i = kh_begin(h); i != kh_end(h); ++i)
	    {
	      if(kh_exist(h, i))
		{
		  if(corsaro_file_write(corsaro, STATE(corsaro)->outfile,
					kh_key(h, i),
					CORSARO_FLOWTUPLE_BYTECNT) !=
		     CORSARO_FLOWTUPLE_BYTECNT)
		    {
		      corsaro_log(__func__, corsaro,
				  "could not write flowtuple to file");
		      return -1;
		    }
		  free(kh_key(h, i));
		}
	    }
	}
    }

  if(corsaro_file_write(corsaro, STATE(corsaro)->outfile, &hbuf[0], 6) != 6)
    {
      corsaro_log(__func__, corsaro, "could not dump flowtuple trailer to file");
      return -1;
    }
  return 0;
}

/** Dump the given flowtuple to the plugin's outfile in ASCII format */
static int ascii_dump(corsaro_t *corsaro, corsaro_flowtuple_class_type_t dist)
{
  struct corsaro_flowtuple_state_t *state = STATE(corsaro);
  kh_sixt_t *h = state->st_hash[dist];

  corsaro_flowtuple_t **sorted_keys;
  int j;
  khiter_t i;

  /*const char *name = class_names[dist];*/
  corsaro_flowtuple_class_start_t class_start;
  corsaro_flowtuple_class_end_t class_end;

  class_start.magic = CORSARO_FLOWTUPLE_MAGIC;
  class_start.class_type = dist;
  class_start.count = kh_size(h);

  class_end.magic = CORSARO_FLOWTUPLE_MAGIC;
  class_end.class_type = dist;

  corsaro_flowtuple_class_start_fprint(corsaro, STATE(corsaro)->outfile,
				    &class_start);

  if(kh_size(h) > 0)
    {
      if(state->sort_enabled == CORSARO_FLOWTUPLE_SORT_ENABLED)
	{
	  /* sort the hash before dumping */
	  if(sort_hash(corsaro, h, &sorted_keys) != 0)
	    {
	      corsaro_log(__func__, corsaro, "could not sort keys");
	      return -1;
	    }
	  for(j = 0; j < kh_size(h); j++)
	    {
	      corsaro_flowtuple_fprint(corsaro, STATE(corsaro)->outfile,
				       sorted_keys[j]);

	      free(sorted_keys[j]);
	    }
	  free(sorted_keys);
	}
      else
	{
	  /* do not sort the hash before dumping */
	  for(i = kh_begin(h); i != kh_end(h); ++i)
	    {
	      if(kh_exist(h, i))
		{
		  corsaro_flowtuple_fprint(corsaro,
					   STATE(corsaro)->outfile,
					   kh_key(h, i));
		  free(kh_key(h, i));
		}
	    }
	}
    }

  corsaro_flowtuple_class_end_fprint(corsaro, STATE(corsaro)->outfile,
				     &class_end);
  return 0;
}

/** Check that a class start record is valid */
static int validate_class_start(corsaro_flowtuple_class_start_t *class)
{
  /* byteswap the values */
  class->magic = ntohl(class->magic);
  class->class_type = ntohs(class->class_type);
  class->count = ntohl(class->count);

  /* do some sanity checking */
  if(class->magic != CORSARO_FLOWTUPLE_MAGIC ||
     class->class_type > CORSARO_FLOWTUPLE_CLASS_MAX)
    {
      return 0;
    }
  return 1;
}

/** Read a class start record */
static int read_class_start(corsaro_in_t *corsaro,
			    corsaro_in_record_type_t *record_type,
			    corsaro_in_record_t *record)
{
  off_t bytes_read;

  if((bytes_read =
      corsaro_io_read_bytes(corsaro, record,
			  sizeof(corsaro_flowtuple_class_start_t))) !=
     sizeof(corsaro_flowtuple_class_start_t))
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bytes_read;
    }

  if(validate_class_start((corsaro_flowtuple_class_start_t *)record->buffer) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate flowtuple class");
      corsaro_log_in(__func__, corsaro,
		     "it is possible this flowtuple file was written "
#ifdef CORSARO_SLASH_EIGHT
		     "without /8 "
#else
		     "with /8 "
#endif
		     "optimizations enabled");
      corsaro_log_in(__func__, corsaro, "try rebuilding using the "
#ifdef CORSARO_SLASH_EIGHT
		    "--without-slash-eight "
#else
		    "--with-slash-eight=0 "
#endif
		    "configure option");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  assert(bytes_read == sizeof(corsaro_flowtuple_class_start_t));

  *record_type = CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START;
  STATE_IN(corsaro)->tuple_total = ((corsaro_flowtuple_class_start_t *)
				  record->buffer)->count;

  STATE_IN(corsaro)->expected_type = (STATE_IN(corsaro)->tuple_total == 0) ?
    CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END :
    CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE;

  return bytes_read;
}

/** Check that a class end record is valid */
static int validate_class_end(corsaro_flowtuple_class_end_t *class)
{
  /* byteswap the values */
  class->magic = ntohl(class->magic);
  class->class_type = ntohs(class->class_type);

  /* do some sanity checking */
  if(class->magic != CORSARO_FLOWTUPLE_MAGIC ||
     class->class_type > CORSARO_FLOWTUPLE_CLASS_MAX)
    {
      return 0;
    }
  return 1;
}

/** Read a class end record */
static int read_class_end(corsaro_in_t *corsaro,
			  corsaro_in_record_type_t *record_type,
			  corsaro_in_record_t *record)
{
  off_t bytes_read;

  if((bytes_read =
      corsaro_io_read_bytes(corsaro, record,
			  sizeof(corsaro_flowtuple_class_end_t))) !=
     sizeof(corsaro_flowtuple_class_end_t))
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bytes_read;
    }

  if(validate_class_end((corsaro_flowtuple_class_end_t *)record->buffer) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate flowtuple class end");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  assert(bytes_read == sizeof(corsaro_flowtuple_class_end_t));

  *record_type = CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END;
  STATE_IN(corsaro)->expected_type = (((corsaro_flowtuple_class_end_t *)
				     record->buffer)->class_type ==
				    (uint16_t)CORSARO_FLOWTUPLE_CLASS_MAX) ?
    CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END :
    CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START;
  STATE_IN(corsaro)->tuple_total = 0;
  STATE_IN(corsaro)->tuple_cnt = 0;

  return bytes_read;
}

/** Attempt to validate a flowtuple record (no-op) */
static int validate_flowtuple(corsaro_flowtuple_t *flowtuple)
{
  /* there is no real validation that can be done with this */
  return 1;
}

/** Read a flowtuple record */
static int read_flowtuple(corsaro_in_t *corsaro,
			 corsaro_in_record_type_t *record_type,
			 corsaro_in_record_t *record)
{
  off_t bytes_read;

  if((bytes_read = corsaro_io_read_bytes(corsaro, record,
				       sizeof(corsaro_flowtuple_t))) !=
     sizeof(corsaro_flowtuple_t))
    {
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return bytes_read;
    }

  if(validate_flowtuple((corsaro_flowtuple_t *)record->buffer) != 1)
    {
      corsaro_log_in(__func__, corsaro, "could not validate flowtuple");
      *record_type = CORSARO_IN_RECORD_TYPE_NULL;
      return -1;
    }

  *record_type = CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE;

  if(++(STATE_IN(corsaro)->tuple_cnt) == STATE_IN(corsaro)->tuple_total)
    {
      STATE_IN(corsaro)->expected_type = CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END;
    }

  assert(bytes_read == sizeof(corsaro_flowtuple_t));

  return bytes_read;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_flowtuple_alloc(corsaro_t *corsaro)
{
  return &corsaro_flowtuple_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_flowtuple_probe_filename(const char *fname)
{
  /* look for 'corsaro_flowtuple' in the name */
  return corsaro_plugin_probe_filename(fname, &corsaro_flowtuple_plugin);
}

/** Implements the probe_magic function of the plugin API */
int corsaro_flowtuple_probe_magic(corsaro_in_t *corsaro,
				  corsaro_file_in_t *file)
{
  char buffer[1024];
  int len;
  int i = CORSARO_IO_INTERVAL_HEADER_BYTE_LEN;

  len = corsaro_file_rpeek(file, buffer, sizeof(buffer));

  /* an corsaro flowtuple file will have 'SIX[TU]' at 14, 15, 16, 17 */
  if(len >= CORSARO_IO_INTERVAL_HEADER_BYTE_LEN+4 &&
     buffer[i++] == 'S' && buffer[i++] == 'I' &&
     buffer[i++] == 'X' && (buffer[i] == 'T' || buffer[i] == 'U'))
    {
      return 1;
    }

  return 0;
}

/** Implements the init_output function of the plugin API */
int corsaro_flowtuple_init_output(corsaro_t *corsaro)
{
  int i;
  struct corsaro_flowtuple_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_flowtuple_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro,
		"could not malloc corsaro_flowtuple_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* set sort to it's default value */
  state->sort_enabled = CORSARO_FLOWTUPLE_SORT_DEFAULT;

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      return -1;
    }

  /* defer opening the output file until we start the first interval */

  for(i = 0; i <= CORSARO_FLOWTUPLE_CLASS_MAX; i++)
    {
      assert(state->st_hash[i] == NULL);
      state->st_hash[i] = kh_init(sixt);
      assert(state->st_hash[i] != NULL);
    }

  return 0;

 err:
  corsaro_flowtuple_close_output(corsaro);
  return -1;
}

/** Implements the init_input function of the plugin API */
int corsaro_flowtuple_init_input(corsaro_in_t *corsaro)
{
  struct corsaro_flowtuple_in_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_flowtuple_in_state_t))) == NULL)
    {
      corsaro_log_in(__func__, corsaro,
		"could not malloc corsaro_flowtuple_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* we initially expect an corsaro interval record */
  state->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START;

  /* don't set the tuple_cnt until we actually see a class start record */

  return 0;

 err:
  corsaro_flowtuple_close_input(corsaro);
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_flowtuple_close_input(corsaro_in_t *corsaro)
{
  struct corsaro_flowtuple_in_state_t *state = STATE_IN(corsaro);

  if(state != NULL)
    {
      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }
  return 0;
}

/** Implements the close_output function of the plugin API */
int corsaro_flowtuple_close_output(corsaro_t *corsaro)
{
  int i;
  struct corsaro_flowtuple_state_t *state = STATE(corsaro);

  if(state != NULL)
    {
      for(i = 0; i <= CORSARO_FLOWTUPLE_CLASS_MAX; i++)
	{
	  if(state->st_hash[i] != NULL)
	    {
	      /* NB: flowtuples are freed in the dump functions */
	      kh_destroy(sixt, state->st_hash[i]);
	      state->st_hash[i] = NULL;
	    }
	}

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
off_t corsaro_flowtuple_read_record(struct corsaro_in *corsaro,
			       corsaro_in_record_type_t *record_type,
			       corsaro_in_record_t *record)
{

  struct corsaro_flowtuple_in_state_t *state = STATE_IN(corsaro);

  off_t bytes_read = -1;

  /* we have 5 different types of records that could be in this file */
  switch(state->expected_type)
    {
    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_interval_start(corsaro, corsaro->file,
					  record_type, record);
      if(bytes_read == sizeof(corsaro_interval_t))
	{
	  state->expected_type = CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START;
	}
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START:
      /* we'll handle this one */
      bytes_read = read_class_start(corsaro, record_type, record);
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE:
      /* we'll handle this too */
      bytes_read = read_flowtuple(corsaro, record_type, record);
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END:
      /* as with this */
      bytes_read = read_class_end(corsaro, record_type, record);
      break;

    case CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END:
      /* ask the io subsystem to read it for us */
      bytes_read = corsaro_io_read_interval_end(corsaro, corsaro->file,
					      record_type, record);
      if(bytes_read == sizeof(corsaro_interval_t))
	{
	  state->expected_type = CORSARO_IN_RECORD_TYPE_IO_INTERVAL_START;
	}
      break;

    default:
      corsaro_log_in(__func__, corsaro, "invalid expected record type");
    }

  return bytes_read;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_flowtuple_read_global_data_record(struct corsaro_in *corsaro,
			      enum corsaro_in_record_type *record_type,
			      struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_flowtuple_start_interval(corsaro_t *corsaro,
				     corsaro_interval_t *int_start)
{
  if(STATE(corsaro)->outfile == NULL)
    {
      if((
	  STATE(corsaro)->outfile_p[STATE(corsaro)->outfile_n] =
	  corsaro_io_prepare_file(corsaro,
				  PLUGIN(corsaro)->name,
				  int_start)) == NULL)
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
int corsaro_flowtuple_end_interval(corsaro_t *corsaro,
				   corsaro_interval_t *int_end)
{
  int i;
  struct corsaro_flowtuple_state_t *state = STATE(corsaro);

  corsaro_io_write_interval_start(corsaro, state->outfile,
				  &corsaro->interval_start);

  for(i = 0; i <= CORSARO_FLOWTUPLE_CLASS_MAX; i++)
    {
      assert(state->st_hash[i] != NULL);

      if(
	 (CORSARO_FILE_MODE(state->outfile)
	  == CORSARO_FILE_MODE_BINARY &&
	  binary_dump(corsaro, i) != 0)
	 ||
	 (CORSARO_FILE_MODE(state->outfile)
	  == CORSARO_FILE_MODE_ASCII &&
	  ascii_dump(corsaro, i) != 0)
	 )
	{
	  corsaro_log(__func__, corsaro, "could not dump hash");
	  return -1;
	}

      /* free all of the flowtuples */
      /* 10/25/12 ak optimizes slightly by moving the frees to the dump
	 functions. makes it harder to maintain, but we dont have to
	 walk the hash twice */
      /*kh_free(sixt, state->st_hash[i], &corsaro_flowtuple_free);*/
      kh_clear(sixt, state->st_hash[i]);
    }
  corsaro_io_write_interval_end(corsaro, state->outfile, int_end);

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
int corsaro_flowtuple_process_packet(corsaro_t *corsaro,
				     corsaro_packet_t *packet)
{
  libtrace_packet_t *ltpacket = LT_PKT(packet);
  libtrace_ip_t *ip_hdr = NULL;
  libtrace_icmp_t *icmp_hdr = NULL;
  libtrace_tcp_t *tcp_hdr = NULL;
  corsaro_flowtuple_t  t;
  int class;

  /* no point carrying on if a previous plugin has already decided we should
     ignore this packet */
  if((packet->state.flags & CORSARO_PACKET_STATE_IGNORE) != 0)
    {
      return 0;
    }

  if((ip_hdr = trace_get_ip(ltpacket)) == NULL)
    {
      /* non-ipv4 packet */
      return 0;
    }

  t.ip_len = ip_hdr->ip_len;

  t.src_ip = ip_hdr->ip_src.s_addr;
  CORSARO_FLOWTUPLE_IP_TO_SIXT(ip_hdr->ip_dst.s_addr, &t);

  t.protocol = ip_hdr->ip_p;
  t.tcp_flags = 0; /* in case we don't find a tcp header */

  t.ttl = ip_hdr->ip_ttl;

  if(ip_hdr->ip_p == TRACE_IPPROTO_ICMP &&
     (icmp_hdr = trace_get_icmp(ltpacket)) != NULL)
    {
      t.src_port = htons(icmp_hdr->type);
      t.dst_port = htons(icmp_hdr->code);
    }
  else
    {
      if(ip_hdr->ip_p == TRACE_IPPROTO_TCP &&
	 (tcp_hdr = trace_get_tcp(ltpacket)) != NULL)
	{
	  /* we have ignore the NS flag because it doesn't fit in
	     an 8 bit field. blame alberto (ak - 2/2/12) */
	  t.tcp_flags = (
			 (tcp_hdr->cwr << 7) |
			 (tcp_hdr->ece << 6) |
			 (tcp_hdr->urg << 5) |
			 (tcp_hdr->ack << 4) |
			 (tcp_hdr->psh << 3) |
			 (tcp_hdr->rst << 2) |
			 (tcp_hdr->syn << 1) |
			 (tcp_hdr->fin << 0)
			 );
	}

      t.src_port = htons(trace_get_source_port(ltpacket));
      t.dst_port = htons(trace_get_destination_port(ltpacket));
    }

  /* classify this packet and increment the appropriate hash */
  if((class = flowtuple_classify_packet(corsaro, ltpacket)) < 0)
    {
      corsaro_log(__func__, corsaro, "could not classify packet");
      return -1;
    }

  if(class == CORSARO_FLOWTUPLE_CLASS_BACKSCATTER)
    {
      packet->state.flags |= CORSARO_PACKET_STATE_FLAG_BACKSCATTER;
    }

  if(corsaro_flowtuple_add_inc(STATE(corsaro)->st_hash[class], &t, 1) != 0)
    {
      corsaro_log(__func__, corsaro,
		  "could not increment value for flowtuple");
      return -1;
    }
  return 0;
}

/** Implements the process_flowtuple function of the plugin API */
int corsaro_flowtuple_process_flowtuple(corsaro_t *corsaro,
					corsaro_flowtuple_t *flowtuple,
					corsaro_packet_state_t *state)
{
  /* no point carrying on if a previous plugin has already decided we should
     ignore this tuple */
  if((state->flags & CORSARO_PACKET_STATE_IGNORE) != 0)
    {
      return 0;
    }

  if(corsaro_flowtuple_add_inc(STATE(corsaro)->
			       st_hash[STATE(corsaro)->current_class],
			       flowtuple,
			       ntohl(flowtuple->packet_cnt)) != 0)
	{
	  corsaro_log(__func__, corsaro,
		      "could not increment value for flowtuple");
	  return -1;
	}
  return 0;
}

/** Implements the process_flowtuple_class_start function of the plugin API */
int corsaro_flowtuple_process_flowtuple_class_start(corsaro_t *corsaro,
					corsaro_flowtuple_class_start_t *class)
{
  STATE(corsaro)->current_class = class->class_type;
  return 0;
}

/** Implements the process_flowtuple_class_end function of the plugin API */
int corsaro_flowtuple_process_flowtuple_class_end(corsaro_t *corsaro,
					corsaro_flowtuple_class_end_t *class)
{
  /* we just need the class starts really */
  return 0;
}

/* ==== FlowTuple External Convenience Functions ==== */

/** Check if an input file is a FlowTuple file */
int corsaro_flowtuple_probe_file(corsaro_in_t *corsaro, const char *fturi)
{
  corsaro_file_in_t *ifile = NULL;
  int res;

  if(corsaro_flowtuple_probe_filename(fturi) != 0)
    {
      return 1;
    }

  /* open the file */
  if((ifile = corsaro_file_ropen(fturi)) == NULL)
    {
      /* this simply means that it is likely a libtrace file */
      return 0;
    }
  res = corsaro_flowtuple_probe_magic(corsaro, ifile);

  corsaro_file_rclose(ifile);
  return res;
}

/** Convenience function to get the source IP address from a FlowTuple */
uint32_t corsaro_flowtuple_get_source_ip(corsaro_flowtuple_t *flowtuple)
{
  assert(flowtuple != NULL);

  return flowtuple->src_ip;
}

/** Convenience function to get the destination IP address from a FlowTuple */
uint32_t corsaro_flowtuple_get_destination_ip(corsaro_flowtuple_t *flowtuple)
{
  assert(flowtuple != NULL);

  return CORSARO_FLOWTUPLE_SIXT_TO_IP(flowtuple);
}

/** Print a flowtuple to a file in ASCII format */
off_t corsaro_flowtuple_fprint(corsaro_t *corsaro, corsaro_file_t *file,
			   corsaro_flowtuple_t *flowtuple)
{
  char ip_a[INET_ADDRSTRLEN];
  char ip_b[INET_ADDRSTRLEN];
  uint32_t tmp;

  assert(corsaro != NULL);
  assert(file != NULL);
  assert(flowtuple != NULL);

  tmp = flowtuple->src_ip;
  inet_ntop(AF_INET,&tmp, &ip_a[0], 16);
  tmp = CORSARO_FLOWTUPLE_SIXT_TO_IP(flowtuple);
  inet_ntop(AF_INET, &tmp, &ip_b[0], 16);

  return corsaro_file_printf(corsaro, file, "%s|%s"
			   "|%"PRIu16"|%"PRIu16
			   "|%"PRIu8"|%"PRIu8"|0x%02"PRIx8
			   "|%"PRIu16
			   ",%"PRIu32"\n",
			   ip_a, ip_b,
			   ntohs(flowtuple->src_port),
			   ntohs(flowtuple->dst_port),
			   flowtuple->protocol,
			   flowtuple->ttl,
			   flowtuple->tcp_flags,
			   ntohs(flowtuple->ip_len),
			   ntohl(flowtuple->packet_cnt));
}

/** Print a FlowTuple to stdout in ASCII format */
void corsaro_flowtuple_print(corsaro_flowtuple_t *flowtuple)
{
  char ip_a[16];
  char ip_b[16];
  uint32_t tmp;

  assert(flowtuple != NULL);

  tmp = flowtuple->src_ip;
  inet_ntop(AF_INET,&tmp, &ip_a[0], 16);
  tmp = CORSARO_FLOWTUPLE_SIXT_TO_IP(flowtuple);
  inet_ntop(AF_INET, &tmp, &ip_b[0], 16);

  fprintf(stdout, "%s|%s"
	 "|%"PRIu16"|%"PRIu16
	 "|%"PRIu8"|%"PRIu8"|0x%02"PRIx8
	 "|%"PRIu16
	 ",%"PRIu32"\n",
	 ip_a, ip_b,
	 ntohs(flowtuple->src_port),
	 ntohs(flowtuple->dst_port),
	 flowtuple->protocol,
	 flowtuple->ttl,
	 flowtuple->tcp_flags,
	 ntohs(flowtuple->ip_len),
	 ntohl(flowtuple->packet_cnt));
}

/** Print a class start record to a file in ASCII format */
off_t corsaro_flowtuple_class_start_fprint(corsaro_t *corsaro,
				   corsaro_file_t *file,
				   corsaro_flowtuple_class_start_t *class)
{
  return corsaro_file_printf(corsaro, file,
			   "START %s %"PRIu32"\n",
			   class_names[class->class_type],
			   class->count);
}

/** Print a class start record to stdout in ASCII format */
void corsaro_flowtuple_class_start_print(corsaro_flowtuple_class_start_t *class)
{
  fprintf(stdout, "START %s %"PRIu32"\n", class_names[class->class_type],
	  class->count);
}

/** Print a class end record to a file in ASCII format */
off_t corsaro_flowtuple_class_end_fprint(corsaro_t *corsaro,
					 corsaro_file_t *file,
				      corsaro_flowtuple_class_end_t *class)
{
  return corsaro_file_printf(corsaro, file, "END %s\n",
			   class_names[class->class_type]);
}

/** Print a class end record to stdout in ASCII format */
void corsaro_flowtuple_class_end_print(corsaro_flowtuple_class_end_t *class)
{
  fprintf(stdout, "END %s\n", class_names[class->class_type]);
}

/** Print a record to a file in ASCII format */
off_t corsaro_flowtuple_record_fprint(corsaro_t *corsaro, corsaro_file_t *file,
				   corsaro_in_record_type_t record_type,
				   corsaro_in_record_t *record)
{
  switch(record_type)
    {
    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START:
      return corsaro_flowtuple_class_start_fprint(corsaro, file,
                            (corsaro_flowtuple_class_start_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END:
      return corsaro_flowtuple_class_end_fprint(corsaro, file,
			    (corsaro_flowtuple_class_end_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE:
      return corsaro_flowtuple_fprint(corsaro, file,
				   (corsaro_flowtuple_t *)record->buffer);
      break;

    default:
      corsaro_log(__func__, corsaro, "record_type %d not a flowtuple record",
		record_type);
      return -1;
      break;
    }

  return -1;
}

/** Print a record to stdout in ASCII format */
int corsaro_flowtuple_record_print(corsaro_in_record_type_t record_type,
				corsaro_in_record_t *record)
{
  switch(record_type)
    {
    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_START:
      corsaro_flowtuple_class_start_print(
			(corsaro_flowtuple_class_start_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_CLASS_END:
      corsaro_flowtuple_class_end_print(
			    (corsaro_flowtuple_class_end_t *)record->buffer);
      break;

    case CORSARO_IN_RECORD_TYPE_FLOWTUPLE_FLOWTUPLE:
      corsaro_flowtuple_print((corsaro_flowtuple_t *)record->buffer);
      break;

    default:
      corsaro_log_file(__func__, NULL, "record_type %d not a flowtuple record",
		record_type);
      return -1;
      break;
    }

  return 0;
}

/** Free a FlowTuple record */
void inline corsaro_flowtuple_free(corsaro_flowtuple_t *t)
{
  free(t);
}

/** Either add the given flowtuple to the hash, or increment the current count */
int corsaro_flowtuple_add_inc(void *h, corsaro_flowtuple_t *t,
			      uint32_t increment)
{
  kh_sixt_t *hash = (kh_sixt_t *)h;
  int khret;
  khiter_t khiter;
  corsaro_flowtuple_t *new_6t = NULL;

  assert(hash != NULL);

  /* check if this is in the hash already */
  if((khiter = kh_get(sixt, hash, t)) == kh_end(hash))
    {
      /* create a new tuple struct */
      if((new_6t = malloc(sizeof(corsaro_flowtuple_t))) == NULL)
	{
	  corsaro_log_file(__func__, NULL, "malloc failed");
	  return -1;
	}

      /* fill it */
      memcpy(new_6t, t, sizeof(corsaro_flowtuple_t));

      /* add it to the hash */
      khiter = kh_put(sixt, hash, new_6t, &khret);
      /* set the count to one */
      /*kh_value(hash, khiter) = increment;*/
      new_6t->packet_cnt = htonl(increment);
    }
  else
    {
      /* simply increment the existing one */
      /*kh_value(hash, khiter)+=increment;*/
      new_6t = kh_key(hash, khiter);

      /* will this cause a wrap? */
      assert((UINT32_MAX - ntohl(new_6t->packet_cnt)) > increment);

      new_6t->packet_cnt = htonl(ntohl(new_6t->packet_cnt)+increment);
    }
  return 0;
}
