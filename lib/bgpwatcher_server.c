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

enum {
  POLL_ITEM_CLIENT = 0,
  POLL_ITEM_CNT    = 1,
};

typedef struct client {
  /* Identity frame that the client sent us */
  zframe_t *identity;

  /** Printable ID of client (for debugging and logging) */
  char *id;

  /** Time at which the client expires */
  uint64_t expiry;
} client_t;

static client_t *client_init(bgpwatcher_server_t *server, zframe_t *identity)
{
  client_t *client;

  if((client = malloc(sizeof(client_t))) == NULL)
    {
      return NULL;
    }

  client->identity = identity;
  client->id = zframe_strhex(identity);
  client->expiry = zclock_time() +
    (server->heartbeat_interval * server->heartbeat_liveness);

  return client;
}

static void client_free(client_t *client)
{
  if(client == NULL)
    {
      return;
    }

  if(client->identity != NULL)
    {
      zframe_destroy(&client->identity);
    }

  if(client->id != NULL)
    {
      free(client->id);
      client->id = NULL;
    }

  free(client);

  return;
}

/* figure out what client_next should really do since we won't be
   round-robin-ing things to clients */

static int client_ready(zlist_t *clients, client_t *client)
{
  client_t *s;

  /* first we have to see if we already have this client in the list */
  s = zlist_first(clients);
  while(s != NULL)
    {
      if(strcmp(client->id, s->id) == 0)
	{
	  fprintf(stderr, "DEBUG: Replacing existing client (%s)\n", s->id);
	  zlist_remove(clients, s);
	  client_free(s);
	  break;
	}

      s = zlist_next(clients);
    }

  /* now we add this nice shiny new client to the list */
  return zlist_append(clients, client);
}

static void clients_purge(bgpwatcher_server_t *server)
{
    client_t *client = zlist_first(server->clients);

    while(client != NULL)
      {
        if(zclock_time () < client->expiry)
	  {
	    break; /* client is alive, we're done here */
	  }

	fprintf(stderr, "DEBUG: Removing dead client (%s)\n", client->id);
	/** @todo call the client_disconnect callback on purge */
        zlist_remove(server->clients, client);
	client_free(client);
        client = zlist_first(server->clients);
    }
}

static void clients_free(bgpwatcher_server_t *server)
{
  client_t *client = zlist_first(server->clients);

    while(client != NULL)
      {
        zlist_remove(server->clients, client);
	client_free(client);
        client = zlist_first(server->clients);
      }
    zlist_destroy(&server->clients);
}

static int run_server(bgpwatcher_server_t *server)
{
  zmq_pollitem_t poll_items [] = {
    {server->client_socket, 0, ZMQ_POLLIN, 0}, /* POLL_ITEM_CLIENT */
  };
  int rc;

  zmsg_t *msg = NULL;
  zframe_t *frame = NULL;

  client_t *client = NULL;

  bgpwatcher_msg_type_t msg_type;
  uint8_t msg_type_p;

  fprintf(stderr, "DEBUG: Beginning loop cycle\n");

  /* poll for messages from clients */
  if((rc = zmq_poll(poll_items, POLL_ITEM_CNT,
		    server->heartbeat_interval * ZMQ_POLL_MSEC)) == -1)
    {
      goto interrupt;
    }

  /* handle message from a client */
  if(poll_items[POLL_ITEM_CLIENT].revents & ZMQ_POLLIN)
    {
      if((msg = zmsg_recv(server->client_socket)) == NULL)
	{
	  goto interrupt;
	}

      /* any kind of message from a client means that it is alive */
      /* treat the first frame as an identity frame */
      if((frame = zmsg_pop(msg)) == NULL)
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				 "Could not parse response from client");
	  goto err;
	}

      /* create state for this client */
      if((client = client_init(server, frame)) == NULL)
	{
	  goto err;
	}
      /* frame is owned by client object */

      /* add it to the queue of connected clients */
      client_ready(server->clients, client);
      /* client is owned by server->clients */

      /* now we validate the actual message and pass along the info to the
	 appropriate callback */
      msg_type = bgpwatcher_msg_type(msg);

      if(msg_type == BGPWATCHER_MSG_TYPE_READY)
	{
	  fprintf(stderr, "DEBUG: Adding new client (%s)\n", client->id);
	  /* ignore these as we already did the work */
	  zmsg_destroy(&msg);
	}
      else if(msg_type == BGPWATCHER_MSG_TYPE_HEARTBEAT)
	{
	  fprintf(stderr, "DEBUG: Got a heartbeat from %s\n", client->id);
	  /* ignore these */
	  zmsg_destroy(&msg);
	}
      else if(msg_type == BGPWATCHER_MSG_TYPE_REQUEST)
	{
	  /* DEBUG */
	  fprintf(stderr, "DEBUG: Got request from client:\n");
	  zmsg_print(msg);
	  zmsg_destroy(&msg);

	  /* parse the request, and then call the appropriate callback */
	  /* send a reply back to the client based on the callback result */

#if 0
	  /* there must be at least two frames for a valid reply:
	     1. client address 2. empty (3. reply body) */
	  if(zmsg_size(msg) < 2)
	    {
	      tsmq_set_err(broker->tsmq, TSMQ_ERR_PROTOCOL,
			   "Malformed reply received from server");
	      goto err;
	    }

	  /** @todo can we do more error checking here? */
	  /* pass this message along to the client */
	  if(zmsg_send(&msg, broker->client_socket) == -1)
	    {
	      tsmq_set_err(broker->tsmq, errno,
			   "Could not forward server reply to client");
	      goto err;
	    }
#endif

	  /* msg is destroyed by zmsg_send */
	}
      else
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				 "Invalid message type (%d) rx'd from client",
				 msg_type);
	  goto err;
	}

    }

  /* time for heartbeats */
  assert(server->heartbeat_next > 0);
  if(zclock_time() >= server->heartbeat_next)
    {
      client = zlist_first(server->clients);

      while(client != NULL)
	{
	  if(zframe_send(&client->identity, server->client_socket,
			 ZFRAME_REUSE | ZFRAME_MORE) == -1)
	    {
	      bgpwatcher_err_set_err(ERR, errno,
				     "Could not send heartbeat id to client %s",
				     client->id);
	      goto err;
	    }

	  msg_type_p = BGPWATCHER_MSG_TYPE_HEARTBEAT;
	  if((frame = zframe_new(&msg_type_p, 1)) == NULL)
	    {
	      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
				     "Could not create new heartbeat frame");
	      goto err;
	    }

	  if(zframe_send(&frame, server->client_socket, 0) == -1)
	    {
	      bgpwatcher_err_set_err(ERR, errno,
				     "Could not send heartbeat msg to client %s",
				     client->id);
	      goto err;
	    }

	  client = zlist_next(server->clients);
	}
      server->heartbeat_next = zclock_time() + server->heartbeat_interval;
    }
  clients_purge(server);

  return 0;

 err:
  /* try and clean up everything */
  zframe_destroy(&frame);
  zmsg_destroy(&msg);
  return -1;

 interrupt:
  /* we were interrupted */
  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INTERRUPT, "Caught SIGINT");
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
  assert(server != NULL);
  server->shutdown = 1;
}

void bgpwatcher_server_free(bgpwatcher_server_t *server)
{
  assert(server != NULL);

  if(server->callbacks != NULL)
    {
      free(server->callbacks);
      server->callbacks = NULL;
    }

  if(server->client_uri != NULL)
    {
      free(server->client_uri);
      server->client_uri = NULL;
    }

  if(server->clients != NULL)
    {
      clients_free(server);
      server->clients = NULL;
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
