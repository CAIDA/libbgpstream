/*
 * bgpcorsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "config.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libtrace.h"

#include "utils.h"

#include "bgpcorsaro.h"
#include "bgpcorsaro_log.h"

/** @file
 *
 * @brief Code which uses libbgpcorsaro to process a trace file and generate output
 *
 * @author Alistair King
 *
 */

#define PROJECT_CMD_CNT 10
#define TYPE_CMD_CNT    10
#define COLLECTOR_CMD_CNT 100
#define WINDOW_CMD_CNT 1024

struct window {
  char *start;
  char *end;
};

/** Indicates that Bgpcorsaro is waiting to shutdown */
volatile sig_atomic_t bgpcorsaro_shutdown = 0;

/** The number of SIGINTs to catch before aborting */
#define HARD_SHUTDOWN 3

/* for when we are reading trace files */
/** A pointer to a libtrace object */
static bgpstream_t *stream = NULL;
/** A pointer to a bgpstream record */
static bgpstream_record_t *record = NULL;

/** A pointer to the instance of bgpcorsaro that we will drive */
static bgpcorsaro_t *bgpcorsaro = NULL;

/** Handles SIGINT gracefully and shuts down */
static void catch_sigint(int sig)
{
  bgpcorsaro_shutdown++;
  if(bgpcorsaro_shutdown == HARD_SHUTDOWN)
    {
      fprintf(stderr, "caught %d SIGINT's. shutting down NOW\n",
	      HARD_SHUTDOWN);
      exit(-1);
    }

  fprintf(stderr, "caught SIGINT, shutting down at the next opportunity\n");

  signal(sig, catch_sigint);
}

/** Clean up all state before exit */
static void clean()
{
  if(record != NULL)
    {
      bgpstream_destroy_record(record);
      record = NULL;
    }

  if(bgpcorsaro != NULL)
    {
      bgpcorsaro_finalize_output(bgpcorsaro);
    }
}

/** Print usage information to stderr */
static void usage()
{
  int i;
  char **plugin_names;
  int plugin_cnt;
  if((plugin_cnt = bgpcorsaro_get_plugin_names(&plugin_names)) < 0)
    {
      /* not much we can do */
      fprintf(stderr, "bgpcorsaro_get_plugin_names failed\n");
      return;
    }

  fprintf(stderr,
	  "usage: bgpcorsaro -o outfile [<options>]\n"
	  "\n"
	  "Available options are:\n"
	  "   -a             align the end time of the first interval\n"
	  "   -b             make blocking requests for BGP records\n"
	  "                   allows bgpcorsaro to be used to process data in real-time\n"
	  "   -C <collector> process records from only the given collector*\n"
	  "   -i <interval>  distribution interval in seconds (default: %d)\n"
	  "   -L             disable logging to a file\n"
	  "   -n <name>      monitor name (default: "
	  STR(BGPCORSARO_MONITOR_NAME)")\n"
	  "   -o <outfile>   use <outfile> as a template for file names.\n"
	  "                   - %%P => plugin name\n"
	  "                   - %%N => monitor name\n"
	  "                   - see man strftime(3) for more options\n"
	  "   -p <plugin>    enable the given plugin (default: all)*\n"
	  "                   available plugins:\n",
	  BGPCORSARO_INTERVAL_DEFAULT);

  for(i = 0; i < plugin_cnt; i++)
    {
      fprintf(stderr, "                    - %s\n", plugin_names[i]);
    }
  fprintf(stderr,
	  "                   use -p \"<plugin_name> -?\" to see plugin options\n"
	  "   -P <project>   process records from only the given project (routeviews, ris)*\n"
	  "   -r <intervals> rotate output files after n intervals\n"
	  "   -R <intervals> rotate bgpcorsaro meta files after n intervals\n"
	  "   -T <type>      process records with only the given type (ribs, updates)*\n"
	  "   -W <start,end> process records only within the given time window*\n"
	  "\n"
	  "* denotes an option that can be given multiple times\n"
	  );

  bgpcorsaro_free_plugin_names(plugin_names, plugin_cnt);
}

/** Entry point for the Bgpcorsaro tool */
int main(int argc, char *argv[])
{
  /* we MUST not use any of the getopt global vars outside of arg parsing */
  /* this is because the plugins can use get opt to parse their config */
  int opt;
  int prevoptind;
  char *tmpl = NULL;
  char *name = NULL;
  int i = -1000;
  char *plugins[BGPCORSARO_PLUGIN_ID_MAX];
  int plugin_cnt = 0;
  char *plugin_arg_ptr = NULL;
  int align = 0;
  int rotate = 0;
  int meta_rotate = -1;
  int logfile_disable = 0;

  char *projects[PROJECT_CMD_CNT];
  int projects_cnt = 0;

  char *types[TYPE_CMD_CNT];
  int types_cnt = 0;

  char *collectors[COLLECTOR_CMD_CNT];
  int collectors_cnt = 0;

  char *endp;
  struct window windows[WINDOW_CMD_CNT];
  int windows_cnt = 0;

  int blocking = 0;

  int rc = 0;

  signal(SIGINT, catch_sigint);

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":C:i:n:o:p:P:r:R:T:W:abLv?")) >= 0)
    {
      if (optind == prevoptind + 2 && (optarg == NULL || *optarg == '-') ) {
        opt = ':';
        -- optind;
      }
      switch(opt)
	{
	case 'a':
	  align = 1;
	  break;

	case 'b':
	  blocking = 1;
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

	case 'i':
	  i = atoi(optarg);
	  break;

	case 'L':
	  logfile_disable = 1;
	  break;

	case 'n':
	  name = strdup(optarg);
	  break;

	case 'o':
	  tmpl = strdup(optarg);
	  break;

	case 'p':
	  plugins[plugin_cnt++] = strdup(optarg);
	  break;

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

	case 'r':
	  rotate = atoi(optarg);
	  break;

	case 'R':
	  meta_rotate = atoi(optarg);
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

	case ':':
	  fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
	  usage();
	  exit(-1);
	  break;

	case '?':
	case 'v':
	  fprintf(stderr, "bgpcorsaro version %d.%d.%d\n",
		  BGPCORSARO_MAJOR_VERSION,
		  BGPCORSARO_MID_VERSION,
		  BGPCORSARO_MINOR_VERSION);
	  usage();
	  exit(0);
	  break;

	default:
	  usage();
	  exit(-1);
	}
    }

  /* store the value of the last index*/
  /*lastopt = optind;*/

  /* reset getopt for others */
  optind = 1;

  /* -- call NO library functions which may use getopt before here -- */
  /* this ESPECIALLY means bgpcorsaro_enable_plugin */

  if(tmpl == NULL)
    {
      fprintf(stderr,
	      "ERROR: An output file template must be specified using -o\n");
      usage();
      goto err;
    }

  /* alloc bgpcorsaro */
  if((bgpcorsaro = bgpcorsaro_alloc_output(tmpl)) == NULL)
    {
      usage();
      goto err;
    }

  if(name != NULL && bgpcorsaro_set_monitorname(bgpcorsaro, name) != 0)
    {
      bgpcorsaro_log(__func__, bgpcorsaro, "failed to set monitor name");
      goto err;
    }

  if(i > -1000)
    {
      bgpcorsaro_set_interval(bgpcorsaro, i);
    }

  if(align == 1)
    {
      bgpcorsaro_set_interval_alignment(bgpcorsaro, BGPCORSARO_INTERVAL_ALIGN_YES);
    }

  if(rotate > 0)
    {
      bgpcorsaro_set_output_rotation(bgpcorsaro, rotate);
    }

  if(meta_rotate >= 0)
    {
      bgpcorsaro_set_meta_output_rotation(bgpcorsaro, meta_rotate);
    }

  for(i=0;i<plugin_cnt;i++)
    {
      /* the string at plugins[i] will contain the name of the plugin,
	 optionally followed by a space and then the arguments to pass
	 to the plugin */
      if((plugin_arg_ptr = strchr(plugins[i], ' ')) != NULL)
	{
	  /* set the space to a nul, which allows plugins[i] to be used
	     for the plugin name, and then increment plugin_arg_ptr to
	     point to the next character, which will be the start of the
	     arg string (or at worst case, the terminating \0 */
	  *plugin_arg_ptr = '\0';
	  plugin_arg_ptr++;
	}

      if(bgpcorsaro_enable_plugin(bgpcorsaro, plugins[i], plugin_arg_ptr) != 0)
	{
	  fprintf(stderr, "ERROR: Could not enable plugin %s\n",
		  plugins[i]);
	  usage();
	  goto err;
	}
    }

  if(logfile_disable != 0)
    {
      bgpcorsaro_disable_logfile(bgpcorsaro);
    }

  if(bgpcorsaro_start_output(bgpcorsaro) != 0)
    {
      usage();
      goto err;
    }

  /* create a record buffer */
  if (record == NULL &&
      (record = bgpstream_create_record()) == NULL) {
    fprintf(stderr, "ERROR: Could not create BGPStream record\n");
    return -1;
  }

  if((stream = bgpstream_create()) == NULL)
    {
      fprintf(stderr, "ERROR: Could not create BGPStream instance\n");
      return -1;
    }

  /* we only support MYSQL as the datasource */
  bgpstream_set_data_interface(stream, BS_MYSQL);
  //bgpstream_set_data_interface(stream, BS_CSVFILE);

  /* pass along the user's filter requests to bgpstream */

  /* types */
  for(i=0; i<types_cnt; i++)
    {
      bgpstream_add_filter(stream, BS_BGP_TYPE, types[i]);
      free(types[i]);
    }

  /* projects */
  for(i=0; i<projects_cnt; i++)
    {
      bgpstream_add_filter(stream, BS_PROJECT, projects[i]);
      free(projects[i]);
    }

  /* collectors */
  for(i=0; i<collectors_cnt; i++)
    {
      bgpstream_add_filter(stream, BS_COLLECTOR, collectors[i]);
      free(collectors[i]);
    }

  /* windows */
  for(i=0; i<windows_cnt; i++)
    {
      bgpstream_add_interval_filter(stream, BS_TIME_INTERVAL,
				    windows[i].start, windows[i].end);
      free(windows[i].start);
      free(windows[i].end);
    }

  /* blocking */
  if(blocking != 0)
    {
      bgpstream_set_blocking(stream);
    }

  if(bgpstream_init(stream) < 0) {
    fprintf(stderr, "ERROR: Could not init BGPStream\n");
    return -1;
  }

  /* let bgpcorsaro have the trace pointer */
  bgpcorsaro_set_stream(bgpcorsaro, stream);

  while (bgpcorsaro_shutdown == 0 &&
	 (rc = bgpstream_get_next_record(stream, record))>0) {
    /*bgpcorsaro_log(__func__, bgpcorsaro, "got a record!");*/
    if(bgpcorsaro_per_record(bgpcorsaro, record) != 0)
      {
	bgpcorsaro_log(__func__, bgpcorsaro, "bgpcorsaro_per_record failed");
	return -1;
      }
  }

  if (rc < 0) {
    bgpcorsaro_log(__func__, bgpcorsaro,
		   "bgpstream encountered an error processing records");
    return 1;
  }

  /* free the plugin strings */
  for(i=0;i<plugin_cnt;i++)
    {
      if(plugins[i] != NULL)
	free(plugins[i]);
    }

  /* free the template string */
  if(tmpl != NULL)
    free(tmpl);

  bgpcorsaro_finalize_output(bgpcorsaro);
  bgpcorsaro = NULL;
  if(stream != NULL)
    {
      bgpstream_destroy(stream);
      stream = NULL;
    }

  clean();
  return 0;

 err:
  /* if we bail early, let us be responsible and up the memory we alloc'd */
  for(i=0;i<plugin_cnt;i++)
    {
      if(plugins[i] != NULL)
	free(plugins[i]);
    }

  if(tmpl != NULL)
    free(tmpl);

  clean();

  return -1;
}
