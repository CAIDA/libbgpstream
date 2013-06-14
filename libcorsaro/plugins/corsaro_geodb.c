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

#include "ip_utils.h"
#include "khash.h"
#include "patricia.h"
#include "utils.h"

#include "corsaro_geo.h"
#include "corsaro_log.h"
#include "corsaro_plugin.h"

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

#include "corsaro_geodb.h"

/** @file
 *
 * @brief Corsaro CSV Database plugin
 *
 * This plugin is designed to tag packets based on the geolocation data found in
 * the Maxmind Geo CSV format databases. That is, a database which consists of
 * two tables: Blocks and Locations. See http://dev.maxmind.com/geoip/geolite
 * for the free GeoLite versions of these databases.
 * 
 * It has been extended to understand the NetAcuity Edge database also, but only
 * once it has been converted to this format. Contact corsaro-info@caida.org
 * for more information about this feature.
 *
 * @author Alistair King
 *
 */

/** The magic number for this plugin - "GODB" */
#define CORSARO_GEODB_MAGIC 0x474F4442

/** The name of this plugin */
#define PLUGIN_NAME "geodb"

#define MAXMIND_NAME \
  (corsaro_geo_get_provider_name(CORSARO_GEO_PROVIDER_MAXMIND))

#define NETACQ_EDGE_NAME \
  (corsaro_geo_get_provider_name(CORSARO_GEO_PROVIDER_NETACQ_EDGE))

#define DEFAULT_PROVIDER_NAME MAXMIND_NAME

/** The length of the static line buffer */
#define BUFFER_LEN 1024

/** The number of columns in the maxmind locations CSV file */
#define MAXMIND_LOCATION_COL_CNT 9

/** The number of header rows in the maxmind CSV files */
#define MAXMIND_HEADER_ROW_CNT 2

/** The number of columns in the blocks CSV file */
#define BLOCKS_COL_CNT 3

/** The number of columns in the netacq edge locations CSV file */
#define NETACQ_EDGE_LOCATION_COL_CNT 23

/** The number of header rows in the netacq edge CSV files */
#define NETACQ_EDGE_HEADER_ROW_CNT 1

/** The default file name for the locations file */
#define LOCATIONS_FILE_NAME "GeoLiteCity-Location.csv.gz"

/** The default file name for the blocks file */
#define BLOCKS_FILE_NAME "GeoLiteCity-Blocks.csv.gz"

KHASH_INIT(u16u16, uint16_t, uint16_t, 1, 
	   kh_int_hash_func, kh_int_hash_equal)

/** Common plugin information across all instances */
static corsaro_plugin_t corsaro_geodb_plugin = {
  PLUGIN_NAME,                                 /* name */
  CORSARO_PLUGIN_ID_GEODB,                      /* id */
  CORSARO_GEODB_MAGIC,                          /* magic */
#ifdef WITH_PLUGIN_SIXT
  CORSARO_PLUGIN_GENERATE_PTRS_FT(corsaro_geodb),  /* func ptrs */
#else
  CORSARO_PLUGIN_GENERATE_PTRS(corsaro_geodb),
#endif
  CORSARO_PLUGIN_GENERATE_TAIL,
};

/** Holds the state for an instance of this plugin */
struct corsaro_geodb_state_t {
  corsaro_geo_provider_t *provider;

  /* Info extracted from args */
  corsaro_geo_provider_id_t provider_id;
  char *locations_file;
  char *blocks_file;

  /* hash that maps from country code to continent code */
  khash_t(u16u16) *country_continent;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)							\
  (CORSARO_PLUGIN_STATE(corsaro, geodb, CORSARO_PLUGIN_ID_GEODB))

/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_GEODB))

static void usage(corsaro_plugin_t *plugin)
{
  fprintf(stderr, 
	  "plugin usage: %s [-p format] (-l locations -b blocks)|(-d directory)\n"
	  "       -d            directory containing blocks and location files\n"
	  "       -b            blocks file (must be used with -l)\n"
	  "       -l            locations file (must be used with -b)\n"
	  "       -p            database format (default: %s)\n"
	  "                       format must be one of:\n"
	  "                       - %s\n"
	  "                       - %s\n",	  
	  plugin->argv[0],
	  DEFAULT_PROVIDER_NAME,
	  MAXMIND_NAME,
	  NETACQ_EDGE_NAME);
}

/** @todo add option to choose datastructure */
static int parse_args(corsaro_t *corsaro)
{
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  int opt;
  char *directory = NULL;
  char *ptr = NULL;

  assert(plugin->argc > 0 && plugin->argv != NULL);

  if(plugin->argc == 1)
    {
      usage(plugin);
      return -1;
    }

  /* NB: remember to reset optind to 1 before using getopt! */
  optind = 1;

  while((opt = getopt(plugin->argc, plugin->argv, "b:d:l:p:?")) >= 0)
    {
      switch(opt)
	{
	case 'b':
	  /* NB: although the storage for these strings belongs to us,
	     we strdup them so that we can free on cleanup to handle the
	     case where we dynamically build them from the directory path
	  */
	  state->blocks_file = strdup(optarg);
	  break;

	case 'd':
	  directory = optarg;
	  break;

	case 'l':
	  state->locations_file = strdup(optarg);
	  break;

	case 'p':
	  if(strncasecmp(optarg, MAXMIND_NAME, strlen(MAXMIND_NAME)) == 0)
	    {
	      state->provider_id = CORSARO_GEO_PROVIDER_MAXMIND;
	    }
	  else if(strncasecmp(optarg, NETACQ_EDGE_NAME, 
			      strlen(NETACQ_EDGE_NAME)) == 0)
	    {
	      state->provider_id = CORSARO_GEO_PROVIDER_NETACQ_EDGE;
	    }
	  else
	    {
	      fprintf(stderr, "ERROR: invalid database format (%s)\n",
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

  if(directory != NULL)
    {
      /* check if they were daft and specified explicit files too */
      if(state->locations_file != NULL || state->blocks_file != NULL)
	{
	  fprintf(stderr, "WARNING: both directory and file name specified.\n");

	  /* free up the dup'd strings */
	  if(state->locations_file != NULL)
	    {
	      free(state->locations_file);
	      state->locations_file = NULL;
	    }

	  if(state->blocks_file != NULL)
	    {
	      free(state->blocks_file);
	      state->blocks_file = NULL;
	    }
	}

      /* remove the trailing slash if there is one */
      if(directory[strlen(directory)-1] == '/')
	{
	  directory[strlen(directory)-1] = '\0';
	}

      /* malloc storage for the dir+/+file string */
      if((state->locations_file = malloc(
					 strlen(directory)+1+
					 strlen(LOCATIONS_FILE_NAME)+1)) 
	 == NULL)
	{
	  corsaro_log(__func__, corsaro, 
		      "could not malloc location file string");
	  return -1;
	}

      if((state->blocks_file = malloc(
				      strlen(directory)+1+
				      strlen(BLOCKS_FILE_NAME)+1)) 
	 == NULL)
	{
	  corsaro_log(__func__, corsaro, 
		      "could not malloc blocks file string");
	  return -1;
	}

      /** @todo make this check for both .gz and non-.gz files */

      ptr = stpncpy(state->locations_file, directory, strlen(directory));
      *ptr++ = '/';
      /* last copy needs a +1 to get the terminating nul. d'oh */
      ptr = stpncpy(ptr, LOCATIONS_FILE_NAME, strlen(LOCATIONS_FILE_NAME)+1);

      ptr = stpncpy(state->blocks_file, directory, strlen(directory));
      *ptr++ = '/';
      ptr = stpncpy(ptr, BLOCKS_FILE_NAME, strlen(BLOCKS_FILE_NAME)+1);
      
    }

  if(state->locations_file == NULL || state->blocks_file == NULL)
    {
      fprintf(stderr, "ERROR: %s requires either '-d' or both '-b' and '-l'\n",
	      plugin->argv[0]);
      usage(plugin);
      return -1;
    }

  if(state->provider_id == 0)
    {
      state->provider_id = CORSARO_GEO_PROVIDER_MAXMIND;
    }

  return 0;
}

/** @todo make use sscanf */
static int parse_maxmind_location_row(struct corsaro_geodb_state_t *state,
				      corsaro_geo_provider_t *provider, 
				      char *row)
{
  char *rowp = row;
  char *tok = NULL;
  int tokc = 0;
  corsaro_geo_record_t tmp;
  corsaro_geo_record_t *record;

  uint16_t cntry_code = 0;
  khiter_t khiter;

  /* make sure tmp is zeroed */
  memset(&tmp, 0, sizeof(corsaro_geo_record_t));

  while((tok = strsep(&rowp, ",")) != NULL)
    {
      switch(tokc)
	{
	case 0:
	  /* id */
	  /* init this record */
	  tmp.id = atoi(tok);
	  break;

	case 1:
	  /* country code */
	  if(tok[0] != '"' || tok[strlen(tok)-1] != '"')
	    {
	      return -1;
	    }
	  cntry_code = (tok[1]<<8) | tok[2];
	  memcpy(&tmp.country_code, tok+1, 2);
	  break;

	case 2:
	  /* region string */
	  if(tok[0] != '"' || tok[strlen(tok)-1] != '"')
	    {
	      return -1;
	    }
	  if(tok[1] != '"' && tok[2] != '"')
	    {
	      tmp.region[2] = '\0';
	      memcpy(&tmp.region, tok+1, 2);
	    }
	  else
	    {
	      tmp.region[0] = '\0';
	    }
	  break;
	     
	case 3:
	  /* city */
	  if(tok[0] != '"' || tok[strlen(tok)-1] != '"')
	    {
	      return -1;
	    }
	  tmp.city = strndup(tok+1, strlen(tok+1)-1);
	  break;
	      
	case 4:
	  /* postal code */
	  if(tok[0] != '"' || tok[strlen(tok)-1] != '"')
	    {
	      return -1;
	    }
	  tmp.post_code = strndup(tok+1, strlen(tok+1)-1);
	  break;

	case 5:
	  /* latitude */
	  tmp.latitude = atof(tok);
	  break;

	case 6:
	  /* longitude */
	  tmp.longitude = atof(tok);
	  break;

	case 7:
	  /* metro code - whatever the heck that is */
	  tmp.metro_code = atoi(tok);
	  break;
	      
	case 8:
	  /* area code - (phone) */
	  tmp.area_code = atoi(tok);
	  break;

	default:
	  return -1;
	  break;
	}
      tokc++;
    }

  /* at the end of successful row parsing, tokc will be 9 */
  /* make sure we parsed exactly as many columns as we anticipated */
  if(tokc != MAXMIND_LOCATION_COL_CNT)
    {
      fprintf(stderr, "ERROR: Expecting %d columns in the locations file, "
	      "but actually got %d\n", 
	      MAXMIND_LOCATION_COL_CNT, tokc);
      return -1;
    }

  /* look up the continent code */
  if((khiter = kh_get(u16u16, state->country_continent, cntry_code)) == 
     kh_end(state->country_continent))
    {
      fprintf(stderr, "ERROR: Invalid country code (%s)\n",
	      tmp.country_code);
      return -1;
    }

  tmp.continent_code = kh_value(state->country_continent, khiter);

  /*
  corsaro_log(__func__, NULL, "looking up %s (%x) got %x",
	      tmp.country_code, cntry_code, tmp.continent_code);
  */

  if((record = corsaro_geo_init_record(provider, tmp.id)) == NULL)
    {
      return -1;
    }

  memcpy(record, &tmp, sizeof(corsaro_geo_record_t));

  return 0;
}

static int read_maxmind_locations(corsaro_t *corsaro, 
				  corsaro_geo_provider_t *provider,
			  corsaro_file_in_t *file)
{
  char buffer[BUFFER_LEN];
  int linec = 0;
  
  while(corsaro_file_rgets(file, &buffer, BUFFER_LEN) > 0)
    {
      /* skip the first two lines */
      if(linec++ < MAXMIND_HEADER_ROW_CNT)
	{
	  continue;
	}

      chomp(buffer);

      if(parse_maxmind_location_row(STATE(corsaro), provider, buffer) != 0)
	{
	  corsaro_log(__func__, corsaro, "invalid maxmind location file");
	  return -1;
	}
    }
  
  return 0;
}

/** @todo make use sscanf */
static int parse_netacq_edge_location_row(corsaro_geo_provider_t *provider, 
					  char *row)
{
  char *rowp = row;
  char *tok = NULL;
  int tokc = 0;
  corsaro_geo_record_t tmp;
  corsaro_geo_record_t *record;

  /* make sure tmp is zeroed */
  memset(&tmp, 0, sizeof(corsaro_geo_record_t));

  while((tok = strsep(&rowp, ",")) != NULL)
    {
      switch(tokc)
	{
	case 0:
	  /* id */
	  /* init this record */
	  tmp.id = atoi(tok);
	  break;

	case 1: /* country 3 letter */
	case 2: /* region string */
	  break;

	case 3: /* city string */
	  tmp.city = strndup(tok, strlen(tok));
	  break;

	case 4: /* connection speed string */
	  tmp.conn_speed = strndup(tok, strlen(tok));
	  break;

	case 5: /* metro code */
	  /* metro code - whatever the heck that is */
	  tmp.metro_code = atoi(tok);
	  break;

	case 6: /* latitude */
	  tmp.latitude = atof(tok);
	  break;

	case 7: /* longitude */
	  tmp.longitude = atof(tok);
	  break;

	case 8: /* postal code */
	  tmp.post_code = strndup(tok, strlen(tok));
	  break;

	case 9: /* country code */
	case 10: /* region code */
	case 11: /* city code */
	  break;

	case 12: /* continent code */
	  tmp.continent_code = atoi(tok);

	case 13: /* country code - 2 letter */
	  memcpy(&tmp.country_code, tok, 2);
	  break;

	case 14: /* internal code - wtf? */
	case 15: /* area codes (plural) */
	case 16: /* country-conf ?? */
	case 17: /* region-conf */
	case 18: /* city-conf */
	case 19: /* postal-conf */
	case 20: /* gmt-offset */
	case 21: /* in-dst */
	  break;

	case 22: /* seems they use a trailing comma */
	  break;

	default:
	  return -1;
	  break;
	}
      tokc++;
    }

  /* at the end of successful row parsing, tokc will be 22 */
  /* make sure we parsed exactly as many columns as we anticipated */
  if(tokc != NETACQ_EDGE_LOCATION_COL_CNT)
    {
      fprintf(stderr, "ERROR: Expecting %d columns in the locations file, "
	      "but actually got %d\n", 
	      NETACQ_EDGE_LOCATION_COL_CNT, tokc);
      return -1;
    }

  if((record = corsaro_geo_init_record(provider, tmp.id)) == NULL)
    {
      return -1;
    }

  memcpy(record, &tmp, sizeof(corsaro_geo_record_t));

  return 0;
}

static int read_netacq_edge_locations(corsaro_t *corsaro, 
				      corsaro_geo_provider_t *provider,
				      corsaro_file_in_t *file)
{
  char buffer[BUFFER_LEN];
  int linec = 0;
  
  while(corsaro_file_rgets(file, &buffer, BUFFER_LEN) > 0)
    {
      /* skip the first line (header) */
      if(linec++ < NETACQ_EDGE_HEADER_ROW_CNT)
	{
	  continue;
	}
      
      chomp(buffer);

      if(parse_netacq_edge_location_row(provider, buffer) != 0)
	{
	  corsaro_log(__func__, corsaro, "invalid netacq location file");
	  return -1;
	}
    }
  
  return 0;
}

/** @todo make use scanf */
static int read_blocks(corsaro_t *corsaro, corsaro_file_in_t *file)
{
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  char buffer[BUFFER_LEN];
  int linec = 0;

  int skip = 0;
  int offset = 0;

  switch(STATE(corsaro)->provider_id)
    {
    case CORSARO_GEO_PROVIDER_MAXMIND:
      skip = MAXMIND_HEADER_ROW_CNT;
      offset = 1; /* skip over the " at the beginning of the string */
      break;

    case CORSARO_GEO_PROVIDER_NETACQ_EDGE:
      skip = NETACQ_EDGE_HEADER_ROW_CNT;
      break;
      
    default:
      return -1;
    }

  char *rowp;
  char *tok = NULL;
  int tokc = 0;

  uint32_t id = 0;

  ip_prefix_list_t *pfx_list = NULL;
  ip_prefix_list_t *temp = NULL;
  ip_prefix_t lower;
  ip_prefix_t upper;

  lower.masklen = 32;
  upper.masklen = 32;

  corsaro_geo_record_t *record = NULL;
  
  while(corsaro_file_rgets(file, &buffer, BUFFER_LEN) > 0)
    {
      /* skip the first two lines */
      if(linec++ < skip)
	{
	  continue;
	}

      rowp = buffer;
      tokc = 0;

      lower.addr = 0;
      upper.addr = 0;

      while((tok = strsep(&rowp, ",")) != NULL)
	{
	  switch(tokc)
	    {
	    case 0:
	      /* start ip */
	      lower.addr = atoi(tok+offset);
	      break;
	      
	    case 1:
	      /* end ip */
	      upper.addr = atoi(tok+offset);
	      break;
	      
	    case 2:
	      /* id */
	      id = atoi(tok+offset);
	      break;
	      
	    default:
	      corsaro_log(__func__, corsaro, "invalid blocks file");
	      return -1;
	      break;
	    }
	  tokc++;
	}

      /* make sure we parsed exactly as many columns as we anticipated */
      if(tokc != BLOCKS_COL_CNT)
	{
	  corsaro_log(__func__, corsaro, "invalid blocks file");
	  return -1;
	}

      assert(id > 0);
     
      /* convert the range to prefixes */
      if(ip_range_to_prefix(lower, upper, &pfx_list) != 0)
	{
	  corsaro_log(__func__, corsaro, "could not convert range to pfxs");
	  return -1;
	}
      assert(pfx_list != NULL);

      /* get the record from the provider */
      if((record = corsaro_geo_get_record(state->provider, id)) == NULL)
	{
	  corsaro_log(__func__, corsaro, "missing record for location %d",
		      id);
	  return -1;
	}

      /* iterate over and add each prefix to the trie */
      while(pfx_list != NULL)
	{
	  if(corsaro_geo_provider_associate_record(corsaro,
						   state->provider,
						   htonl(pfx_list->prefix.addr),
						   pfx_list->prefix.masklen,
						   record) != 0)
	    {
	      corsaro_log(__func__, corsaro, "failed to associate record");
	      return -1;
	    }

	  /* store this node so we can free it */
	  temp = pfx_list;
	  /* move on to the next pfx */
	  pfx_list = pfx_list->next;
	  /* free this node (saves us walking the list twice) */
	  free(temp);
	};
    }
  
  return 0;
}

static int process_generic(corsaro_t *corsaro, corsaro_packet_state_t *state,
			   uint32_t src_ip)
{
  struct corsaro_geodb_state_t *plugin_state = STATE(corsaro);
  
  /* remove the old record from the provider */
  corsaro_geo_provider_clear(plugin_state->provider);

  /* add this record to the provider */
  corsaro_geo_provider_add_record(plugin_state->provider, 
				  corsaro_geo_provider_lookup_record(
								     corsaro, 
								     plugin_state->provider,
								     src_ip));

#if 0
  /** todo move this into a nice generic 'geodump' plugin */
  /* DEBUG */
  corsaro_geo_provider_t *provider;
  corsaro_geo_record_t *tmp = NULL;
  struct in_addr addr;
  addr.s_addr = src_ip;

  fprintf(stdout, "src: %s\n",
	  inet_ntoa(addr));

  /* first, ask for the default geo provider */
  provider = corsaro_geo_get_default(corsaro);
  assert(provider != NULL);

  while((tmp = corsaro_geo_next_record(provider, tmp)) != NULL)
    {
      corsaro_geo_dump_record(tmp);
    }
#endif

  return 0;
}

/* == PUBLIC PLUGIN FUNCS BELOW HERE == */

corsaro_plugin_t *corsaro_geodb_alloc(corsaro_t *corsaro)
{
  return &corsaro_geodb_plugin;
}

int corsaro_geodb_probe_filename(const char *fname)
{
  /* this writes no files! */
  return 0;
}

int corsaro_geodb_probe_magic(corsaro_in_t *corsaro, 
			      corsaro_file_in_t *file)
{
  /* this writes no files! */
  return 0;
}

int corsaro_geodb_init_output(corsaro_t *corsaro)
{
  struct corsaro_geodb_state_t *state;
  corsaro_plugin_t *plugin = PLUGIN(corsaro);
  corsaro_file_in_t *file = NULL;

  int country_cnt;
  int continent_cnt;
  const char **countries;
  const char **continents;
  uint16_t cntry_code = 0;
  uint16_t cont_code = 0;
  int i;
  khiter_t khiter;
  int khret;

  assert(plugin != NULL);

  if((state = malloc_zero(sizeof(struct corsaro_geodb_state_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		  "could not malloc corsaro_maxmind_state_t");
      return -1;
    }
  corsaro_plugin_register_state(corsaro->plugin_manager, plugin, state);

  /* parse the arguments */
  if(parse_args(corsaro) != 0)
    {
      return -1;
    }

  assert(state->locations_file != NULL && state->blocks_file != NULL
	 && state->provider_id > 0);

  /* register us as a geolocation provider */
  if((state->provider = 
      corsaro_geo_init_provider(corsaro,
				state->provider_id,
				CORSARO_GEO_DATASTRUCTURE_DEFAULT,
				CORSARO_GEO_PROVIDER_DEFAULT_YES)) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not register as a geolocation"
		  " provider");
      return -1;
    }

  /* populate the country2continent hash */
  state->country_continent = kh_init(u16u16);
  country_cnt = corsaro_geo_get_maxmind_iso2_list(&countries);
  continent_cnt = corsaro_geo_get_maxmind_country_continent_list(&continents);
  assert(country_cnt == continent_cnt);
  for(i=0; i< country_cnt; i++)
    {
      cntry_code = (countries[i][0]<<8) | countries[i][1];
      cont_code = (continents[i][0]<<8) | continents[i][1];

      /* create a mapping for this country */
      khiter = kh_put(u16u16, state->country_continent, cntry_code, &khret);
      kh_value(state->country_continent, khiter) = cont_code;
    }
  

  /* open the locations file */
  if((file = corsaro_file_ropen(state->locations_file)) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		  "failed to open location file '%s'", state->locations_file);
      return -1;
    }
  
  /* populate the locations hash */
  switch(state->provider->id)
    {
    case CORSARO_GEO_PROVIDER_MAXMIND:
      if(read_maxmind_locations(corsaro, state->provider, file) != 0)
	{
	  corsaro_log(__func__, corsaro, "failed to parse locations file");
	  goto err;
	}
      break;

    case CORSARO_GEO_PROVIDER_NETACQ_EDGE:
      if(read_netacq_edge_locations(corsaro, state->provider, file) != 0)
	{
	  corsaro_log(__func__, corsaro, "failed to parse locations file");
	  goto err;
	}      
      break;

    default:
      corsaro_log(__func__, corsaro, "invalid provider ID for this plugin");
      goto err;
      break;
    }

  /* close the locations file */
  corsaro_file_rclose(file);

  /* open the blocks file */
  if((file = corsaro_file_ropen(state->blocks_file)) == NULL)
    {
      corsaro_log(__func__, corsaro, 
		  "failed to open blocks file '%s'", state->blocks_file);
      goto err;
    }

  /* populate the trie (by joining on the id in the hash) */
  if(read_blocks(corsaro, file) != 0)
    {
      corsaro_log(__func__, corsaro, "failed to parse blocks file");
      goto err;
    }

  /* close the blocks file */
  corsaro_file_rclose(file);

  return 0;

 err:
  if(file != NULL)
    {
      corsaro_file_rclose(file);
    }
  usage(plugin);
  return -1;
}

int corsaro_geodb_init_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

int corsaro_geodb_close_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

int corsaro_geodb_close_output(corsaro_t *corsaro)
{  
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  if(state != NULL)
    {
      if(state->provider != NULL)
	{
	  corsaro_geo_free_provider(corsaro, state->provider);
	  state->provider = NULL;
	}

      if(state->locations_file != NULL)
	{
	  free(state->locations_file);
	  state->locations_file = NULL;
	}

      if(state->blocks_file != NULL)
	{
	  free(state->blocks_file);
	  state->blocks_file = NULL;
	}

      corsaro_plugin_free_state(corsaro->plugin_manager, PLUGIN(corsaro));
    }
  return 0;
}

off_t corsaro_geodb_read_record(struct corsaro_in *corsaro, 
				corsaro_in_record_type_t *record_type, 
				corsaro_in_record_t *record)
{
  assert(0);
  return -1;
}

off_t corsaro_geodb_read_global_data_record(struct corsaro_in *corsaro, 
					    enum corsaro_in_record_type *record_type, 
					    struct corsaro_in_record *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

int corsaro_geodb_start_interval(corsaro_t *corsaro, 
				 corsaro_interval_t *int_start)
{
  /* we don't care */
  return 0;
}

int corsaro_geodb_end_interval(corsaro_t *corsaro, 
			       corsaro_interval_t *int_end)
{
  /* we don't care */
  return 0;
}

int corsaro_geodb_process_packet(corsaro_t *corsaro, 
				 corsaro_packet_t *packet)
{
  libtrace_packet_t *ltpacket = LT_PKT(packet);
  libtrace_ip_t  *ip_hdr  = NULL;
  
  /* check for ipv4 */
  if((ip_hdr = trace_get_ip(ltpacket)) == NULL)
    {
      /* not an ip packet */
      return 0;
    }
  
  return process_generic(corsaro, &packet->state, ip_hdr->ip_src.s_addr);
}

#ifdef WITH_PLUGIN_SIXT
int corsaro_geodb_process_flowtuple(corsaro_t *corsaro,
				    corsaro_flowtuple_t *flowtuple,
				    corsaro_packet_state_t *state)
{
  return process_generic(corsaro, state, 
			 corsaro_flowtuple_get_source_ip(flowtuple));
}

int corsaro_geodb_process_flowtuple_class_start(corsaro_t *corsaro,
						corsaro_flowtuple_class_start_t *class)
{
  /* we dont care about these */
  return 0;
}

int corsaro_geodb_process_flowtuple_class_end(corsaro_t *corsaro,
					      corsaro_flowtuple_class_end_t *class)
{
  /* dont care */
  return 0;
}
#endif
