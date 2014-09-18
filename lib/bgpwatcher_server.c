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


#include <stdint.h>
#include <stdio.h>

#include <bgpwatcher_server.h>

#include "utils.h"

#define ERR (&server->err)

static int run_server(bgpwatcher_server_t *server)
{
  fprintf(stderr, "running server\n");
  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INTERRUPT,
			 "Interrupted");
  return -1;
}

bgpwatcher_server_t *bgpwatcher_server_init(
				       bgpwatcher_server_callbacks_t *callbacks)
{
  bgpwatcher_server_t *server = NULL;
  assert(callbacks != NULL);

  if((server = malloc_zero(sizeof(bgpwatcher_server_t))) == NULL)
    {
      fprintf(stderr, "ERROR: Could not allocate server structure\n");
      return NULL;
    }

  server->callbacks = callbacks;

  /* init czmq */
  if((server->ctx = zctx_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to create 0MQ context");
      goto err;
    }

  /* set default config */

  if((server->client_uri =
      strdup(BGPWATCHER_CLIENT_URI_DEFAULT)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to duplicate client uri string");
      goto err;
    }

  server->heartbeat_interval = BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;

  server->heartbeat_liveness = BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;

  /* create an empty client list */
  if((server->clients = zlist_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not create client list");
      goto err;
    }

  return server;

 err:
  if(server != NULL)
    {
      bgpwatcher_server_free(server);
    }
  return NULL;
}

int bgpwatcher_server_start(bgpwatcher_server_t *server)
{
  /* bind to client socket */
  if((server->client_socket = zsocket_new(server->ctx, ZMQ_ROUTER)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_START_FAILED,
			     "Failed to create client socket");
      return -1;
    }

  if(zsocket_bind(server->client_socket, "%s", server->client_uri) < 0)
    {
      bgpwatcher_err_set_err(ERR, errno, "Could not bind to client socket");
      return -1;
    }

  /* seed the time for the next heartbeat sent to servers */
  server->heartbeat_next = zclock_time() + server->heartbeat_interval;

  /* start processing requests */
  while((server->shutdown == 0) && (run_server(server) == 0))

    bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_UNHANDLED, "Unhandled error");
  return -1;
}

void bgpwatcher_server_perr(bgpwatcher_server_t *server)
{
  assert(server != NULL);
  bgpwatcher_err_perr(ERR);
}

void bgpwatcher_server_stop(bgpwatcher_server_t *server)
{
  server->shutdown = 1;
}

void bgpwatcher_server_free(bgpwatcher_server_t *server)
{
  assert(server != NULL);

  if(server->client_uri != NULL)
    {
      free(server->client_uri);
      server->client_uri = NULL;
    }

  /* free'd by zctx_destroy */
  server->client_socket = NULL;

  zctx_destroy(&server->ctx);

  free(server);

  return;
}

int bgpwatcher_server_set_client_uri(bgpwatcher_server_t *server,
				     const char *uri)
{
  assert(server != NULL);

  /* remember, we set one by default */
  assert(server->client_uri != NULL);
  free(server->client_uri);

  if((server->client_uri = strdup(uri)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not malloc client uri string");
      return -1;
    }

  return 0;
}

void bgpwatcher_server_set_heartbeat_interval(bgpwatcher_server_t *server,
					      uint64_t interval_ms)
{
  assert(server != NULL);

  server->heartbeat_interval = interval_ms;
}

void bgpwatcher_server_set_heartbeat_liveness(bgpwatcher_server_t *server,
					      int beats)
{
  assert(server != NULL);

  server->heartbeat_liveness = beats;
}
