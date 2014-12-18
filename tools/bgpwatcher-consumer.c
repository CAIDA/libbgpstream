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
#include <time.h>       /* time */

/* this must be all we include from bgpwatcher */
#include <bgpwatcher_client.h>
#include <bgpwatcher_view.h>
#include <bgpwatcher_consumer_manager.h>

#include "config.h"
#include "utils.h"

static bw_consumer_manager_t *manager = NULL;

static void consumer_usage()
{
  assert(manager != NULL);
  bwc_t **avail_consumers = NULL;
  int i;

  /* get the available consumers from the manager */
  avail_consumers = bw_consumer_manager_get_all_consumers(manager);

  fprintf(stderr,
	  "                               available consumers:\n");
  for(i = 0; i < BWC_ID_LAST; i++)
    {
      /* skip unavailable consumers */
      if(avail_consumers[i] == NULL)
	{
	  continue;
	}

      assert(bwc_get_name(avail_consumers[i]));
      fprintf(stderr,
	      "                                - %s\n",
	      bwc_get_name(avail_consumers[i]));
    }
}

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>]\n"
	  "       -c <consumer>         Consumer to active (can be used multiple times)\n",
	  name);
  consumer_usage();
  fprintf(stderr,
	  "       -i <interval-ms>      Time in ms between heartbeats to server\n"
	  "                               (default: %d)\n"
          "       -I <interest>         Advertise the given interest. May be used multiple times\n"
          "                               One of: first-full, full, partial\n"
	  "       -l <beats>            Number of heartbeats that can go by before the\n"
	  "                               server is declared dead (default: %d)\n"
	  "       -n <identity>         Globally unique client name (default: random)\n"
	  "       -r <retry-min>        Min wait time (in msec) before reconnecting server\n"

	  "                               (default: %d)\n"
	  "       -R <retry-max>        Max wait time (in msec) before reconnecting server\n"
	  "                               (default: %d)\n"
	  "       -s <server-uri>       0MQ-style URI to connect to server on\n"
	  "                               (default: %s)\n"
          "       -s <server-sub-uri>   0MQ-style URI to subscribe to tables on\n"
          "                               (default: %s)\n",
	  BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT,
	  BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT,
	  BGPWATCHER_RECONNECT_INTERVAL_MIN,
	  BGPWATCHER_RECONNECT_INTERVAL_MAX,
	  BGPWATCHER_CLIENT_SERVER_URI_DEFAULT,
	  BGPWATCHER_CLIENT_SERVER_SUB_URI_DEFAULT);
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  char *consumer_cmds[BWC_ID_LAST];
  int consumer_cmds_cnt = 0;
  int i;

  const char *server_uri = NULL;
  const char *server_sub_uri = NULL;
  const char *identity = NULL;

  uint64_t heartbeat_interval = BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;
  int heartbeat_liveness      = BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;
  uint64_t reconnect_interval_min = BGPWATCHER_RECONNECT_INTERVAL_MIN;
  uint64_t reconnect_interval_max = BGPWATCHER_RECONNECT_INTERVAL_MAX;

  uint8_t interests = 0;
  uint8_t intents = 0;
  bgpwatcher_client_t *client = NULL;

  int rx_interests;
  bgpwatcher_view_t *view = NULL;

  /* better just grab a pointer to the manager before anybody goes crazy and
     starts dumping usage strings */
  if((manager = bw_consumer_manager_create()) == NULL)
    {
      fprintf(stderr, "ERROR: Could not initialize consumer manager\n");
      return -1;
    }

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":c:i:I:l:n:r:R:s:S:v?")) >= 0)
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
	  if(consumer_cmds_cnt >= BWC_ID_LAST-1)
	    {
	      fprintf(stderr, "ERROR: At most %d consumers can be enabled\n",
		      BWC_ID_LAST);
	      usage(argv[0]);
	      return -1;
	    }
	  consumer_cmds[consumer_cmds_cnt++] = optarg;
	  break;

	case 'i':
	  heartbeat_interval = atoi(optarg);
	  break;

        case 'I':
          if(strcmp(optarg, "first-full") == 0)
            {
              interests |= BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL;
            }
          else if(strcmp(optarg, "full") == 0)
            {
              interests |= BGPWATCHER_CONSUMER_INTEREST_FULL;
            }
          else if(strcmp(optarg, "partial") == 0)
            {
              interests |= BGPWATCHER_CONSUMER_INTEREST_PARTIAL;
            }
          else
            {
              fprintf(stderr,
                      "ERROR: Invalid interest (%s)."
                      "Interest must be one of "
                      "'first-full', 'full', or 'partial'\n", optarg);
              usage(argv[0]);
              return -1;
            }
          break;

	case 'l':
	  heartbeat_liveness = atoi(optarg);
	  break;

	case 'n':
	  identity = optarg;
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

	case 'S':
	  server_sub_uri = optarg;
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

  if(consumer_cmds_cnt == 0)
    {
      fprintf(stderr,
	      "ERROR: Consumer(s) must be specified using -c\n");
      usage(argv[0]);
      return -1;
    }

  for(i=0; i<consumer_cmds_cnt; i++)
    {
      assert(consumer_cmds[i] != NULL);
      if(bw_consumer_manager_enable_consumer_from_str(manager,
						      consumer_cmds[i]) == NULL)
        {
          usage(argv[0]);
          goto err;
        }
    }

  if(interests == 0)
    {
      fprintf(stderr, "WARN: Defaulting to FIRST-FULL interest\n");
      fprintf(stderr, "WARN: Specify interests using -p <interest>\n");
      interests = BGPWATCHER_CONSUMER_INTEREST_FIRSTFULL;
    }

  if((client =
      bgpwatcher_client_init(interests, intents)) == NULL)
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

  if(server_sub_uri != NULL &&
     bgpwatcher_client_set_server_sub_uri(client, server_sub_uri) != 0)
    {
      bgpwatcher_client_perr(client);
      goto err;
    }

  if(identity != NULL &&
     bgpwatcher_client_set_identity(client, identity) != 0)
    {
      bgpwatcher_client_perr(client);
      goto err;
    }

  bgpwatcher_client_set_heartbeat_interval(client, heartbeat_interval);

  bgpwatcher_client_set_heartbeat_liveness(client, heartbeat_liveness);

  bgpwatcher_client_set_reconnect_interval_min(client, reconnect_interval_min);

  bgpwatcher_client_set_reconnect_interval_max(client, reconnect_interval_max);

  fprintf(stderr, "INFO: Starting client... ");
  if(bgpwatcher_client_start(client) != 0)
    {
      bgpwatcher_client_perr(client);
      goto err;
    }
  fprintf(stderr, "done\n");

  if((view = bgpwatcher_view_create(NULL)) == NULL)
    {
      fprintf(stderr, "ERROR: Could not create view\n");
      goto err;
    }

  while((rx_interests =
         bgpwatcher_client_recv_view(client,
                                     BGPWATCHER_CLIENT_RECV_MODE_BLOCK,
                                     view)) > 0)
    {
      if(bw_consumer_manager_process_view(manager, rx_interests, view) != 0)
	{
	  fprintf(stderr, "ERROR: Failed to process view at %d\n",
		  bgpwatcher_view_time(view));
	  goto err;
	}

      bgpwatcher_view_clear(view);
    }

  fprintf(stderr, "INFO: Shutting down...\n");

  bgpwatcher_client_stop(client);
  bgpwatcher_client_perr(client);

  /* cleanup */
  bgpwatcher_client_free(client);
  bgpwatcher_view_destroy(view);
  bw_consumer_manager_destroy(&manager);
  fprintf(stderr, "INFO: Shutdown complete\n");

  /* complete successfully */
  return 0;

 err:
  bgpwatcher_client_perr(client);
  if(client != NULL) {
    bgpwatcher_client_free(client);
  }
  bgpwatcher_view_destroy(view);
  bw_consumer_manager_destroy(&manager);
  return -1;
}
