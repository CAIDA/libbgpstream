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

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libtrace.h"

#include "utils.h"

#include "corsaro.h"
#include "corsaro_log.h"

#ifdef WITH_PLUGIN_SIXT
#include "corsaro_flowtuple.h"
#endif

/** @file
 *
 * @brief Code which uses libcorsaro to process a trace file and generate output
 *
 * @author Alistair King
 *
 */

/** The number of intervals in CAIDA's legacy flowtuple files */
#define LEGACY_INTERVAL_CNT 60

/** Indicates that Corsaro is waiting to shutdown */
volatile sig_atomic_t corsaro_shutdown = 0;

/** The number of SIGINTs to catch before aborting */
#define HARD_SHUTDOWN 3

/* for when we are reading trace files */
/** A pointer to a libtrace object */
static libtrace_t *trace = NULL;
/** A pointer to a libtrace packet */
static libtrace_packet_t *packet = NULL;
/** A pointer to a libtrace BPF filter */
static libtrace_filter_t *filter = NULL;

#ifdef WITH_PLUGIN_SIXT
/* for when we are reading flowtuple files */
/** A pointer to a corsaro_in object for use when reading flowtuple files */
static corsaro_in_t *corsaro_in = NULL;
/** A pointer to a corsaro record */
static corsaro_in_record_t *record = NULL;
#endif

/** A pointer to the instance of corsaro that we will drive */
static corsaro_t *corsaro = NULL;
/** Should a live interface be set to promiscuous mode? */
static int promisc = 0;
/** The number of legacy intervals we have processed */
static int legacy_intervals = 0;

/** Handles SIGINT gracefully and shuts down */
static void catch_sigint(int sig)
{
  corsaro_shutdown++;
  if(corsaro_shutdown == HARD_SHUTDOWN)
    {
      fprintf(stderr, "caught %d SIGINT's. shutting down NOW\n",
	      HARD_SHUTDOWN);
      exit(-1);
    }

  fprintf(stderr, "caught SIGINT, shutting down at the next opportunity\n");

  /* this is a global flag for libtrace, so probably doesn't hurt to call it but
     lets be a little careful */
  if(trace != NULL)
    {
      trace_interrupt();
    }

  signal(sig, catch_sigint);
}

/** Clean up all state before exit */
static void clean()
{
  if(packet != NULL)
    {
      trace_destroy_packet(packet);
      packet = NULL;
    }

#ifdef WITH_PLUGIN_SIXT
  if(record != NULL)
    {
      corsaro_in_free_record(record);
      record = NULL;
    }
#endif

  if(corsaro != NULL)
    {
      corsaro_finalize_output(corsaro);
    }
}

/** Prepare a new trace file for reading */
static int init_trace(char *tracefile)
{
  /* create a packet buffer */
  if (packet == NULL &&
      (packet = trace_create_packet()) == NULL) {
    perror("Creating libtrace packet");
    return -1;
  }

  trace = trace_create(tracefile);

  if (trace_is_err(trace)) {
    trace_perror(trace,"Opening trace file");
    return -1;
  }

  /* just in case someone is being dumb */
  if(legacy_intervals == 1)
    {
      fprintf(stderr, "WARNING: -l makes no sense when used with a pcap file\n");
    }

  /* enable promisc mode on the input if desired by the user */
  if(promisc == 1)
    {
      corsaro_log(__func__, corsaro,
		  "switching input to promiscuous mode");
      if(trace_config(trace,TRACE_OPTION_PROMISC,&promisc)) {
	trace_perror(trace,"ignoring: ");
      }
    }

  if (trace_start(trace) == -1) {
    trace_perror(trace,"Starting trace");
    return -1;
  }

  return 0;
}

/** Close a trace file */
static void close_trace()
{
  if(trace != NULL)
    {
      trace_destroy(trace);
      trace = NULL;
    }
}

/** Process a trace file */
static int process_trace(char *traceuri)
{
  if(init_trace(traceuri) != 0)
    {
      corsaro_log(__func__, corsaro,
		  "could not init trace for reading %s",
		  traceuri);
      return -1;
    }

  /* let corsaro have the trace pointer */
  corsaro_set_trace(corsaro, trace);

  while (corsaro_shutdown == 0 && trace_read_packet(trace, packet)>0) {
    if((filter == NULL || trace_apply_filter(filter, packet) > 0) &&
       corsaro_per_packet(corsaro, packet) != 0)
      {
	corsaro_log(__func__, corsaro, "corsaro_per_packet failed");
	return -1;
      }
  }

  if (trace_is_err(trace)) {
    trace_perror(trace,"Reading packets");
    corsaro_log(__func__, corsaro, "libtrace had an error reading packets");
    return 1;
  }

  if(trace_get_dropped_packets(trace) != UINT64_MAX)
    {
      corsaro_log(__func__, corsaro, "dropped pkt cnt: %"PRIu64"\n",
		  trace_get_dropped_packets(trace));
    }

  return 0;
}

#ifdef WITH_PLUGIN_SIXT
/** Prepare for processing a FlowTuple file */
static int init_flowtuple(const char *tuplefile)
{
  /* get the corsaro_in object we need to use to read the tuple file */
  if((corsaro_in = corsaro_alloc_input(tuplefile)) == NULL)
    {
      corsaro_log(__func__, corsaro,
		  "could not alloc corsaro_in to read %s",
		  tuplefile);
      return -1;
    }

  /* get a record buffer */
  if((record = corsaro_in_alloc_record(corsaro_in)) == NULL)
    {
      corsaro_log(__func__, corsaro, "could not alloc record");
      return -1;
    }

  /* start corsaro */
  if(corsaro_start_input(corsaro_in) != 0)
    {
      corsaro_log(__func__, corsaro, "could not start corsaro");
      return -1;
    }

  return 0;
}

/** Close a flowtuple input file */
static void close_flowtuple()
{
  if(record != NULL)
    {
      corsaro_in_free_record(record);
      record = NULL;
    }

  if(corsaro_in != NULL)
    {
      corsaro_finalize_input(corsaro_in);
      corsaro_in = NULL;
    }
}

/** Process a FlowTuple input file */
static int process_corsaro(const char *corsuri)
{
  off_t len = 0;
  corsaro_in_record_type_t type = CORSARO_IN_RECORD_TYPE_NULL;
  corsaro_interval_t *int_end = NULL;

  if(init_flowtuple(corsuri) != 0)
    {
      corsaro_log(__func__, corsaro,
		  "could not init flowtuple reading for %s", corsuri);
      return -1;
    }

  while (corsaro_shutdown == 0 &&
	 (len = corsaro_in_read_record(corsaro_in, &type, record)) > 0) {

    /* if we are in legacy interval mode, and this is an interval end
       record, then subtract one from the time, unless it is the last
       interval in the input file */
    /* because only CAIDA has legacy flowtuple files, we make an assumption
       that every 60 intervals we will see a 'last interval' style interval */
    if(legacy_intervals == 1 &&
       type == CORSARO_IN_RECORD_TYPE_IO_INTERVAL_END)
      {
	int_end = (corsaro_interval_t*)corsaro_in_get_record_data(record);

	assert(int_end->number <= LEGACY_INTERVAL_CNT);

	if(int_end->number < (LEGACY_INTERVAL_CNT - 1))
	  {
	    int_end->time--;
	  }
      }

    corsaro_per_record(corsaro, type, record);

    /* reset the type to NULL to indicate we don't care */
    type = CORSARO_IN_RECORD_TYPE_NULL;
  }

  if(len < 0)
    {
      corsaro_log(__func__, corsaro,
		  "corsaro_in_read_record failed to read record\n");
      return -1;
    }

  close_flowtuple();
  return 0;
}
#endif

/** Print usage information to stderr */
static void usage(const char *name)
{
  int i;
  char **plugin_names;
  int plugin_cnt;
  if((plugin_cnt = corsaro_get_plugin_names(&plugin_names)) < 0)
    {
      /* not much we can do */
      fprintf(stderr, "corsaro_get_plugin_names failed\n");
      return;
    }

  fprintf(stderr,
	  "usage: %s [-alP] -o outfile [-i interval] [-m mode] [-n name]\n"
	  "               [-p plugin] [-f filter] [-r intervals]"
	  " trace_uri [trace_uri...]\n"
	  "       -a            align the end time of the first interval\n"
	  "       -o <outfile>  use <outfile> as a template for file names.\n"
	  "                      - %%P => plugin name\n"
	  "                      - %%N => monitor name\n"
	  "                      - see man strftime(3) for more options\n"
	  "       -f <filter>   BPF filter to apply to packets\n"
	  "       -G            disable the global metadata output file\n"
	  "       -i <interval> distribution interval in seconds (default: %d)\n"
	  "       -l            the input file has legacy intervals (FlowTuple only)\n"
	  "       -L            disable logging to a file\n"
	  "       -m <mode>     output in 'ascii' or 'binary'. (default: binary)\n"
	  "       -n <name>     monitor name (default: "
	  STR(CORSARO_MONITOR_NAME)")\n"
	  "       -p <plugin>   enable the given plugin, -p can be used "
	  "multiple times (default: all)\n"
	  "                     available plugins:\n",
	  name, CORSARO_INTERVAL_DEFAULT);

  for(i = 0; i < plugin_cnt; i++)
    {
      fprintf(stderr, "                      - %s\n", plugin_names[i]);
    }
  fprintf(stderr,
	  "                     use -p \"<plugin_name> -?\" to see plugin options\n"
	  "       -P            enable promiscuous mode on the input"
	  " (if supported)\n"
	  "       -r            rotate output files after n intervals\n"
	  "       -R            rotate corsaro meta files after n intervals\n"
	  );

  corsaro_free_plugin_names(plugin_names, plugin_cnt);
}

/** Entry point for the Corsaro tool */
int main(int argc, char *argv[])
{
  int opt;
  int prevoptind;
  /* we MUST not use any of the getopt global vars outside of arg parsing */
  /* this is because the plugins can use get opt to parse their config */
  int lastopt;
  char *tmpl = NULL;
  char *name = NULL;
  char *bpf_filter = NULL;
  int i = -1000;
  int mode = CORSARO_FILE_MODE_BINARY;
  char *plugins[CORSARO_PLUGIN_ID_MAX];
  int plugin_cnt = 0;
  char *plugin_arg_ptr = NULL;
  int tracefile_cnt = 0;
  char *traceuri = "Multiple Traces";
  int align = 0;
  int rotate = 0;
  int meta_rotate = -1;
  int logfile_disable = 0;
  int global_file_disable = 0;

  signal(SIGINT, catch_sigint);

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":f:i:m:n:o:p:r:R:aGlLPv?")) >= 0)
    {
      if (optind == prevoptind + 2 && *optarg == '-' ) {
        opt = ':';
        -- optind;
      }
      switch(opt)
	{
	case 'G':
	  global_file_disable = 1;
	  break;

	case 'i':
	  i = atoi(optarg);
	  break;

	case 'l':
	  legacy_intervals = 1;
	  break;

	case 'L':
	  logfile_disable = 1;
	  break;

	case 'm':
	  if(strcmp(optarg, "ascii") == 0)
	    {
	      /* ascii output format */
	      mode = CORSARO_FILE_MODE_ASCII;
	    }
	  else if(strcmp(optarg, "binary") == 0)
	    {
	      mode = CORSARO_FILE_MODE_BINARY;
	    }
	  else
	    {
	      fprintf(stderr,
		      "ERROR: mode parameter must be 'ascii' or 'binary'\n");
	      usage(argv[0]);
	      exit(-1);
	    }
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

	case 'f':
	  bpf_filter = strdup(optarg);
	  break;

	case 'a':
	  align = 1;
	  break;

	case 'P':
	  promisc = 1;
	  break;

	case 'r':
	  rotate = atoi(optarg);
	  break;

	case 'R':
	  meta_rotate = atoi(optarg);
	  break;

	case ':':
	  fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
	  usage(argv[0]);
	  exit(-1);
	  break;

	case '?':
	case 'v':
	  fprintf(stderr, "corsaro version %d.%d.%d\n", CORSARO_MAJOR_VERSION,
		  CORSARO_MID_VERSION, CORSARO_MINOR_VERSION);
	  usage(argv[0]);
	  exit(0);
	  break;

	default:
	  usage(argv[0]);
	  exit(-1);
	}
    }

  /* store the value of the last index*/
  lastopt = optind;

  /* reset getopt for others */
  optind = 1;

  /* -- call NO library functions which may use getopt before here -- */
  /* this ESPECIALLY means corsaro_enable_plugin */

  /* ensure that there is at least one trace file given */
  if(lastopt > argc - 1)
    {
      fprintf(stderr, "ERROR: At least one trace file must be specified\n");
      usage(argv[0]);
      goto err;
    }

  tracefile_cnt = argc-lastopt;

  /* argv[lastopt] is the pcap file */
  /* if there is only one trace file, we want to tell corsaro what the uri was
     rather than the silly "Multiple Traces" default */
  if(tracefile_cnt == 1)
    {
      traceuri = argv[lastopt];
    }

  if(tmpl == NULL)
    {
      fprintf(stderr,
	      "ERROR: An output file template must be specified using -o\n");
      usage(argv[0]);
      goto err;
    }

  /* alloc corsaro */
  if((corsaro = corsaro_alloc_output(tmpl, mode)) == NULL)
    {
      usage(argv[0]);
      goto err;
    }

  /* create the bpf filter if specified */
  if(bpf_filter != NULL)
    {
      corsaro_log(__func__, corsaro, "compiling filter: \"%s\"",
		  bpf_filter);
      filter = trace_create_filter(bpf_filter);
    }

  /* keep a record of what this file was called */
  if(corsaro_set_traceuri(corsaro, traceuri) != 0)
    {
      corsaro_log(__func__, corsaro, "failed to set trace uri");
      goto err;
    }

  if(name != NULL && corsaro_set_monitorname(corsaro, name) != 0)
    {
      corsaro_log(__func__, corsaro, "failed to set monitor name");
      goto err;
    }

  if(i > -1000)
    {
      corsaro_set_interval(corsaro, i);
    }

  if(align == 1)
    {
      corsaro_set_interval_alignment(corsaro, CORSARO_INTERVAL_ALIGN_YES);
    }

  if(rotate > 0)
    {
      corsaro_set_output_rotation(corsaro, rotate);
    }

  if(meta_rotate >= 0)
    {
      corsaro_set_meta_output_rotation(corsaro, meta_rotate);
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

      if(corsaro_enable_plugin(corsaro, plugins[i], plugin_arg_ptr) != 0)
	{
	  fprintf(stderr, "ERROR: Could not enable plugin %s\n",
		  plugins[i]);
	  usage(argv[0]);
	  goto err;
	}
    }

  if(logfile_disable != 0)
    {
      corsaro_disable_logfile(corsaro);
    }

  if(global_file_disable != 0)
    {
      corsaro_disable_globalfile(corsaro);
    }

  if(corsaro_start_output(corsaro) != 0)
    {
      /* 02/25/13 - ak comments debug message */
      /*
      corsaro_log(__func__, corsaro, "failed to start corsaro");
      */
      usage(argv[0]);
      goto err;
    }

  for(i = lastopt; i < argc && corsaro_shutdown == 0; i++)
    {
      /* this should be a new file we're dealing with */
      assert(trace == NULL);
#ifdef WITH_PLUGIN_SIXT
      assert(corsaro_in == NULL);
#endif

      corsaro_log(__func__, corsaro, "processing %s", argv[i]);

#ifdef WITH_PLUGIN_SIXT
      /* are we dealing with a flowtuple file ? */
      /* @todo replace this with a corsaro_flowtuple call to check the magic */
      if(corsaro_flowtuple_probe_file(NULL, argv[i]) == 1)
	{
	  if(process_corsaro(argv[i]) != 0)
	    {
	      /* let process_corsaro log the error */
	      goto err;
	    }
	}
      else
	{
#endif
	  if(process_trace(argv[i]) != 0)
	    {
	      /* let process_trace log the error */
	      goto err;
	    }
	  /* close the trace unless this is the last trace */
	  if(i < argc-1)
	    {
	      close_trace();
	    }
#ifdef WITH_PLUGIN_SIXT
	}
#endif
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

  corsaro_finalize_output(corsaro);
  close_trace();
  corsaro = NULL;

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
