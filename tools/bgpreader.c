/*
 * bgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpstream.
 *
 * bgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpstream is distributed in the hope that it will be usefuql,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>

#include <bgpstream.h>

#define PROJECT_CMD_CNT 10
#define TYPE_CMD_CNT    10
#define COLLECTOR_CMD_CNT 100
#define WINDOW_CMD_CNT 1024

struct window {
  char *start;
  char *end;
};


void usage() {
  fprintf(stderr,
	  "usage: bgpreader -d <datasource> [<options>]\n"
	  "Available datasources are:\n"
	  "   mysql        load bgp dumps information from the bgparchive mysql database\n"
	  "   csvfile      load bgp dumps information from a csv file\n"
	  "   customlist   mock datasource used to test the library\n"
	  "Available options are:\n"
	  "   -P <project>   process records from only the given project (routeviews, ris)*\n"
	  "   -C <collector> process records from only the given collector*\n"
	  "   -T <type>      process records with only the given type (ribs, updates)*\n"
	  "   -W <start,end> process records only within the given time window*\n"
	  "   -b             make blocking requests for BGP records\n"
	  "                  allows bgpstream to be used to process data in real-time\n"
	  "   -r             print information for each BGP record (in bgpstream format) [default]\n"
	  "   -m             print information for each BGP valid record (in bgpdump -m format)\n"
	  "   -e             print information for each element of a valid BGP record\n"
	  "mysql specific options are:\n"
	  "   -D <database_name>  the database name [default: bgparchive]\n"
	  "   -U <user>           the user name to connect to the database [default: bgpstream]\n"
	  "   -H <host>           the host that host the the database [default: localhost]\n"
	  "csvfile specific options are:\n"
	  "   -F <filename>  the csvfile to read\n"
	  "\n"
	  "   -h             print this help menu\n"
	  "* denotes an option that can be given multiple times\n"
	  );
}

// print functions

static void print_bs_record(bgpstream_record_t * bs_record);
static void print_elem(bgpstream_elem_t *elem);


int main(int argc, char *argv[])
{

  int opt;
  int prevoptind;

  opterr = 0;

  // variables associated with options
  char *projects[PROJECT_CMD_CNT];
  int projects_cnt = 0;

  char *types[TYPE_CMD_CNT];
  int types_cnt = 0;

  char *collectors[COLLECTOR_CMD_CNT];
  int collectors_cnt = 0;

  struct window windows[WINDOW_CMD_CNT];
  char *endp;
  int windows_cnt = 0;

  char * datasource_name = NULL;
  int blocking = 0;
  int record_output_on = 0;
  int record_bgpdump_output_on = 0;
  int elem_output_on = 0;
  char *mysql_dbname = NULL;
  char *mysql_user = NULL;
  char *mysql_host = NULL;
  char *csvfile_file = NULL;

  while (prevoptind = optind,
	 (opt = getopt (argc, argv, "P:C:T:W:d:brmeD:U:H:F:h?")) >= 0)
    {
      if (optind == prevoptind + 2 && (optarg == NULL || *optarg == '-') ) {
        opt = ':';
        -- optind;
      }
      switch (opt)
	{
	case 'P':
	  if(projects_cnt == PROJECT_CMD_CNT)
	    {
	      fprintf(stderr,
		      "ERROR: A maximum of %d projects can be specified on "
		      "the command line\n",
		      PROJECT_CMD_CNT);
	      usage();
	      exit(-1);
	    }
	  projects[projects_cnt++] = strdup(optarg);
	  break;
	case 'C':
	  if(collectors_cnt == COLLECTOR_CMD_CNT)
	    {
	      fprintf(stderr,
		      "ERROR: A maximum of %d collectors can be specified on "
		      "the command line\n",
		      COLLECTOR_CMD_CNT);
	      usage();
	      exit(-1);
	    }
	  collectors[collectors_cnt++] = strdup(optarg);
	  break;
	case 'T':
	  if(types_cnt == TYPE_CMD_CNT)
	    {
	      fprintf(stderr,
		      "ERROR: A maximum of %d types can be specified on "
		      "the command line\n",
		      TYPE_CMD_CNT);
	      usage();
	      exit(-1);
	    }
	  types[types_cnt++] = strdup(optarg);
	  break;
	case 'W':
	  if(windows_cnt == WINDOW_CMD_CNT)
	  {
	    fprintf(stderr,
		    "ERROR: A maximum of %d windows can be specified on "
		    "the command line\n",
		    WINDOW_CMD_CNT);
	    usage();
	    exit(-1);
	  }
	  /* split the window into a start and end */
	  if((endp = strchr(optarg, ',')) == NULL)
	    {
	      fprintf(stderr, "ERROR: Malformed time window (%s)\n", optarg);
	      fprintf(stderr, "ERROR: Expecting start,end\n");
	      usage();
	      exit(-1);
	    }
	  *endp = '\0';
	  endp++;
	  windows[windows_cnt].start = strdup(optarg);
	  windows[windows_cnt].end =  strdup(endp);
	  windows_cnt++;
	  break;
	case 'd':
	  datasource_name = strdup(optarg);
	  break;
	case 'D':
	  mysql_dbname = strdup(optarg);
	  break;
	case 'U':
	  mysql_user = strdup(optarg);
	  break;
	case 'H':
	  mysql_host = strdup(optarg);
	  break;
	case 'F':
	  csvfile_file = strdup(optarg);
	  break;
	case 'b':
	  blocking = 1;
	  break;
	case 'r':
	  record_output_on = 1;
	  break;
	case 'm':
	  record_bgpdump_output_on = 1;
	  break;
	case 'e':
	  elem_output_on = 1;
	  break;
	case ':':
	  fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
	  usage();
	  exit(-1);
	  break;
	case '?':
	case 'v':
	  fprintf(stderr, "bgpreader version %d.%d.%d\n",
		  BGPSTREAM_MAJOR_VERSION,
		  BGPSTREAM_MID_VERSION,
		  BGPSTREAM_MINOR_VERSION);
	  usage();
	  exit(0);
	  break;
	default:
	  usage();
	  exit(-1);
	}
    }

  // datasource is the only required argument
  if(datasource_name == NULL)
    {
    fprintf(stderr, "ERROR: Missing mandatory argument -d <datasource>\n");
    usage();
    exit(-1);
  }

  bgpstream_datasource_type datasource_type;
  if(strcmp(datasource_name, "mysql") == 0)
    {
      datasource_type = BS_MYSQL;
    }
  else
    {
      if(strcmp(datasource_name, "csvfile") == 0)
	{
	  datasource_type = BS_CSVFILE;
	}
      else
	{
	  if(strcmp(datasource_name, "customlist") == 0)
	    {
	      datasource_type = BS_CUSTOMLIST;
	    }
	  else
	    {
	      fprintf(stderr, "ERROR: Datasource %s is not valid.\n", datasource_name);
	      usage();
	      exit(-1);
	    }
	}
    }

  // signal if there are incompatible arguments that will be ignored
  if(
     (datasource_type != BS_MYSQL &&
     (mysql_dbname != NULL || mysql_user != NULL || mysql_host != NULL) ) ||
     (datasource_type != BS_CSVFILE && csvfile_file != NULL)
     )
    {
      fprintf(stderr, "WARNING: some of the datasource options provided do not apply\n"
	      "\t to the datasource choosen and they will be ignored.\n");
    }


  // if the user did not specify any output format
  // then the default one is per record
  if(record_output_on == 0 && elem_output_on == 0 && record_bgpdump_output_on == 0) {
    record_output_on = 1;
  }

  // the program can now start

  // allocate memory for interface
  bgpstream_t * const bs = bgpstream_create();
  if(!bs) {
    fprintf(stderr, "ERROR: Could not create BGPStream instance\n");
    return -1;
  }

  int i = 0; // iterator

  /* projects */
  for(i=0; i<projects_cnt; i++)
    {
      bgpstream_add_filter(bs, BS_PROJECT, projects[i]);
      free(projects[i]);
    }

  /* collectors */
  for(i=0; i<collectors_cnt; i++)
    {
      bgpstream_add_filter(bs, BS_COLLECTOR, collectors[i]);
      free(collectors[i]);
    }

  /* types */
  for(i=0; i<types_cnt; i++)
    {
      bgpstream_add_filter(bs, BS_BGP_TYPE, types[i]);
      free(types[i]);
    }

  /* windows */
  for(i=0; i<windows_cnt; i++)
    {
      bgpstream_add_interval_filter(bs, BS_TIME_INTERVAL,
				    windows[i].start, windows[i].end);
      free(windows[i].start);
      free(windows[i].end);
    }

  /* datasource */
  bgpstream_set_data_interface(bs, datasource_type);


  /* datasource options */
  if(mysql_dbname != NULL)
    {
      bgpstream_set_data_interface_options(bs, BS_MYSQL_DB, mysql_dbname);
    }
  if(mysql_user != NULL)
    {
      bgpstream_set_data_interface_options(bs, BS_MYSQL_USER, mysql_user);
    }
  if(mysql_host != NULL)
    {
      bgpstream_set_data_interface_options(bs, BS_MYSQL_HOST, mysql_host);
    }
  if(csvfile_file != NULL)
    {
      bgpstream_set_data_interface_options(bs, BS_CSVFILE_FILE, csvfile_file);
    }


  /* blocking */
  if(blocking != 0)
    {
      bgpstream_set_blocking(bs);
    }

   // allocate memory for bs_record
  bgpstream_record_t *bs_record = bgpstream_record_create();
  if(bs_record == NULL)
    {
      fprintf(stderr, "ERROR: Could not create BGPStream record\n");
      bgpstream_destroy(bs);
      return -1;
    }

    // turn on interface
  if(bgpstream_start(bs) < 0) {
    fprintf(stderr, "ERROR: Could not init BGPStream\n");
    return -1;
  }

  // use the interface
  int get_next_ret = 0;
  bgpstream_elem_t * bs_elem_head;
  bgpstream_elem_t * bs_elem_iterator;
  do
    {
      get_next_ret = bgpstream_get_next_record(bs, bs_record);
      if(get_next_ret && record_output_on)
	{
	  print_bs_record(bs_record);
	}
      if(get_next_ret && bs_record->status == VALID_RECORD)
	{
	  if(record_bgpdump_output_on)
	    {
	      bgpstream_record_print_mrt_data(bs_record);
	    }
	  if(elem_output_on)
	    {
	      // extract queue
	      bs_elem_head = bgpstream_elem_queue_create(bs_record);
	      bs_elem_iterator = bs_elem_head;
	      while(bs_elem_iterator)
		{
		  print_elem(bs_elem_iterator);
		  bs_elem_iterator = bs_elem_iterator->next;
		}
	      bgpstream_elem_queue_destroy(bs_elem_head);
	    }
	}
  }
  while(get_next_ret > 0);

  // de-allocate memory for bs_record
  bgpstream_record_destroy(bs_record);

  // turn off interface
  bgpstream_stop(bs);

  // deallocate memory for interface
  bgpstream_destroy(bs);

  // deallocate memory for strings
  if(datasource_name != NULL)
    {
      free(datasource_name);
    }
  if(mysql_dbname != NULL)
    {
      free(mysql_dbname);
    }
  if(mysql_user != NULL)
    {
      free(mysql_user);
    }
  if(mysql_host != NULL)
    {
      free(mysql_host);
    }
  if(csvfile_file != NULL)
    {
      free(csvfile_file);
    }

  return 0;
}

// print utility functions

static char* get_dump_type_str(bgpstream_record_dump_type_t dump_type)
{
  switch(dump_type)
    {
    case BGPSTREAM_UPDATE:
      return "update";
    case BGPSTREAM_RIB:
      return "rib";
    }
  return "";
}

static char* get_dump_pos_str(bgpstream_dump_position_t dump_pos)
{
  switch(dump_pos)
    {
    case DUMP_START:
      return "start";
    case DUMP_MIDDLE:
      return "middle";
    case DUMP_END:
      return "end";
    }
  return "";
}

static char *get_record_status_str(bgpstream_record_status_t status)
{
  switch(status)
    {
    case VALID_RECORD:
      return "valid_record";
    case FILTERED_SOURCE:
      return "filtered_source";
    case EMPTY_SOURCE:
      return "empty_source";
    case CORRUPTED_SOURCE:
      return "corrupted_source";
    case CORRUPTED_RECORD:
      return "corrupted_record";
    }
  return "";
}


static void print_bs_record(bgpstream_record_t * bs_record)
{
  assert(bs_record);
  printf("%ld|", bs_record->attributes.record_time);
  printf("%s|", bs_record->attributes.dump_project);
  printf("%s|", bs_record->attributes.dump_collector);
  printf("%s|", get_dump_type_str(bs_record->attributes.dump_type));
  printf("%s|", get_record_status_str(bs_record->status));
  printf("%ld|", bs_record->attributes.dump_time);
  printf("%s|", get_dump_pos_str(bs_record->dump_pos));
  printf("\n");

}


static void print_elem(bgpstream_elem_t * elem)
{
  assert(bs_elem);
  char buf[4096];

  if(bgpstream_elem_snprintf(buf, 4096, elem) == NULL)
    {
      fprintf(stderr, "Elem longer than 4096 bytes\n");
      assert(0);
    }
  printf("%s\n", buf);
}
