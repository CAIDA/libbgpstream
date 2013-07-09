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

#include "csv.h"
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

/** The name of the maxmind provider */
#define MAXMIND_NAME \
  (corsaro_geo_get_provider_name(CORSARO_GEO_PROVIDER_MAXMIND))

/** The name of the netacq edge provider */
#define NETACQ_EDGE_NAME \
  (corsaro_geo_get_provider_name(CORSARO_GEO_PROVIDER_NETACQ_EDGE))

/** The default provider name */
#define DEFAULT_PROVIDER_NAME MAXMIND_NAME

/** The length of the static line buffer */
#define BUFFER_LEN 1024

/** The columns in the maxmind locations CSV file */
typedef enum maxmind_locations_cols {
  /** ID */
  MAXMIND_LOCATION_COL_ID     = 0,
  /** 2 Char Country Code */
  MAXMIND_LOCATION_COL_CC     = 1,
  /** Region String */
  MAXMIND_LOCATION_COL_REGION = 2,
  /** City String */
  MAXMIND_LOCATION_COL_CITY   = 3,
  /** Postal Code String */
  MAXMIND_LOCATION_COL_POSTAL = 4,
  /** Latitude */
  MAXMIND_LOCATION_COL_LAT    = 5,
  /** Longitude */
  MAXMIND_LOCATION_COL_LONG   = 6,
  /** Metro Code */
  MAXMIND_LOCATION_COL_METRO  = 7,
  /** Area Code (phone) */
  MAXMIND_LOCATION_COL_AREA   = 8,

  /** Total number of columns in locations table */
  MAXMIND_LOCATION_COL_COUNT  = 9
} maxmind_locations_cols_t;

/** The number of header rows in the maxmind CSV files */
#define MAXMIND_HEADER_ROW_CNT 2

/** The columns in the netacq_edge locations CSV file */
typedef enum netacq_edge_locations_cols {
  /** ID */
  NETACQ_EDGE_LOCATION_COL_ID        = 0,
  /** 3 Char Country Code */
  NETACQ_EDGE_LOCATION_COL_CC3       = 1,   /* not used */
  /** Region String */
  NETACQ_EDGE_LOCATION_COL_REGION    = 2,   /* not used */
  /** City String */
  NETACQ_EDGE_LOCATION_COL_CITY      = 3,
  /** Connection Speed String */
  NETACQ_EDGE_LOCATION_COL_CONN      = 4,
  /** Metro Code */
  NETACQ_EDGE_LOCATION_COL_METRO     = 5,
  /** Latitude */
  NETACQ_EDGE_LOCATION_COL_LAT       = 6,
  /** Longitude */
  NETACQ_EDGE_LOCATION_COL_LONG      = 7,
  /** Postal Code */
  NETACQ_EDGE_LOCATION_COL_POSTAL    = 8,
  /** Country Code */
  NETACQ_EDGE_LOCATION_COL_CNTRYCODE = 9,   /* not used */
  /** Region Code */
  NETACQ_EDGE_LOCATION_COL_RCODE     = 10,  /* not used */
  /** City Code */
  NETACQ_EDGE_LOCATION_COL_CITYCODE  = 11,  /* not used */
  /** Continent Code */
  NETACQ_EDGE_LOCATION_COL_CONTCODE  = 12,
  /** 2 Char Country Code */
  NETACQ_EDGE_LOCATION_COL_CC        = 13,
  /** Internal Code */
  NETACQ_EDGE_LOCATION_COL_INTERNAL  = 14,  /* not used */
  /** Area Codes (plural) */
  NETACQ_EDGE_LOCATION_COL_AREACODES = 15,  /* not used */
  /** Country-Conf ?? */
  NETACQ_EDGE_LOCATION_COL_CNTRYCONF = 16,  /* not used */
  /** Region-Conf ?? */
  NETACQ_EDGE_LOCATION_COL_REGCONF   = 17,  /* not used */
  /** City-Conf ?? */
  NETACQ_EDGE_LOCATION_COL_CITYCONF  = 18,  /* not used */
  /** Postal-Conf ?? */
  NETACQ_EDGE_LOCATION_COL_POSTCONF  = 19,  /* not used */
  /** GMT-Offset */
  NETACQ_EDGE_LOCATION_COL_GMTOFF    = 20,  /* not used */
  /** In CST */
  NETACQ_EDGE_LOCATION_COL_INDST     = 21,  /* not used */
  /** Trailing comma */
  NETACQ_EDGE_LOCATION_COL_TRAIL     = 22,  /* not used */

  /** Total number of columns in the locations table */
  NETACQ_EDGE_LOCATION_COL_COUNT     = 23
} netacq_edge_locations_cols_t;

/** The columns in the maxmind locations CSV file */
typedef enum blocks_cols {
  /** Range Start IP */
  BLOCKS_COL_STARTIP     = 0,
  /** Range End IP */
  BLOCKS_COL_ENDIP       = 1,
  /** ID */
  BLOCKS_COL_ID          = 2,

  /** Total number of columns in blocks table */
  BLOCKS_COL_COUNT  = 3
} blocks_cols_t;

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

  /* State for CSV parser */
  struct csv_parser parser;
  int current_line;
  int current_column;
  corsaro_geo_record_t tmp_record;
  uint16_t cntry_code;
  uint32_t block_id;
  ip_prefix_t block_lower;
  ip_prefix_t block_upper;

  /* hash that maps from country code to continent code */
  khash_t(u16u16) *country_continent;
};

/** Extends the generic plugin state convenience macro in corsaro_plugin.h */
#define STATE(corsaro)							\
  (CORSARO_PLUGIN_STATE(corsaro, geodb, CORSARO_PLUGIN_ID_GEODB))

/** Extends the generic plugin plugin convenience macro in corsaro_plugin.h */
#define PLUGIN(corsaro)						\
  (CORSARO_PLUGIN_PLUGIN(corsaro, CORSARO_PLUGIN_ID_GEODB))

/** Print usage information to stderr */
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

/** Parse the arguments given to the plugin
 * @todo add option to choose datastructure
 */
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

/* Parse a maxmind location cell */
static void parse_maxmind_location_cell(void *s, size_t i, void *data)
{
  corsaro_t *corsaro = (corsaro_t*)data;
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  corsaro_geo_record_t *tmp = &(state->tmp_record);
  char *tok = (char*)s;

  char *end;

  /* skip the first two lines */
  if(state->current_line < MAXMIND_HEADER_ROW_CNT)
    {
      return;
    }

  /*
  corsaro_log(__func__, corsaro, "row: %d, column: %d, tok: %s",
	      state->current_line,
	      state->current_column,
	      tok);
  */

  switch(state->current_column)
    {
    case MAXMIND_LOCATION_COL_ID:
      /* init this record */
      tmp->id = strtol(tok, &end, 10);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid ID Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	  return;
	}
      break;

    case MAXMIND_LOCATION_COL_CC:
      /* country code */
      if(tok == NULL || strlen(tok) != 2)
	{
	  corsaro_log(__func__, corsaro, "Invalid Country Code (%s)", tok);
	  state->parser.status = CSV_EUSER;
	  return;
	}
      state->cntry_code = (tok[0]<<8) | tok[1];
      memcpy(tmp->country_code, tok, 2);
      break;

    case MAXMIND_LOCATION_COL_REGION:
      /* region string */
      if(tok == NULL || strlen(tok) == 0)
	{
	  tmp->region[0] = '\0';
	}
      else
	{
	  tmp->region[2] = '\0';
	  memcpy(tmp->region, tok, 2);
	}
      break;

    case MAXMIND_LOCATION_COL_CITY:
      /* city */
      tmp->city = strndup(tok, strlen(tok));
      break;

    case MAXMIND_LOCATION_COL_POSTAL:
      /* postal code */
      tmp->post_code = strndup(tok, strlen(tok));
      break;

    case MAXMIND_LOCATION_COL_LAT:
      /* latitude */
      tmp->latitude = strtof(tok, &end);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid Latitude Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	  return;
	}
      break;

    case MAXMIND_LOCATION_COL_LONG:
      /* longitude */
      tmp->longitude = strtof(tok, &end);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid Longitude Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	  return;
	}
      break;

    case MAXMIND_LOCATION_COL_METRO:
      /* metro code - whatever the heck that is */
      if(tok != NULL)
	{
	  tmp->metro_code = strtol(tok, &end, 10);
	  if (end == tok || *end != '\0' || errno == ERANGE)
	    {
	      corsaro_log(__func__, corsaro, "Invalid Metro Value (%s)", tok);
	      state->parser.status = CSV_EUSER;
	      return;
	    }
	}
      break;

    case MAXMIND_LOCATION_COL_AREA:
      /* area code - (phone) */
      if(tok != NULL)
	{
	  tmp->area_code = strtol(tok, &end, 10);
	  if (end == tok || *end != '\0' || errno == ERANGE)
	    {
	      corsaro_log(__func__, corsaro,
			  "Invalid Area Code Value (%s)", tok);
	      state->parser.status = CSV_EUSER;
	      return;
	    }
	}
      break;

    default:
      corsaro_log(__func__, corsaro, "Invalid Maxmind Location Column (%d:%d)",
	     state->current_line, state->current_column);
      state->parser.status = CSV_EUSER;
      return;
      break;
    }

  /* move on to the next column */
  state->current_column++;
}

/** Handle an end-of-row event from the CSV parser */
static void parse_maxmind_location_row(int c, void *data)
{
  corsaro_t *corsaro = (corsaro_t*)data;
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  corsaro_geo_record_t *record;

  khiter_t khiter;

  /* skip the first two lines */
  if(state->current_line < MAXMIND_HEADER_ROW_CNT)
    {
      state->current_line++;
      return;
    }

  /* at the end of successful row parsing, current_column will be 9 */
  /* make sure we parsed exactly as many columns as we anticipated */
  if(state->current_column != MAXMIND_LOCATION_COL_COUNT)
    {
      corsaro_log(__func__, corsaro,
		  "ERROR: Expecting %d columns in the locations file, "
		  "but actually got %d",
		  MAXMIND_LOCATION_COL_COUNT, state->current_column);
      state->parser.status = CSV_EUSER;
      return;
    }

  /* look up the continent code */
  if((khiter = kh_get(u16u16, state->country_continent, state->cntry_code)) ==
     kh_end(state->country_continent))
    {
      corsaro_log(__func__, corsaro, "ERROR: Invalid country code (%s) (%x)",
		  state->tmp_record.country_code,
		  state->cntry_code);
      state->parser.status = CSV_EUSER;
      return;
    }

  state->tmp_record.continent_code =
    kh_value(state->country_continent, khiter);

  /*
  corsaro_log(__func__, NULL, "looking up %s (%x) got %x",
	      tmp.country_code, cntry_code, tmp.continent_code);
  */

  if((record = corsaro_geo_init_record(state->provider,
				       state->tmp_record.id)) == NULL)
    {
      corsaro_log(__func__, corsaro, "ERROR: Could not initialize geo record");
      state->parser.status = CSV_EUSER;
      return;
    }

  memcpy(record, &(state->tmp_record), sizeof(corsaro_geo_record_t));

  /* done processing the line */

  /* increment the current line */
  state->current_line++;
  /* reset the current column */
  state->current_column = 0;
  /* reset the temp record */
  memset(&(state->tmp_record), 0, sizeof(corsaro_geo_record_t));
  /* reset the country code */
  state->cntry_code = 0;

  return;
}

/** Parse a netacq location cell */
static void parse_netacq_edge_location_cell(void *s, size_t i, void *data)
{
  corsaro_t *corsaro = (corsaro_t*)data;
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  corsaro_geo_record_t *tmp = &(state->tmp_record);
  char *tok = (char*)s;

  char *end;

  /* skip the first two lines */
  if(state->current_line < NETACQ_EDGE_HEADER_ROW_CNT)
    {
      return;
    }

  /*
  corsaro_log(__func__, corsaro, "row: %d, column: %d, tok: %s",
	      state->current_line,
	      state->current_column,
	      tok);
  */

  switch(state->current_column)
    {
    case NETACQ_EDGE_LOCATION_COL_ID:
      /* init this record */
      tmp->id = strtol(tok, &end, 10);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid ID Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	  return;
	}
      break;

    case NETACQ_EDGE_LOCATION_COL_CC3:
    case NETACQ_EDGE_LOCATION_COL_REGION:
      break;

    case NETACQ_EDGE_LOCATION_COL_CITY:
      if(tok != NULL)
	{
	  tmp->city = strndup(tok, strlen(tok));
	}
      break;

    case NETACQ_EDGE_LOCATION_COL_CONN:
      if(tok != NULL)
	{
	  tmp->conn_speed = strndup(tok, strlen(tok));
	}
      break;

    case NETACQ_EDGE_LOCATION_COL_METRO:
      /* metro code - whatever the heck that is */
      if(tok != NULL)
	{
	  tmp->metro_code = strtol(tok, &end, 10);
	  if (end == tok || *end != '\0' || errno == ERANGE)
	    {
	      corsaro_log(__func__, corsaro, "Invalid Metro Value (%s)", tok);
	      state->parser.status = CSV_EUSER;
	      return;
	    }
	}
      break;

    case NETACQ_EDGE_LOCATION_COL_LAT:
      tmp->latitude = strtof(tok, &end);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid Latitude Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	  return;
	}
      break;

    case NETACQ_EDGE_LOCATION_COL_LONG:
      /* longitude */
      tmp->longitude = strtof(tok, &end);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid Longitude Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	  return;
	}
      break;

    case NETACQ_EDGE_LOCATION_COL_POSTAL:
      tmp->post_code = strndup(tok, strlen(tok));
      break;

    case NETACQ_EDGE_LOCATION_COL_CNTRYCODE:
    case NETACQ_EDGE_LOCATION_COL_RCODE:
    case NETACQ_EDGE_LOCATION_COL_CITYCODE:
      break;

    case NETACQ_EDGE_LOCATION_COL_CONTCODE:
      if(tok != NULL)
	{
	  tmp->continent_code = strtol(tok, &end, 10);
	  if (end == tok || *end != '\0' || errno == ERANGE)
	    {
	      corsaro_log(__func__, corsaro,
			  "Invalid Continent Code Value (%s)", tok);
	      state->parser.status = CSV_EUSER;
	      return;
	    }
	}
      break;

    case NETACQ_EDGE_LOCATION_COL_CC:
      if(tok == NULL || strlen(tok) != 2)
	{
	  corsaro_log(__func__, corsaro, "Invalid Country Code (%s)", tok);
      corsaro_log(__func__, corsaro,
		  "Invalid Net Acuity Edge Location Column (%d:%d)",
	     state->current_line, state->current_column);
	  state->parser.status = CSV_EUSER;
	  return;
	}
      memcpy(tmp->country_code, tok, 2);
      break;

    case NETACQ_EDGE_LOCATION_COL_INTERNAL:
    case NETACQ_EDGE_LOCATION_COL_AREACODES:
    case NETACQ_EDGE_LOCATION_COL_CNTRYCONF:
    case NETACQ_EDGE_LOCATION_COL_REGCONF:
    case NETACQ_EDGE_LOCATION_COL_CITYCONF:
    case NETACQ_EDGE_LOCATION_COL_POSTCONF:
    case NETACQ_EDGE_LOCATION_COL_GMTOFF:
    case NETACQ_EDGE_LOCATION_COL_INDST:
    case NETACQ_EDGE_LOCATION_COL_TRAIL:
      break;

    default:
      corsaro_log(__func__, corsaro,
		  "Invalid Net Acuity Edge Location Column (%d:%d)",
	     state->current_line, state->current_column);
      state->parser.status = CSV_EUSER;
      return;
      break;
    }

  /* move on to the next column */
  state->current_column++;

  return;
}

/** Handle an end-of-row event from the CSV parser */
static void parse_netacq_edge_location_row(int c, void *data)
{
  corsaro_t *corsaro = (corsaro_t*)data;
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  corsaro_geo_record_t *record;

  /* skip the first two lines */
  if(state->current_line < NETACQ_EDGE_HEADER_ROW_CNT)
    {
      state->current_line++;
      return;
    }

  /* at the end of successful row parsing, current_column will be 9 */
  /* make sure we parsed exactly as many columns as we anticipated */
  if(state->current_column != NETACQ_EDGE_LOCATION_COL_COUNT)
    {
      corsaro_log(__func__, corsaro,
		  "ERROR: Expecting %d columns in the locations file, "
		  "but actually got %d",
		  NETACQ_EDGE_LOCATION_COL_COUNT, state->current_column);
      state->parser.status = CSV_EUSER;
      return;
    }

  if((record = corsaro_geo_init_record(state->provider,
				       state->tmp_record.id)) == NULL)
    {
      corsaro_log(__func__, corsaro, "ERROR: Could not initialize geo record");
      state->parser.status = CSV_EUSER;
      return;
    }

  memcpy(record, &(state->tmp_record), sizeof(corsaro_geo_record_t));

  /* done processing the line */

  /* increment the current line */
  state->current_line++;
  /* reset the current column */
  state->current_column = 0;
  /* reset the temp record */
  memset(&(state->tmp_record), 0, sizeof(corsaro_geo_record_t));
  /* reset the country code */
  state->cntry_code = 0;

  return;
}

/** Read a locations file */
static int read_locations(corsaro_t *corsaro, corsaro_file_in_t *file)
{
  struct corsaro_geodb_state_t *state = STATE(corsaro);

  char buffer[BUFFER_LEN];
  int read = 0;

  void (*cell_callback)(void *s, size_t i, void *data);
  void (*row_callback)(int c, void *data);
  const char *provider_name;
  switch(STATE(corsaro)->provider_id)
    {
    case CORSARO_GEO_PROVIDER_MAXMIND:
      cell_callback = parse_maxmind_location_cell;
      row_callback = parse_maxmind_location_row;
      provider_name = MAXMIND_NAME;
      break;

    case CORSARO_GEO_PROVIDER_NETACQ_EDGE:
      cell_callback = parse_netacq_edge_location_cell;
      row_callback = parse_netacq_edge_location_row;
      provider_name = NETACQ_EDGE_NAME;
      break;

    default:
      corsaro_log(__func__, corsaro, "Invalid provider type");
      return -1;
    }

  /* reset the state variables before we start */
  state->current_column = 0;
  state->current_line = 0;
  memset(&(state->tmp_record), 0, sizeof(corsaro_geo_record_t));
  state->cntry_code = 0;

  /* options for the csv parser */
  int options = CSV_STRICT | CSV_REPALL_NL | CSV_STRICT_FINI |
    CSV_APPEND_NULL | CSV_EMPTY_IS_NULL;

  csv_init(&(state->parser), options);

  while((read = corsaro_file_rread(file, &buffer, BUFFER_LEN)) > 0)
    {
      if(csv_parse(&(state->parser), buffer, read,
		   cell_callback,
		   row_callback,
		   corsaro) != read)
	{
	  corsaro_log(__func__, corsaro,
		      "Error parsing %s Location file", provider_name);
	  corsaro_log(__func__, corsaro,
		      "CSV Error: %s",
		      csv_strerror(csv_error(&(state->parser))));
	  return -1;
	}
    }

  if(csv_fini(&(state->parser),
	      cell_callback,
	      row_callback,
	      corsaro) != 0)
    {
      corsaro_log(__func__, corsaro,
		  "Error parsing %s Location file", provider_name);
      corsaro_log(__func__, corsaro,
		  "CSV Error: %s",
		  csv_strerror(csv_error(&(state->parser))));
      return -1;
    }

  csv_free(&(state->parser));

  return 0;
}

/** Parse a blocks cell */
static void parse_blocks_cell(void *s, size_t i, void *data)
{
  corsaro_t *corsaro = (corsaro_t*)data;
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  char *tok = (char*)s;
  char *end;

  int skip = 0;
  switch(STATE(corsaro)->provider_id)
    {
    case CORSARO_GEO_PROVIDER_MAXMIND:
      skip = MAXMIND_HEADER_ROW_CNT;
      break;

    case CORSARO_GEO_PROVIDER_NETACQ_EDGE:
      skip = NETACQ_EDGE_HEADER_ROW_CNT;
      break;

    default:
      corsaro_log(__func__, corsaro, "Invalid provider type");
      state->parser.status = CSV_EUSER;
      return;
    }
  /* skip the first lines */
  if(state->current_line < skip)
    {
      return;
    }

  switch(state->current_column)
    {
    case BLOCKS_COL_STARTIP:
      /* start ip */
      state->block_lower.addr = strtol(tok, &end, 10);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid Start IP Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	}
      break;

    case BLOCKS_COL_ENDIP:
      /* end ip */
      state->block_upper.addr = strtol(tok, &end, 10);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid End IP Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	}
      break;

    case BLOCKS_COL_ID:
      /* id */
      state->block_id = strtol(tok, &end, 10);
      if (end == tok || *end != '\0' || errno == ERANGE)
	{
	  corsaro_log(__func__, corsaro, "Invalid ID Value (%s)", tok);
	  state->parser.status = CSV_EUSER;
	}
      break;

    default:
      corsaro_log(__func__, corsaro, "Invalid Blocks Column (%d:%d)",
		  state->current_line, state->current_column);
      state->parser.status = CSV_EUSER;
      break;
    }

  /* move on to the next column */
  state->current_column++;
}

static void parse_blocks_row(int c, void *data)
{
  corsaro_t *corsaro = (corsaro_t*)data;
  struct corsaro_geodb_state_t *state = STATE(corsaro);

  ip_prefix_list_t *pfx_list = NULL;
  ip_prefix_list_t *temp = NULL;
  corsaro_geo_record_t *record = NULL;

  /* skip the first lines */
  int skip = 0;
  switch(STATE(corsaro)->provider_id)
    {
    case CORSARO_GEO_PROVIDER_MAXMIND:
      skip = MAXMIND_HEADER_ROW_CNT;
      break;

    case CORSARO_GEO_PROVIDER_NETACQ_EDGE:
      skip = NETACQ_EDGE_HEADER_ROW_CNT;
      break;

    default:
      corsaro_log(__func__, corsaro, "Invalid provider type");
      state->parser.status = CSV_EUSER;
      return;
    }
  if(state->current_line < skip)
    {
      state->current_line++;
      return;
    }

  /* done processing the line */

  /* make sure we parsed exactly as many columns as we anticipated */
  if(state->current_column != BLOCKS_COL_COUNT)
    {
      corsaro_log(__func__, corsaro,
		  "ERROR: Expecting %d columns in the blocks file, "
		  "but actually got %d",
		  BLOCKS_COL_COUNT, state->current_column);
      state->parser.status = CSV_EUSER;
      return;
    }

  assert(state->block_id > 0);

  /* convert the range to prefixes */
  if(ip_range_to_prefix(state->block_lower,
			state->block_upper,
			&pfx_list) != 0)
    {
      corsaro_log(__func__, corsaro,
		  "ERROR: Could not convert range to pfxs");
      state->parser.status = CSV_EUSER;
      return;
    }
  assert(pfx_list != NULL);

  /* get the record from the provider */
  if((record = corsaro_geo_get_record(state->provider,
				      state->block_id)) == NULL)
    {
      corsaro_log(__func__, corsaro,
		  "ERROR: Missing record for location %d",
		  state->block_id);
      state->parser.status = CSV_EUSER;
      return;
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
	  corsaro_log(__func__, corsaro,
		      "ERROR: Failed to associate record");
	  state->parser.status = CSV_EUSER;
	  return;
	}

      /* store this node so we can free it */
      temp = pfx_list;
      /* move on to the next pfx */
      pfx_list = pfx_list->next;
      /* free this node (saves us walking the list twice) */
      free(temp);
    }

  /* increment the current line */
  state->current_line++;
  /* reset the current column */
  state->current_column = 0;
}

/** Read a blocks file (maxmind or netacq)  */
static int read_blocks(corsaro_t *corsaro, corsaro_file_in_t *file)
{
  struct corsaro_geodb_state_t *state = STATE(corsaro);
  char buffer[BUFFER_LEN];
  int read = 0;

  /* reset the state variables before we start */
  state->current_column = 0;
  state->current_line = 0;
  state->block_id = 0;
  state->block_lower.masklen = 32;
  state->block_upper.masklen = 32;

  /* options for the csv parser */
  int options = CSV_STRICT | CSV_REPALL_NL | CSV_STRICT_FINI |
    CSV_APPEND_NULL | CSV_EMPTY_IS_NULL;

  csv_init(&(state->parser), options);


  while((read = corsaro_file_rread(file, &buffer, BUFFER_LEN)) > 0)
    {
      if(csv_parse(&(state->parser), buffer, read,
		   parse_blocks_cell,
		   parse_blocks_row,
		   corsaro) != read)
	{
	  corsaro_log(__func__, corsaro,
		      "Error parsing Blocks file");
	  corsaro_log(__func__, corsaro,
		      "CSV Error: %s",
		      csv_strerror(csv_error(&(state->parser))));
	  return -1;
	}
    }

  if(csv_fini(&(state->parser),
	      parse_blocks_cell,
	      parse_blocks_row,
	      corsaro) != 0)
    {
      corsaro_log(__func__, corsaro,
		  "Error parsing Maxmind Location file");
      corsaro_log(__func__, corsaro,
		  "CSV Error: %s",
		  csv_strerror(csv_error(&(state->parser))));
      return -1;
    }

  csv_free(&(state->parser));

  return 0;
}

/** Common code between process_packet and process_flowtuple */
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

/** Implements the alloc function of the plugin API */
corsaro_plugin_t *corsaro_geodb_alloc(corsaro_t *corsaro)
{
  return &corsaro_geodb_plugin;
}

/** Implements the probe_filename function of the plugin API */
int corsaro_geodb_probe_filename(const char *fname)
{
  /* this writes no files! */
  return 0;
}

/** Implements the probe_magic function of the plugin API */
int corsaro_geodb_probe_magic(corsaro_in_t *corsaro,
			      corsaro_file_in_t *file)
{
  /* this writes no files! */
  return 0;
}

/** Implements the init_output function of the plugin API */
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
  if(read_locations(corsaro, file) != 0)
    {
      corsaro_log(__func__, corsaro, "failed to parse locations file");
      goto err;
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

/** Implements the init_input function of the plugin API */
int corsaro_geodb_init_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_input function of the plugin API */
int corsaro_geodb_close_input(corsaro_in_t *corsaro)
{
  assert(0);
  return -1;
}

/** Implements the close_output function of the plugin API */
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

/** Implements the read_record function of the plugin API */
off_t corsaro_geodb_read_record(struct corsaro_in *corsaro,
				corsaro_in_record_type_t *record_type,
				corsaro_in_record_t *record)
{
  assert(0);
  return -1;
}

/** Implements the read_global_data_record function of the plugin API */
off_t corsaro_geodb_read_global_data_record(corsaro_in_t *corsaro,
				     corsaro_in_record_type_t *record_type,
				     corsaro_in_record_t *record)
{
  /* we write nothing to the global file. someone messed up */
  return -1;
}

/** Implements the start_interval function of the plugin API */
int corsaro_geodb_start_interval(corsaro_t *corsaro,
				 corsaro_interval_t *int_start)
{
  /* we don't care */
  return 0;
}

/** Implements the end_interval function of the plugin API */
int corsaro_geodb_end_interval(corsaro_t *corsaro,
			       corsaro_interval_t *int_end)
{
  /* we don't care */
  return 0;
}

/** Implements the process_packet function of the plugin API */
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
/** Implements the process_flowtuple function of the plugin API */
int corsaro_geodb_process_flowtuple(corsaro_t *corsaro,
				    corsaro_flowtuple_t *flowtuple,
				    corsaro_packet_state_t *state)
{
  return process_generic(corsaro, state,
			 corsaro_flowtuple_get_source_ip(flowtuple));
}

/** Implements the process_flowtuple_class_start function of the plugin API */
int corsaro_geodb_process_flowtuple_class_start(corsaro_t *corsaro,
						corsaro_flowtuple_class_start_t *class)
{
  /* we dont care about these */
  return 0;
}

/** Implements the process_flowtuple_class_end function of the plugin API */
int corsaro_geodb_process_flowtuple_class_end(corsaro_t *corsaro,
					      corsaro_flowtuple_class_end_t *class)
{
  /* dont care */
  return 0;
}
#endif
