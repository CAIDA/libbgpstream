/* 
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * alistair@caida.org
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
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "libtrace.h"

#include "khash.h"
#include "patricia.h"
#include "ip_utils.h"
#include "utils.h"

#include "corsaro_io.h"
#include "corsaro_file.h"
#include "corsaro_geo.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

#include "corsaro_pfx2as.h"

/** @file
 *
 * @brief Corsaro plugin that maps ip address to AS Numbers
 *
 * @author Karyn Benson
 * @author Alistair King
 *
 */

/*#define CORSARO_PFX2AS_DEBUG*/

#ifdef CORSARO_PFX2AS_DEBUG
/** The number of cache hits */
static int debug_cache_hits = 0;
/** The number of cache misses */
static int debug_cache_misses = 0;
#endif

/** The magic number for this plugin - "AS##" */
#define CORSARO_PFX2AS_MAGIC 0x41532323

/** The name of this plugin - should match the file name */
#define PLUGIN_NAME "pfx2as"

/** Initialize the hash type (32bit keys, geo_record values) */
KHASH_MAP_INIT_INT(32record, corsaro_geo_record_t *)
/** Initialize the map type (string keys, geo_record values */
KHASH_MAP_INIT_STR(strrec, corsaro_geo_record_t *)

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_pfx2as_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_PFX2AS,                         /* id */
  CORSARO_PFX2AS_MAGIC,                             /* magic */
#ifdef WITH_PLUGIN_SIXT
  CORSARO_PLUGIN_GENERATE_PTRS_FT(corsaro_pfx2as),  /* func ptrs */
#else
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_pfx2as),
#endif
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct corsaro_pfx2as_state_t {
  corsaro_geo_provider_t *provider;

  /** should the as cache be used */
  int cache_enabled;
  /** The hashtable that we will use to cache the ASNs */
  khash_t(32record)   *as_cache;

  /** The CAIDA pfx2as file to use */
  char *pfx2as_file;
};

/** The length of the line buffer when reading pfx2as files */
#define BUFFER_LEN 1024

/** The number of columns in a pfx2as file */
#define PFX2AS_COL_CNT 3

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)							\
  (CORSARO_PLUGIN_STATE(corsaro, pfx2as, CORSARO_PLUGIN_ID_PFX2AS))
/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_PFX2AS))

/** Print plugin usage to stderr */
static void usage(corsaro_plugin_t *plugin)
{
  fprintf(stderr, 
	  "plugin usage: %s [-c] -f pfx2as_file\n"
	  "       -c            cache the results for each IP\n"
	  "       -f            pfx2as file to use for lookups\n",
	  plugin->argv[0]);
}

/** Parse the arguments given to the plugin */
static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_pfx2as_state_t *state = STATE(corsaro);
  int opt;

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, "f:c?")) >= 0)
    {
      switch(opt)
	{
	case 'c':
	  state->cache_enabled = 1;
	  break;

	case 'f':
	  state->pfx2as_file = optarg;
	  break;

	case '?':
	case ':':
	default:
	  usage(plugin);
	  return -1;
	}
    }

  if(state->pfx2as_file == NULL)
    {
      fprintf(stderr, "ERROR: %s requires '-f' to be specified\n",
	      plugin->argv[0]);
      usage(plugin);
      return -1;
    }

  return 0;
}

/** Parse an underscore-separated list of ASNs */
static int parse_asn(char *asn_str, uint32_t **asn_arr)
{
  int asn_cnt = 0;
  uint32_t *asn = NULL;
  char *tok = NULL;
  char *period = NULL;

  while((tok = strsep(&asn_str, "_")) != NULL)
    {      
      /* realloc the asn array to buy us one more */
      if((asn = realloc(asn, sizeof(uint32_t) * (asn_cnt+1))) == NULL)
	{
	  if(asn != NULL)
	    {
	      free(asn);
	    }
	  return -1;
	}

      /* check if this is a 32bit asn */
      if((period = strchr(tok, '.')) != NULL)
	{
	  /* set this to a nul */
	  *period = '\0';
	  /* get the value of the first 16 bits and the second */
	  asn[asn_cnt] = (atoi(tok)<<16) | atoi(period+1);
	}
      else
	{
	  /* do a simple atoi and be done */
	  asn[asn_cnt] = atoi(tok);
	}
      asn_cnt++;
    }

  /* return the array of asn values and the count */
  *asn_arr = asn;
  return asn_cnt;
}

/** Free a string (for use with the map) */
static inline void str_free(const char *str)
{
  free((char*)str);
}

/** Read the prefix2as file */
static int read_routeviews(corsaro_t *corsaro, 
			   corsaro_file_in_t *file)
{
  struct corsaro_pfx2as_state_t *state = STATE(corsaro);

  /* we have to normalize the asns on the fly */
  /* so we read the file, reading records into here */
  int khret;
  khiter_t khiter;
  khash_t(strrec)   *asn_table = kh_init(strrec);
  
  char buffer[BUFFER_LEN];
  char *rowp;
  char *tok = NULL;
  int tokc = 0;

  int asn_id = 0;
  in_addr_t addr = 0;
  uint8_t mask = 0;
  uint32_t *asn = NULL;
  char *asn_str = NULL;
  int asn_cnt = 0;

  corsaro_geo_record_t *record;

  while(corsaro_file_rgets(file, &buffer, BUFFER_LEN) > 0)
    {
      rowp = buffer;
      tokc = 0;

      /* hack off the newline */
      chomp(buffer);
      
      while((tok = strsep(&rowp, "\t")) != NULL)
	{
	  switch(tokc)
	    {
	    case 0:
	      /* network */
	      addr = inet_addr(tok);
	      break;

	    case 1:
	      /* mask */
	      mask = atoi(tok);
	      break;

	    case 2:
	      /* asn */
	      asn_str = strdup(tok);
	      asn_cnt = parse_asn(tok, &asn);
	      break;

	    default:
	      corsaro_log(__func__, corsaro, "invalid pfx2as file");
	      return -1;
	      break;
	    }
	  tokc++;
	}

      if(tokc != PFX2AS_COL_CNT)
	{
	  corsaro_log(__func__, corsaro, "invalid pfx2as file");
	  return -1;
	}
      
      if(asn_cnt <= 0 || asn_str == NULL)
	{
	  corsaro_log(__func__, corsaro, "could not parse asn string");
	  return -1;
	}

      /* check our hash for this asn */
      if((khiter = kh_get(strrec, asn_table, asn_str)) == kh_end(asn_table))
	{
	  /* need to create a record for this asn */
	  if((record = corsaro_geo_init_record(state->provider, asn_id))
	     == NULL)
	    {
	      corsaro_log(__func__, corsaro, "could not alloc geo record");
	      return -1;
	    }

	  /* set the fields */
	  record->asn = asn;
	  record->asn_cnt = asn_cnt;

	  /* put it into our table */
	  khiter = kh_put(strrec, asn_table, asn_str, &khret);
	  kh_value(asn_table, khiter) = record;

	  /* move on to the next id */
	  asn_id++;
	}
      else
	{
	  /* we've seen this ASN before, just use that! */
	  record = kh_value(asn_table, khiter);
	  assert(record != NULL);
	  /* BUT! remember that we strdup'd the asn string */
	  /* add that parse_asn did some malloc'ing */
	  free(asn_str);
	  free(asn);
	  asn = NULL;
	  asn_cnt = 0;
	}

      assert(record != NULL);

      /* how many IP addresses does this prefix cover ? */
      /* we will add this to the record and then use the total count for the asn
	 to find the 'biggest' ASes */
      record->asn_ip_cnt += (ip_broadcast_addr(addr, mask) - 
			       ip_network_addr(addr, mask)) + 1;
      
      /* by here record is the right asn record, associate it with this pfx */
      if(corsaro_geo_provider_associate_record(corsaro,
					       state->provider,
					       addr,
					       mask,
					       record) != 0)
	{
	  corsaro_log(__func__, corsaro, "failed to associate record");
	  return -1;
	}
    }

  /* free our asn_table hash */
  kh_free(strrec, asn_table, str_free);
  kh_destroy(strrec, asn_table);

  return 0;
  
}

/** Get an ASN record from the cache given an IP address */
static corsaro_geo_record_t *cache_get(kh_32record_t *hash, uint32_t ip)
{
  khiter_t khiter;
  assert(hash != NULL);
  
  if((khiter = kh_get(32record, hash, ip)) == kh_end(hash))
    {
      return NULL;
    }
  return kh_value(hash, khiter);
}

/** Add an ASN record to the hash for the given IP address */
static void cache_add(kh_32record_t *hash, uint32_t ip, 
		      corsaro_geo_record_t *record)
{
  int khret;
  khiter_t khiter;

  /* we assume that if you call this you have checked it is not in the hash */
  khiter = kh_put(32record, hash, ip, &khret);
  kh_value(hash, khiter) = record;
}

/** Common code between process_packet and process_flowtuple */
static int process_generic(corsaro_t *corsaro, corsaro_packet_state_t *state,
			   uint32_t src_ip)
{
  struct corsaro_pfx2as_state_t *plugin_state = STATE(corsaro);
  corsaro_geo_record_t *record;

  /* remove the old record from the provider */
  corsaro_geo_provider_clear(plugin_state->provider);

  /* note that the ip address are in network byte order */

if(plugin_state->cache_enabled != 0)
  {
    /* first we'll check the hashtable (if it is enabled) */
    if((record = cache_get(plugin_state->as_cache, src_ip)) == NULL)
      {
	/* its not in the cache - look in the trie */
	
	record = corsaro_geo_provider_lookup_record(corsaro, 
						    plugin_state->provider,
						    src_ip);
	
	cache_add(plugin_state->as_cache, src_ip, record);	  
	
#ifdef CORSARO_PFX2AS_DEBUG
	debug_cache_misses++;      
      }
    else
      {
	debug_cache_hits++;
#endif
      }
  }
 else
   {
     /* go straight to the trie */
     record = corsaro_geo_provider_lookup_record(corsaro, 
						 plugin_state->provider,
						 src_ip);
   }
 
    /* insert the record into the matched records for this packet */
    corsaro_geo_provider_add_record(plugin_state->provider, record);

#if 0
    /** todo move this into a nice generic 'geodump' plugin */
    /* DEBUG */
    corsaro_geo_provider_t *provider;
    corsaro_geo_record_t *tmp = NULL;
    struct in_addr addr;
    addr.s_addr = src_ip;
    
    fprintf(stdout, "src: %s\n",
	    inet_ntoa(addr));
    
    /* first, ask for the pfx2as provider */
    provider = corsaro_geo_get_by_id(corsaro, CORSARO_GEO_PROVIDER_PFX2AS);
    assert(provider != NULL);
    
  while((tmp = corsaro_geo_next_record(provider, tmp)) != NULL)
    {
      corsaro_geo_dump_record(tmp);
    }
#endif

  return 0;

}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_pfx2as_alloc(corsaro_t *corsaro)
{
  return &corsaro_pfx2as_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_pfx2as_probe_filename(const char *fname)
{
  /* look for 'corsaro_pfx2as' in the name */
  return corsaro_plugin_probe_filename(fname, &corsaro_pfx2as_plugin);
}

/** Implements the probe_magic function of the plugin API */
int corsaro_pfx2as_probe_magic(corsaro_in_t *corsaro, corsaro_file_in_t *file)
{
  /* we write no output files, so dont even bother looking */
  return 0;
}

/** Implements the init_output function of the plugin API */
int corsaro_pfx2as_init_output(corsaro_t *corsaro)
{
  struct corsaro_pfx2as_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  corsaro_file_in_t *file = NULL;
  assert(plugin != NULL);
 
  if((state = malloc_zero(sizeof(struct corsaro_pfx2as_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		  "could not malloc corsaro_pfx2as_state_t");
      goto err;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      /* parse args calls usage itself, so do not goto err here */
      return -1;
    }

  /* register as a geoloc provider */
  /* we do not want to be the default provider as we do not provide full
     geo data */
  if((state->provider = 
      corsaro_geo_init_provider(corsaro,
				CORSARO_GEO_PROVIDER_PFX2AS,
				CORSARO_GEO_DATASTRUCTURE_DEFAULT,
				CORSARO_GEO_PROVIDER_DEFAULT_NO)) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not register as a geolocation"
		  " provider");
      goto err;
    }
  
  /* open the routeviews file */
  if((file = corsaro_file_ropen(state->pfx2as_file)) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		  "failed to open pfx2as file '%s'", state->pfx2as_file);
      goto err;
    }

  /* read in the routeviews mapping from network to pfx2as and store in patricia
     tree */
  if (read_routeviews(corsaro, file) != 0)
    {
      corsaro_log(__func__, corsaro, "could not read pfx2as file '%s'", 
		  state->pfx2as_file);
      goto err;
    }

  corsaro_file_rclose(file);

  if(state->cache_enabled != 0)
    {
      /* initialize the hash cache */
      state->as_cache = kh_init(32record);
    }
  
  return 0;

 err:
  corsaro_pfx2as_close_output(corsaro);
  return -1;
}

/** Implements the init_input function of the plugin API */
int corsaro_pfx2as_init_input(corsaro_in_t *corsaro)
{
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_pfx2as_close_input(corsaro_in_t *corsaro)
{
  return -1;
}

/** Implements the close_output function of the plugin API */
int corsaro_pfx2as_close_output(corsaro_t *corsaro)
{
  struct corsaro_pfx2as_state_t *state = STATE(corsaro);

  if(state != NULL)
    {
      if(state->provider != NULL)
	{
	  corsaro_geo_free_provider(corsaro, state->provider);
	  state->provider = NULL;
	}

      if(state->as_cache != NULL)
	{
	  kh_destroy(32record, state->as_cache);
	  state->as_cache = NULL;
	}
      
      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }

#ifdef CORSARO_PFX2AS_DEBUG
  corsaro_log(__func__, corsaro, "cache hits: %d misses: %d", 
	      debug_cache_hits, debug_cache_misses);
#endif
  
  return 0;
}

/** Implements the read_record function of the plugin API */
off_t corsaro_pfx2as_read_record(struct corsaro_in *corsaro, 
				corsaro_in_record_type_t *record_type, 
				corsaro_in_record_t *record)
{
  /* This plugin writes no output data */
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_pfx2as_read_global_data_record(struct corsaro_in *corsaro, 
					    enum corsaro_in_record_type *record_type, 
					    struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_pfx2as_start_interval(corsaro_t *corsaro, 
				  corsaro_interval_t *int_start)
{
  /* we don't care */
  return 0;
}

/** Implements the end_interval function of the plugin API */
int corsaro_pfx2as_end_interval(corsaro_t *corsaro, corsaro_interval_t *int_end)
{
  /* we don't care */
  return 0;
}

/** Implements the process_packet function of the plugin API */
int corsaro_pfx2as_process_packet(corsaro_t *corsaro, 
				 corsaro_packet_t *packet)
{
  libtrace_ip_t  *ip_hdr  = NULL;

  if((ip_hdr = trace_get_ip(LT_PKT(packet))) == NULL)
    {
      /* not an ip packet */
      return 0;
    }

  return process_generic(corsaro, &packet->state, ip_hdr->ip_src.s_addr);
}

#ifdef WITH_PLUGIN_SIXT
/** Implements the process_flowtuple function of the plugin API */
int corsaro_pfx2as_process_flowtuple(corsaro_t *corsaro,
				     corsaro_flowtuple_t *flowtuple,
				     corsaro_packet_state_t *state)
{
  return process_generic(corsaro, state,
			 corsaro_flowtuple_get_source_ip(flowtuple));
}

/** Implements the process_flowtuple_class_start function of the plugin API */
int corsaro_pfx2as_process_flowtuple_class_start(corsaro_t *corsaro,
				   corsaro_flowtuple_class_start_t *class)
{
  /* we dont care about these */
  return 0;
}

/** Implements the process_flowtuple_class_end function of the plugin API */
int corsaro_pfx2as_process_flowtuple_class_end(corsaro_t *corsaro,
				   corsaro_flowtuple_class_end_t *class)
{
  /* dont care */
  return 0;
}
#endif
