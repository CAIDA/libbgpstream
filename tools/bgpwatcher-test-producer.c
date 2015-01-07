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

#include "config.h"
#include "utils.h"

#define TEST_TABLE_NUM_DEFAULT 1
#define TEST_TABLE_SIZE_DEFAULT 50
#define TEST_PEER_NUM_DEFAULT 1

#define ASN_MAX 50000

/* pfx table */
static char                   *test_collector_name;
static uint32_t                test_time;
static uint32_t                test_peer_first_ip;
static bl_addr_storage_t       test_peer_ip;
static uint8_t                 test_peer_status;

/* pfx row */
static bl_pfx_storage_t        test_prefix;
static uint32_t                test_prefix_first_addr;
static uint32_t                test_orig_asn;

static void create_test_data()
{
  /* PREFIX TABLE */

  /* TIME */
  test_time = 1320969600;

  /* COLLECTOR NAME */
  test_collector_name = "TEST-COLLECTOR";

  /* FIRST PEER IP */
  test_peer_ip.ipv4.s_addr = test_peer_first_ip = 0x00FAD982; /* add one each time */
  test_peer_ip.version = BL_ADDR_IPV4;

  /* FIRST PEER STATUS */
  test_peer_status = 0x01;

  /* FIRST PREFIX */
  test_prefix_first_addr = test_prefix.address.ipv4.s_addr = 0x00000000;
  test_prefix.address.version = BL_ADDR_IPV4;
  test_prefix.mask_len = 24;

  /* ORIG ASN */
  test_orig_asn = 1;
}

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>]\n"
          "       -c                    Randomly decide if peers are up or down\n"
          "       -C                    Initial test time (default: %d)\n"
	  "       -i <interval-ms>      Time in ms between heartbeats to server\n"
	  "                               (default: %d)\n"
	  "       -l <beats>            Number of heartbeats that can go by before the\n"
	  "                               server is declared dead (default: %d)\n"
	  "       -m <msg-timeout>      Time to wait before re-sending message to server\n"
	  "                               (default: %d)\n"
	  "       -M <msg-retries>      Number of times to retry a request before giving up\n"
	  "                               (default: %d)\n"
	  "       -n <identity>         Globally unique client name (default: random)\n"
          "       -N <table-cnt>        Number of tables (default: %d)\n"
          "       -p                    Randomly decide if a peer observes each prefix\n"
          "       -P <peer-cnt>         Number of peers (default: %d)\n"
	  "       -r <retry-min>        Min wait time (in msec) before reconnecting server\n"

	  "                               (default: %d)\n"
	  "       -R <retry-max>        Max wait time (in msec) before reconnecting server\n"
	  "                               (default: %d)\n"
	  "       -s <server-uri>       0MQ-style URI to connect to server on\n"
	  "                               (default: %s)\n"
          "       -s <server-sub-uri>   0MQ-style URI to subscribe to tables on\n"
          "                               (default: %s)\n"
	  "       -t <shutdown-timeout> Time to wait for requests on shutdown\n"
	  "                               (default: %d)\n"
	  "       -T <table-size>       Size of prefix tables (default: %d)\n",
	  name,
          test_time,
	  BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT,
	  BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT,
	  BGPWATCHER_CLIENT_REQUEST_TIMEOUT_DEFAULT,
	  BGPWATCHER_CLIENT_REQUEST_RETRIES_DEFAULT,
          TEST_TABLE_NUM_DEFAULT,
          TEST_PEER_NUM_DEFAULT,
	  BGPWATCHER_RECONNECT_INTERVAL_MIN,
	  BGPWATCHER_RECONNECT_INTERVAL_MAX,
	  BGPWATCHER_CLIENT_SERVER_URI_DEFAULT,
	  BGPWATCHER_CLIENT_SERVER_SUB_URI_DEFAULT,
	  BGPWATCHER_CLIENT_SHUTDOWN_LINGER_DEFAULT,
	  TEST_TABLE_SIZE_DEFAULT);
}

int main(int argc, char **argv)
{
  int i, tbl, peer, peer_id;
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  const char *server_uri = NULL;
  const char *server_sub_uri = NULL;
  const char *identity = NULL;

  int use_random_peers = 0;
  int use_random_pfxs = 0;

  uint64_t heartbeat_interval = BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;
  int heartbeat_liveness      = BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;
  uint64_t reconnect_interval_min = BGPWATCHER_RECONNECT_INTERVAL_MIN;
  uint64_t reconnect_interval_max = BGPWATCHER_RECONNECT_INTERVAL_MAX;
  uint64_t shutdown_linger = BGPWATCHER_CLIENT_SHUTDOWN_LINGER_DEFAULT;
  uint64_t request_timeout = BGPWATCHER_CLIENT_REQUEST_TIMEOUT_DEFAULT;
  int request_retries = BGPWATCHER_CLIENT_REQUEST_RETRIES_DEFAULT;

  uint8_t interests = 0;
  uint8_t intents = 0;
  bgpwatcher_client_t *client = NULL;

  /* initialize test data */
  create_test_data();

  uint32_t test_table_size = TEST_TABLE_SIZE_DEFAULT;
  uint32_t test_table_num = TEST_TABLE_NUM_DEFAULT;
  uint32_t test_peer_num = TEST_PEER_NUM_DEFAULT;

  uint32_t pfx_cnt;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":cC:i:l:m:M:n:N:pP:r:R:s:S:t:T:v?")) >= 0)
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
          use_random_peers = 1;
          break;

        case 'C':
          test_time = atoi(optarg);
          break;

	case 'i':
	  heartbeat_interval = atoi(optarg);
	  break;

	case 'l':
	  heartbeat_liveness = atoi(optarg);
	  break;

	case 'm':
	  request_timeout = atoi(optarg);
	  break;

	case 'M':
	  request_retries = atoi(optarg);
	  break;

	case 'n':
	  identity = optarg;
	  break;

	case 'N':
	  test_table_num = atoi(optarg);
	  break;

        case 'p':
          use_random_pfxs = 1;
          break;

	case 'P':
	  test_peer_num = atoi(optarg);
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

	case 't':
	  shutdown_linger = atoi(optarg);
	  break;

	case 'T':
	  test_table_size = atoi(optarg);
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

  interests = 0;
  intents = BGPWATCHER_PRODUCER_INTENT_PREFIX;

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

  bgpwatcher_client_set_shutdown_linger(client, shutdown_linger);

  bgpwatcher_client_set_request_timeout(client, request_timeout);

  bgpwatcher_client_set_request_retries(client, request_retries);

  fprintf(stderr, "TEST: Starting client... ");
  if(bgpwatcher_client_start(client) != 0)
    {
      bgpwatcher_client_perr(client);
      goto err;
    }
  fprintf(stderr, "done\n");

  /* issue a bunch of requests */
  
  /* initialize random seed: */  
  srand(1);


  for(tbl = 0; tbl < test_table_num; tbl++)
    {
      fprintf(stderr,
              "--------------------[ PREFIX START %03d ]--------------------\n",
              tbl);

      if(bgpwatcher_client_pfx_table_begin(client,
                                           test_time+(tbl*60),
                                           test_collector_name,
                                           test_peer_num) != 0)
        {
          fprintf(stderr, "Could not begin pfx table\n");
          goto err;
        }

      /* reset peer ip */
      test_peer_ip.ipv4.s_addr = test_peer_first_ip;

      fprintf(stderr, "TEST: Simulating %d peer(s)\n", test_peer_num);
      for(peer = 0; peer < test_peer_num; peer++)
        {
          test_peer_ip.ipv4.s_addr += htonl(1);
          // returns number from 0 to 2
	  test_peer_status = (use_random_peers) ? rand() % 3 : 2;
          if((peer_id =
              bgpwatcher_client_pfx_table_add_peer(client,
                                                   &test_peer_ip,
						   test_peer_status)) < 0)
            {
              fprintf(stderr, "Could not add peer to table\n");
              goto err;
            }
          fprintf(stderr, "TEST: Added peer %d ", peer_id);

	  if(test_peer_status != 2)
            {
              fprintf(stderr, "(down)\n");
              continue;
            }
          else
            {
              fprintf(stderr, "(up)\n");
            }

          test_prefix.address.ipv4.s_addr = test_prefix_first_addr;
          pfx_cnt = 0;
          for(i=0; i<test_table_size; i++)
            {
              test_prefix.address.ipv4.s_addr =
                htonl(ntohl(test_prefix.address.ipv4.s_addr) + 256);
              test_orig_asn = (test_orig_asn+1) % ASN_MAX;

             /* there is a 1/10 chance that we don't observe this prefix */
              if(use_random_pfxs && (rand() % 10) == 0)
                {
                  /* randomly we don't see this prefix */
                  continue;
                }
              if(bgpwatcher_client_pfx_table_add(client,
                                                 peer_id,
                                                 &test_prefix,
                                                 test_orig_asn) != 0)
                {
                  fprintf(stderr, "Could not add pfx info to table\n");
                  goto err;
                }
              pfx_cnt++;
            }
          fprintf(stderr, "TEST: Added %d prefixes...\n", pfx_cnt);
        }

      if(bgpwatcher_client_pfx_table_end(client) != 0)
        {
          fprintf(stderr, "Could not end table\n");
          goto err;
        }

      fprintf(stderr,
              "--------------------[ PREFIX DONE %03d ]--------------------\n\n",
              tbl);
    }

  fprintf(stderr, "TEST: Shutting down...\n");

  bgpwatcher_client_stop(client);
  bgpwatcher_client_perr(client);

  /* cleanup */
  bgpwatcher_client_free(client);
  fprintf(stderr, "TEST: Shutdown complete\n");

  /* complete successfully */
  return 0;

 err:
  bgpwatcher_client_perr(client);
  if(client != NULL) {
    bgpwatcher_client_free(client);
  }
  return -1;
}
