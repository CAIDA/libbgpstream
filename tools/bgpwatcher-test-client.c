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
#include "utils.h"

#define TEST_TABLE_SIZE_DEFAULT 50
#define PEER_TABLE_SIZE 20

static uint64_t rx = 0;

static void handle_reply(bgpwatcher_client_t *client,
                         seq_num_t seq_num,
                         void *user)
{
#ifdef DEBUG
  fprintf(stderr, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
  fprintf(stderr, "HANDLE: Handling reply\n");
  fprintf(stderr, "Seq Num: %"PRIu32"\n", seq_num);
  fprintf(stderr, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\n");
#endif
  rx++;
}

/* pfx table */
static bgpstream_ip_address_t  test_pfx_peer_ip;
static char                   *test_pfx_collector_name;
static uint32_t                test_pfx_time;

/* pfx row */
static bgpstream_prefix_t      test_pfx_prefix;
static uint32_t                test_pfx_orig_asn;

/* peer table */
static char                   *test_peer_collector_name;
static uint32_t                test_peer_time;

/* peer row */
static bgpstream_ip_address_t  test_peer_peer_ip;
static uint8_t                 test_peer_status;

static void create_test_data()
{
  /* PREFIX TABLE */

  /* PEER IP */
  test_pfx_peer_ip.address.v4_addr.s_addr = 0x0DFAD982;
  test_pfx_peer_ip.type = BST_IPV4;

  /* COLLECTOR NAME */
  test_pfx_collector_name = "TEST-COLLECTOR-PFX";

  /* TIME */
  test_pfx_time = 1320969600;

  /* PREFIX */
  test_pfx_prefix.number.address.v4_addr.s_addr = 0x00E2ACC0;
  test_pfx_prefix.number.type = BST_IPV4;
  test_pfx_prefix.len = 24;

  /* ORIG ASN */
  test_pfx_orig_asn = 12345;

  /* -------------------------------------------------- */
  /* PEER TABLE */

  /* COLLECTOR NAME */
  test_peer_collector_name = "TEST-COLLECTOR-PEER";

  /* TIME */
  test_peer_time = 1410267600;

  /* PEER IP */
  /*2001:48d0:101:501:ec4:7aff:fe12:1108*/
  test_peer_peer_ip.address.v6_addr.s6_addr[0] = 0x20;
  test_peer_peer_ip.address.v6_addr.s6_addr[1] = 0x01;

  test_peer_peer_ip.address.v6_addr.s6_addr[2] = 0x48;
  test_peer_peer_ip.address.v6_addr.s6_addr[3] = 0xd0;

  test_peer_peer_ip.address.v6_addr.s6_addr[4] = 0x01;
  test_peer_peer_ip.address.v6_addr.s6_addr[5] = 0x01;

  test_peer_peer_ip.address.v6_addr.s6_addr[6] = 0x05;
  test_peer_peer_ip.address.v6_addr.s6_addr[7] = 0x01;

  test_peer_peer_ip.address.v6_addr.s6_addr[8] = 0x0e;
  test_peer_peer_ip.address.v6_addr.s6_addr[9] = 0xc4;

  test_peer_peer_ip.address.v6_addr.s6_addr[10] = 0x7a;
  test_peer_peer_ip.address.v6_addr.s6_addr[11] = 0xff;

  test_peer_peer_ip.address.v6_addr.s6_addr[12] = 0xfe;
  test_peer_peer_ip.address.v6_addr.s6_addr[13] = 0x12;

  test_peer_peer_ip.address.v6_addr.s6_addr[14] = 0x11;
  test_peer_peer_ip.address.v6_addr.s6_addr[15] = 0x08;
  test_peer_peer_ip.type = BST_IPV6;

  /* STATUS */
  test_peer_status = 0xF3;
}

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>]\n"
	  "       -i <interval-ms>      Time in ms between heartbeats to server\n"
	  "                               (default: %d)\n"
	  "       -l <beats>            Number of heartbeats that can go by before the\n"
	  "                               server is declared dead (default: %d)\n"
	  "       -m <msg-timeout>      Time to wait before re-sending message to server\n"
	  "                               (default: %d)\n"
	  "       -M <msg-retries>      Number of times to retry a request before giving up\n"
	  "                               (default: %d)\n"
	  "       -n <identity>         Globally unique client name (default: random)\n"
	  "       -r <retry-min>        Min wait time (in msec) before reconnecting server\n"

	  "                               (default: %d)\n"
	  "       -R <retry-max>        Max wait time (in msec) before reconnecting server\n"
	  "                               (default: %d)\n"
	  "       -s <server-uri>       0MQ-style URI to connect to server on\n"
	  "                               (default: %s)\n"
	  "       -t <shutdown-timeout> Time to wait for requests on shutdown\n"
	  "                               (default: %d)\n"
	  "       -T <table-size>       Size of test tables (default: %d)\n",
	  name,
	  BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT,
	  BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT,
	  BGPWATCHER_CLIENT_REQUEST_TIMEOUT_DEFAULT,
	  BGPWATCHER_CLIENT_REQUEST_RETRIES_DEFAULT,
	  BGPWATCHER_RECONNECT_INTERVAL_MIN,
	  BGPWATCHER_RECONNECT_INTERVAL_MAX,
	  BGPWATCHER_CLIENT_SERVER_URI_DEFAULT,
	  BGPWATCHER_CLIENT_SHUTDOWN_LINGER_DEFAULT,
	  TEST_TABLE_SIZE_DEFAULT);
}

int main(int argc, char **argv)
{
  int i;
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  const char *server_uri = NULL;
  const char *identity = NULL;

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

  /* test structures */
  int rc;
  bgpwatcher_client_pfx_table_t *pfx_table = NULL;
  bgpwatcher_client_peer_table_t *peer_table = NULL;
  /* initialize test data */
  create_test_data();

  uint32_t test_table_size = TEST_TABLE_SIZE_DEFAULT;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":i:l:m:M:n:r:R:s:t:T:v?")) >= 0)
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

	case 'm':
	  request_timeout = atoi(optarg);
	  break;

	case 'M':
	  request_retries = atoi(optarg);
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
  intents = BGPWATCHER_PRODUCER_INTENT_PREFIX | BGPWATCHER_PRODUCER_INTENT_PEER;

  if((client =
      bgpwatcher_client_init(interests, intents)) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize bgpwatcher client\n");
      usage(argv[0]);
      goto err;
    }

  bgpwatcher_client_set_cb_handle_reply(client, handle_reply);

  if(server_uri != NULL &&
     bgpwatcher_client_set_server_uri(client, server_uri) != 0)
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

  fprintf(stderr, "TEST: Init tables... ");
  if((pfx_table = bgpwatcher_client_pfx_table_create(client)) == NULL)
    {
      fprintf(stderr, "Could not create table\n");
      goto err;
    }

  if((peer_table = bgpwatcher_client_peer_table_create(client)) == NULL)
    {
      fprintf(stderr, "Could not create table\n");
      goto err;
    }
  fprintf(stderr, "done\n");

  fprintf(stderr, "TEST: Starting client... ");
  if(bgpwatcher_client_start(client) != 0)
    {
      bgpwatcher_client_perr(client);
      goto err;
    }
  fprintf(stderr, "done\n");

  /* issue a bunch of requests */
  fprintf(stderr, "--------------------[ PREFIX START ]--------------------\n");
  if((rc = bgpwatcher_client_pfx_table_begin(pfx_table,
                                             test_pfx_collector_name,
                                             &test_pfx_peer_ip,
                                             test_pfx_time)) < 0)
    {
      fprintf(stderr, "Could not begin pfx table\n");
      goto err;
    }
  fprintf(stderr, "TEST: Sending pfx table begin: %d\n", rc);

  fprintf(stderr, "TEST: Sending %d pfx table records\n", test_table_size);
  for(i=0; i<test_table_size; i++)
    {
      if((rc =
          bgpwatcher_client_pfx_table_add(pfx_table,
                                          &test_pfx_prefix,
                                          test_pfx_orig_asn)) < 0)
	{
	  fprintf(stderr, "Could not add pfx info to table\n");
	  goto err;
	}
    }

  if((rc = bgpwatcher_client_pfx_table_end(pfx_table)) < 0)
    {
      fprintf(stderr, "Could not end table\n");
      goto err;
    }
  fprintf(stderr, "TEST: Sending table end: %d\n", rc);
  fprintf(stderr, "--------------------[ PREFIX DONE ]--------------------\n\n");

  fprintf(stderr, "--------------------[ PEER START ]--------------------\n");
  if((rc = bgpwatcher_client_peer_table_begin(peer_table,
                                              test_peer_collector_name,
                                              test_peer_time)) < 0)
    {
      fprintf(stderr, "Could not begin peer table\n");
      goto err;
    }
  fprintf(stderr, "TEST: Sending peer table begin: %d\n", rc);

  fprintf(stderr, "TEST: Sending %d peer table records\n", PEER_TABLE_SIZE);
  for(i=0; i<PEER_TABLE_SIZE; i++)
    {
      if((rc = bgpwatcher_client_peer_table_add(peer_table,
                                                &test_peer_peer_ip,
                                                test_peer_status)) < 0)
	{
	  fprintf(stderr, "Could not add peer info to table\n");
	  goto err;
	}
    }

  if((rc = bgpwatcher_client_peer_table_end(peer_table)) < 0)
    {
      fprintf(stderr, "Could not end peer table\n");
      goto err;
    }
  fprintf(stderr, "TEST: Sending peer table end: %d\n", rc);
  fprintf(stderr, "--------------------[ PEER DONE ]--------------------\n\n");


  fprintf(stderr, "TEST: Shutting down...\n");
  bgpwatcher_client_pfx_table_free(&pfx_table);
  bgpwatcher_client_peer_table_free(&peer_table);

  bgpwatcher_client_stop(client);
  bgpwatcher_client_perr(client);

  /* cleanup */
  bgpwatcher_client_free(client);
  fprintf(stderr, "TEST: Shutdown complete\n");

  fprintf(stderr, "STATS: Sent %d requests\n", rc+1);
  fprintf(stderr, "STATS: Rx %"PRIu64" replies\n", rx);

  /* complete successfully */
  return 0;

 err:
  bgpwatcher_client_perr(client);
  bgpwatcher_client_pfx_table_free(&pfx_table);
  bgpwatcher_client_peer_table_free(&peer_table);
  if(client != NULL) {
    bgpwatcher_client_free(client);
  }
  return -1;
}
