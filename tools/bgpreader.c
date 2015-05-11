/*
* This file is part of bgpstream
*
* Copyright (c) 2015 The Regents of the University of California.
* Authors: Alistair King, Chiara Orsini
*
* All rights reserved.
*
* This code has been developed by CAIDA at UC San Diego.
* For more information, contact bgpstream-info@caida.org
*
* This source code is proprietary to the CAIDA group at UC San Diego and may
* not be redistributed, published or disclosed without prior permission from
* CAIDA.
*
* Report any bugs, questions or comments to bgpstream-info@caida.org
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
#define OPTION_CMD_CNT 1024

struct window {
  uint32_t start;
  uint32_t end;
};

bgpstream_t *bs;
bgpstream_data_interface_id_t datasource_id = 0;
const char *datasource_name = NULL;

void data_if_usage() {
  bgpstream_data_interface_id_t *ids = NULL;
  int id_cnt = 0;
  int i;

  bgpstream_data_interface_info_t *info = NULL;

  id_cnt = bgpstream_get_data_interfaces(bs, &ids);

  for(i=0; i<id_cnt; i++)
    {
      info = bgpstream_get_data_interface_info(bs, ids[i]);

      fprintf(stderr,
              "       %-15s%s\n", info->name, info->description);
    }
}

void dump_if_options() {
  assert(datasource_id != 0);

  bgpstream_data_interface_option_t *options;
  int opt_cnt = 0;
  int i;

  opt_cnt = bgpstream_get_data_interface_options(bs, datasource_id, &options);

  fprintf(stderr, "Data interface options for '%s':\n", datasource_name);
  if(opt_cnt == 0)
    {
      fprintf(stderr, "   [NONE]\n");
    }
  else
    {
      for(i=0; i<opt_cnt; i++)
        {
          fprintf(stderr, "   %-15s%s\n",
                  options[i].name, options[i].description);
        }
    }
}

void usage() {
  fprintf(stderr,
	  "usage: bgpreader -d <interface> [<options>]\n"
          "   -d <interface> use the given data interface to find available data\n"
          "     available data interfaces are:\n");
  data_if_usage();
  fprintf(stderr,
          "   -o <option-name,option-value>*\n"
          "                  set an option for the current data interface.\n"
          "                    use '-o ?' to get a list of available\n"
          "                    options for the current data interface.\n"
          "                    (data interface must be selected using -d)\n"
	  "   -P <project>   process records from only the given project (routeviews, ris)*\n"
	  "   -C <collector> process records from only the given collector*\n"
	  "   -T <type>      process records with only the given type (ribs, updates)*\n"
	  "   -W <start,end> process records only within the given time window*\n"
          "   -R <freq>      process rib files whose filetime is at least <freq> seconds apart\n"
          "   -U <freq>      process update files whose filetime is at least <freq> seconds apart\n"
	  "   -b             make blocking requests for BGP records\n"
	  "                  allows bgpstream to be used to process data in real-time\n"
	  "   -r             print info for each BGP record (in bgpstream format) [default]\n"
	  "   -m             print info for each BGP valid record (in bgpdump -m format)\n"
	  "   -e             print info for each element of a valid BGP record\n"
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

  char *interface_options[OPTION_CMD_CNT];
  int interface_options_cnt = 0;

  int rib_frequency = 0;
  int update_frequency = 0;
  int blocking = 0;
  int record_output_on = 0;
  int record_bgpdump_output_on = 0;
  int elem_output_on = 0;

  bgpstream_data_interface_option_t *option;

  int i;


  // required to be created before usage is called
  bs = bgpstream_create();
  if(!bs) {
    fprintf(stderr, "ERROR: Could not create BGPStream instance\n");
    return -1;
  }

  while (prevoptind = optind,
	 (opt = getopt (argc, argv, "P:C:T:W:R:U:d:brmeo:h?")) >= 0)
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
	  windows[windows_cnt].start = atoi(optarg);
	  windows[windows_cnt].end =  atoi(endp);
	  windows_cnt++;
	  break;
        case 'R':
          rib_frequency = atoi(optarg);
          break;
        case 'U':
          update_frequency = atoi(optarg);
          break;
	case 'd':
          if((datasource_id =
              bgpstream_get_data_interface_id_by_name(bs, optarg)) == 0)
            {
              fprintf(stderr, "ERROR: Invalid data interface name '%s'\n",
                      optarg);
              usage();
              exit(-1);
            }
          datasource_name = optarg;
	  break;
        case 'o':
          if(interface_options_cnt == OPTION_CMD_CNT)
	    {
	      fprintf(stderr,
		      "ERROR: A maximum of %d interface options can be specified\n",
		      OPTION_CMD_CNT);
	      usage();
	      exit(-1);
	    }
	  interface_options[interface_options_cnt++] = strdup(optarg);
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

  if(datasource_id == 0)
    {
      fprintf(stderr, "ERROR: Data interface must be specified with -d\n");
      usage();
      exit(-1);
    }

  for(i=0; i<interface_options_cnt; i++)
    {
      if(*interface_options[i] == '?')
        {
          dump_if_options();
          usage();
          exit(0);
        }
      else
        {
          /* actually set this option */
          if((endp = strchr(interface_options[i], ',')) == NULL)
            {
              fprintf(stderr,
                      "ERROR: Malformed data interface option (%s)\n",
                      interface_options[i]);
              fprintf(stderr,
                      "ERROR: Expecting <option-name>,<option-value>\n");
              usage();
              exit(-1);
            }
          *endp = '\0';
          endp++;
          if((option =
              bgpstream_get_data_interface_option_by_name(bs, datasource_id,
                                                          interface_options[i])) == NULL)
            {
              fprintf(stderr,
                      "ERROR: Invalid option '%s' for data interface '%s'\n",
                      interface_options[i], datasource_name);
              usage();
              exit(-1);
            }
          bgpstream_set_data_interface_option(bs, option, endp);
        }
      free(interface_options[i]);
      interface_options[i] = NULL;
    }
  interface_options_cnt = 0;

  // if the user did not specify any output format
  // then the default one is per record
  if(record_output_on == 0 && elem_output_on == 0 && record_bgpdump_output_on == 0) {
    record_output_on = 1;
  }

  // the program can now start

  // allocate memory for interface

  /* projects */
  for(i=0; i<projects_cnt; i++)
    {
      bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_PROJECT, projects[i]);
      free(projects[i]);
    }

  /* collectors */
  for(i=0; i<collectors_cnt; i++)
    {
      bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_COLLECTOR, collectors[i]);
      free(collectors[i]);
    }

  /* types */
  for(i=0; i<types_cnt; i++)
    {
      bgpstream_add_filter(bs, BGPSTREAM_FILTER_TYPE_RECORD_TYPE, types[i]);
      free(types[i]);
    }

  /* windows */
  for(i=0; i<windows_cnt; i++)
    {
      bgpstream_add_interval_filter(bs, windows[i].start, windows[i].end);
    }

  /* frequencies */
  if(rib_frequency > 0)
    {
      bgpstream_add_frequency_filter(bs, BGPSTREAM_RIB, rib_frequency);
    }
  
  if(update_frequency > 0)
    {
      bgpstream_add_frequency_filter(bs, BGPSTREAM_UPDATE, update_frequency);
    }
  
  /* datasource */
  bgpstream_set_data_interface(bs, datasource_id);

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
  bgpstream_elem_t * bs_elem;
  do
    {
      get_next_ret = bgpstream_get_next_record(bs, bs_record);
      if(get_next_ret && record_output_on)
	{
	  print_bs_record(bs_record);
	}
      if(get_next_ret && bs_record->status == BGPSTREAM_RECORD_STATUS_VALID_RECORD)
	{
	  if(record_bgpdump_output_on)
	    {
	      bgpstream_record_print_mrt_data(bs_record);
	    }
	  if(elem_output_on)
	    {
	      while((bs_elem =
                     bgpstream_record_get_next_elem(bs_record)) != NULL)
		{
		  print_elem(bs_elem);
		}
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
    case BGPSTREAM_DUMP_START:
      return "start";
    case BGPSTREAM_DUMP_MIDDLE:
      return "middle";
    case BGPSTREAM_DUMP_END:
      return "end";
    }
  return "";
}

static char *get_record_status_str(bgpstream_record_status_t status)
{
  switch(status)
    {
    case BGPSTREAM_RECORD_STATUS_VALID_RECORD:
      return "valid_record";
    case BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE:
      return "filtered_source";
    case BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE:
      return "empty_source";
    case BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE:
      return "corrupted_source";
    case BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD:
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
