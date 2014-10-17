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

static uint64_t rx_success = 0;
static uint64_t rx_fail = 0;

static void handle_reply(bgpwatcher_client_t *client, seq_num_t seq_num,
			 int rc, void *user)
{
#ifdef DEBUG
  fprintf(stderr, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
  fprintf(stderr, "HANDLE: Handling reply\n");
  fprintf(stderr, "Seq Num: %"PRIu32"\n", seq_num);
  fprintf(stderr, "Ret Code: %d\n", rc);
  fprintf(stderr, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n\n");
#endif
  if(rc == 0)
    {
      rx_success++;
    }
  else
    {
      rx_fail++;
    }
}

static bgpwatcher_peer_record_t *create_test_peer()
{
  bgpwatcher_peer_record_t *rec;

  rec = bgpwatcher_peer_record_init();

  if(rec != NULL)
    {
      ((struct sockaddr_in6*)&rec->ip)->sin6_family = AF_INET6;

      /*2001:48d0:101:501:ec4:7aff:fe12:1108*/
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[0] = 0x20;
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[1] = 0x01;

      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[2] = 0x48;
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[3] = 0xd0;

      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[4] = 0x01;
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[5] = 0x01;

      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[6] = 0x05;
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[7] = 0x01;

      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[8] = 0x0e;
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[9] = 0xc4;

      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[10] = 0x7a;
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[11] = 0xff;

      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[12] = 0xfe;
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[13] = 0x12;

      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[14] = 0x11;
      ((struct sockaddr_in6*)&rec->ip)->sin6_addr.s6_addr[15] = 0x08;

      rec->status = 0xF3;
    }

  return rec;
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

  bgpwatcher_client_t *client = NULL;

  /* test structures */
  int rc;
  bgpwatcher_client_pfx_table_t *pfx_table = NULL;

  /* initialize test prefix row data */
  bgpstream_prefix_t prefix;
  prefix.number.address.v4_addr.s_addr = 0xC0ACE200;
  prefix.number.type = BST_IPV4;
  prefix.len = 24;
  bgpstream_ip_address_t peer_ip;
  peer_ip.address.v4_addr.s_addr = 0x82D9FA0D;
  peer_ip.type = BST_IPV4;
  uint32_t orig_asn = 12345;
  char *collector_name = "TEST-COLLECTOR";

  uint32_t pfx_table_time = 1320969600;

  bgpwatcher_client_peer_table_t *peer_table = NULL;
  bgpwatcher_peer_record_t *peer = NULL;
  uint32_t peer_table_time = 1410267600;

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

  if((client = bgpwatcher_client_init()) == NULL)
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

  fprintf(stderr, "TEST: Init tables and records... ");
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
  if((peer = create_test_peer()) == NULL)
    {
      fprintf(stderr, "Could not create test peer\n");
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
  if((rc = bgpwatcher_client_pfx_table_begin(pfx_table, pfx_table_time)) < 0)
    {
      fprintf(stderr, "Could not begin table\n");
      goto err;
    }
  fprintf(stderr, "TEST: Sending table begin: %d\n", rc);

  fprintf(stderr, "TEST: Sending %d pfx table records\n", test_table_size);
  for(i=0; i<test_table_size; i++)
    {
      if((rc = bgpwatcher_client_pfx_table_add(pfx_table,
                                               &prefix, &peer_ip,
                                               orig_asn, collector_name)) < 0)
	{
	  fprintf(stderr, "Could not add pfx to table\n");
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
  if((rc = bgpwatcher_client_peer_table_begin(peer_table, peer_table_time)) < 0)
    {
      fprintf(stderr, "Could not begin table\n");
      goto err;
    }
  fprintf(stderr, "TEST: Sending table begin: %d\n", rc);

  fprintf(stderr, "TEST: Sending %d peer table records\n", PEER_TABLE_SIZE);
  for(i=0; i<PEER_TABLE_SIZE; i++)
    {
      if((rc = bgpwatcher_client_peer_table_add(peer_table, peer)) < 0)
	{
	  fprintf(stderr, "Could not add peer to table\n");
	  goto err;
	}
    }

  if((rc = bgpwatcher_client_peer_table_end(peer_table)) < 0)
    {
      fprintf(stderr, "Could not end table\n");
      goto err;
    }
  fprintf(stderr, "TEST: Sending table end: %d\n", rc);
  fprintf(stderr, "--------------------[ PEER DONE ]--------------------\n\n");


  fprintf(stderr, "TEST: Shutting down...\n");
  bgpwatcher_client_pfx_table_free(&pfx_table);
  bgpwatcher_peer_record_free(&peer);
  bgpwatcher_client_peer_table_free(&peer_table);

  bgpwatcher_client_stop(client);
  bgpwatcher_client_perr(client);

  /* cleanup */
  bgpwatcher_client_free(client);
  fprintf(stderr, "TEST: Shutdown complete\n");

  fprintf(stderr, "STATS: Sent %d requests\n", rc+1);
  fprintf(stderr, "STATS: Rx %"PRIu64" success replies\n", rx_success);
  fprintf(stderr, "STATS: Rx %"PRIu64" failure replies\n", rx_fail);

  /* complete successfully */
  return 0;

 err:
  bgpwatcher_client_perr(client);
  bgpwatcher_client_pfx_table_free(&pfx_table);
  bgpwatcher_peer_record_free(&peer);
  bgpwatcher_client_peer_table_free(&peer_table);
  if(client != NULL) {
    bgpwatcher_client_free(client);
  }
  return -1;
}
