/*
 * This file is part of bgpwatcher
 *
 * Copyright (C) 2015 The Regents of the University of California.
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

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* include bgpwatcher server's public interface */
/* @@ never include the _int.h file from tools. */
#include "bgpwatcher_server.h"
#include "bgpwatcher_common.h"

/** Indicates that bgpwatcher is waiting to shutdown */
volatile sig_atomic_t bgpwatcher_shutdown = 0;

/** The number of SIGINTs to catch before aborting */
#define HARD_SHUTDOWN 3

static bgpwatcher_server_t *watcher = NULL;

/** Handles SIGINT gracefully and shuts down */
static void catch_sigint(int sig)
{
  bgpwatcher_shutdown++;
  if(bgpwatcher_shutdown == HARD_SHUTDOWN)
    {
      fprintf(stderr, "caught %d SIGINT's. shutting down NOW\n",
	      HARD_SHUTDOWN);
      exit(-1);
    }

  fprintf(stderr, "caught SIGINT, shutting down at the next opportunity\n");

  if(watcher != NULL)
    {
      bgpwatcher_server_stop(watcher);
    }

  signal(sig, catch_sigint);
}

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>]\n"
	  "       -c <client-uri>    0MQ-style URI to listen for clients on\n"
	  "                          (default: %s)\n"
          "       -C <client-pub-uri> 0MQ-style URI to publish tables on\n"
          "                          (default: %s)\n"
	  "       -i <interval-ms>   Time in ms between heartbeats to clients\n"
	  "                          (default: %d)\n"
	  "       -l <beats>         Number of heartbeats that can go by before \n"
	  "                          a client is declared dead (default: %d)\n"
	  "       -w <window-len>    Number of views in the window (default: %d)\n"
          "       -m <prefix>        Metric prefix (default: %s)\n",
	  name,
	  BGPWATCHER_CLIENT_URI_DEFAULT,
	  BGPWATCHER_CLIENT_PUB_URI_DEFAULT,
	  BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT,
	  BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT,
	  BGPWATCHER_SERVER_WINDOW_LEN,
          BGPWATCHER_METRIC_PREFIX_DEFAULT);
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  const char *client_uri = NULL;
  const char *client_pub_uri = NULL;

  uint64_t heartbeat_interval = BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;
  int heartbeat_liveness      = BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;
  char metric_prefix[BGPWATCHER_METRIC_PREFIX_LEN];

  strcpy(metric_prefix, BGPWATCHER_METRIC_PREFIX_DEFAULT);

  int window_len = BGPWATCHER_SERVER_WINDOW_LEN;

  signal(SIGINT, catch_sigint);

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":c:C:i:l:w:m:v?")) >= 0)
    {
      if (optind == prevoptind + 2 && *optarg == '-' ) {
        opt = ':';
        -- optind;
      }
      switch(opt)
	{
	case ':':
	  fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
	  usage(argv[0]);
	  return -1;
	  break;

	case 'c':
	  client_uri = optarg;
	  break;

	case 'C':
	  client_pub_uri = optarg;
	  break;

	case 'i':
	  heartbeat_interval = atoi(optarg);
	  break;

	case 'l':
	  heartbeat_liveness = atoi(optarg);
	  break;

	case 'w':
	  window_len = atoi(optarg);
	  break;

        case 'm':
            strcpy(metric_prefix, optarg);
            break;

	case '?':
	case 'v':
	  fprintf(stderr, "bgpwatcher version %d.%d.%d\n",
		  BGPWATCHER_MAJOR_VERSION,
		  BGPWATCHER_MID_VERSION,
		  BGPWATCHER_MINOR_VERSION);
	  usage(argv[0]);
	  return 0;
	  break;

	default:
	  usage(argv[0]);
	  return -1;
	  break;
	}
    }

  /* NB: once getopt completes, optind points to the first non-option
     argument */

  if((watcher = bgpwatcher_server_init()) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize bgpwatcher server\n");
      goto err;
    }

  bgpwatcher_server_set_metric_prefix(watcher, metric_prefix);

  if(client_uri != NULL)
    {
      bgpwatcher_server_set_client_uri(watcher, client_uri);
    }

  if(client_pub_uri != NULL)
    {
      bgpwatcher_server_set_client_pub_uri(watcher, client_pub_uri);
    }

  bgpwatcher_server_set_heartbeat_interval(watcher, heartbeat_interval);

  bgpwatcher_server_set_heartbeat_liveness(watcher, heartbeat_liveness);

  bgpwatcher_server_set_window_len(watcher, window_len);

  /* do work */
  /* this function will block until the server shuts down */
  bgpwatcher_server_start(watcher);

  /* this will always be set, normally to a SIGINT-caught message */
  bgpwatcher_server_perr(watcher);

  /* cleanup */
  bgpwatcher_server_free(watcher);

  /* complete successfully */
  return 0;

 err:
  if(watcher != NULL) {
    bgpwatcher_server_free(watcher);
  }
  return -1;
}
