/*
 * bgpwatcher
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* this must be all we include from bgpwatcher */
#include <bgpwatcher_client.h>

#include "config.h"

/** @todo add re-transmit options here */

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>]\n"
	  "       -i <interval-ms>   Time in ms between heartbeats to server\n"
	  "                          (default: %d)\n"
	  "       -l <beats>         Number of heartbeats that can go by before \n"
	  "                          the server is declared dead (default: %d)\n"
	  "       -r <retry-min>     Min time in ms to wait before reconnecting to server\n"

	  "                          (default: %d)\n"
	  "       -R <retry-max>     Max time in ms to wait before reconnecting to server\n"
	  "                          (default: %d)\n"
	  "       -s <server-uri>    0MQ-style URI to connect to server on\n"
	  "                          (default: %s)\n",
	  name,
	  BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT,
	  BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT,
	  BGPWATCHER_RECONNECT_INTERVAL_MIN,
	  BGPWATCHER_RECONNECT_INTERVAL_MAX,
	  BGPWATCHER_CLIENT_SERVER_URI_DEFAULT);
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  const char *server_uri = NULL;

  uint64_t heartbeat_interval = BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;
  int heartbeat_liveness      = BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;
  uint64_t reconnect_interval_min = BGPWATCHER_RECONNECT_INTERVAL_MIN;
  uint64_t reconnect_interval_max = BGPWATCHER_RECONNECT_INTERVAL_MAX;

  bgpwatcher_client_t *client = NULL;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":i:l:r:R:s:v?")) >= 0)
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

	case 'i':
	  heartbeat_interval = atoi(optarg);
	  break;

	case 'l':
	  heartbeat_liveness = atoi(optarg);
	  break;

	case 'r':
	  reconnect_interval_min = atoi(optarg);
	  break;

	case 'R':
	  reconnect_interval_max = atoi(optarg);
	  break;

	case 's':
	  server_uri = optarg;
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

  if((client = bgpwatcher_client_init()) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize bgpwatcher client\n");
      usage(argv[0]);
      goto err;
    }

  if(server_uri != NULL &&
     bgpwatcher_client_set_server_uri(client, server_uri) != 0)
    {
      bgpwatcher_client_perr(client);
      goto err;
    }

  bgpwatcher_client_set_heartbeat_interval(client, heartbeat_interval);

  bgpwatcher_client_set_heartbeat_liveness(client, heartbeat_liveness);

  bgpwatcher_client_set_reconnect_interval_min(client, reconnect_interval_min);

  bgpwatcher_client_set_reconnect_interval_max(client, reconnect_interval_max);

  if(bgpwatcher_client_start(client) != 0)
    {
      bgpwatcher_client_perr(client);
      goto err;
    }

  /* issue a bunch of requests */
  fprintf(stderr, "issuing requests here!\n");

  bgpwatcher_client_stop(client);
  bgpwatcher_client_perr(client);

  /* cleanup */
  bgpwatcher_client_free(client);

  /* complete successfully */
  return 0;

 err:
  if(client != NULL) {
    bgpwatcher_client_free(client);
  }
  return -1;
}
