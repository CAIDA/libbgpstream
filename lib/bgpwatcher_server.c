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

#include "khash.h"
#include "utils.h"

#define ERR (&server->err)

/* evaluates to true IF there is a callback AND it fails */
#define DO_CALLBACK(cbfunc, args...)					\
  ((server->callbacks->cbfunc != NULL) &&				\
  (server->callbacks->cbfunc(server, args,				\
			     server->callbacks->user) != 0))

enum {
  POLL_ITEM_CLIENT = 0,
  POLL_ITEM_CNT    = 1,
};

static void client_free(bgpwatcher_server_client_t *client)
{
  if(client == NULL)
    {
      return;
    }

  zframe_destroy(&client->identity);

  if(client->id != NULL)
    {
      free(client->id);
      client->id = NULL;
    }

  free(client);

  return;
}

static bgpwatcher_server_client_t *client_init(bgpwatcher_server_t *server,
					       zframe_t *identity)
{
  bgpwatcher_server_client_t *client;
  int khret;
  khiter_t khiter;

  if((client = malloc_zero(sizeof(bgpwatcher_server_client_t))) == NULL)
    {
      return NULL;
    }

  client->identity = identity;

  client->id = zframe_strhex(identity);
  client->expiry = zclock_time() +
    (server->heartbeat_interval * server->heartbeat_liveness);

  client->info.name = client->id;

  /* insert client into the hash */
  khiter = kh_put(strclient, server->clients, client->id, &khret);
  if(khret == -1)
    {
      client_free(client);
      return NULL;
    }
  kh_val(server->clients, khiter) = client;

  return client;
}

/** @todo consider using something other than the hex id as the key */
static bgpwatcher_server_client_t *client_get(bgpwatcher_server_t *server,
					      zframe_t *identity)
{
  bgpwatcher_server_client_t *client;
  khiter_t khiter;
  char *id;

  if((id = zframe_strhex(identity)) == NULL)
    {
      return NULL;
    }

  if((khiter =
      kh_get(strclient, server->clients, id)) == kh_end(server->clients))
    {
      free(id);
      return NULL;
    }

  client = kh_val(server->clients, khiter);
  /* we are already tracking this client, treat the msg as a heartbeat */
  /* touch the timeout */
  client->expiry = zclock_time() +
    (server->heartbeat_interval * server->heartbeat_liveness);
  free(id);
  return client;
}

static void clients_remove(bgpwatcher_server_t *server,
			   bgpwatcher_server_client_t *client)
{
  khiter_t khiter;
  if((khiter =
      kh_get(strclient, server->clients, client->id)) == kh_end(server->clients))
    {
      /* already removed? */
      fprintf(stderr, "WARN: Removing non-existent client\n");
      return;
    }

  kh_del(strclient, server->clients, khiter);
}

static int clients_purge(bgpwatcher_server_t *server)
{
  khiter_t k;
  bgpwatcher_server_client_t *client;

  for(k = kh_begin(server->clients); k != kh_end(server->clients); ++k)
    {
      if(kh_exist(server->clients, k) != 0)
	{
	  client = kh_val(server->clients, k);

	  if(zclock_time () < client->expiry)
	    {
	      break; /* client is alive, we're done here */
	    }

	  fprintf(stderr, "DEBUG: Removing dead client (%s)\n", client->id);
	  if(DO_CALLBACK(client_disconnect, (&client->info)) != 0)
	    {
	      return -1;
	    }
	  /* the key string is actually owned by the client, dont free */
	  client_free(client);
	  client = NULL;
	  kh_del(strclient, server->clients, k);
	}
    }

  return 0;
}

static void clients_free(bgpwatcher_server_t *server)
{
  assert(server != NULL);
  assert(server->clients != NULL);

  kh_free_vals(strclient, server->clients, client_free);
  kh_destroy(strclient, server->clients);
  server->clients = NULL;
}

static int send_reply(bgpwatcher_server_t *server,
		      bgpwatcher_server_client_t *client,
		      zframe_t *seq_frame,
		      uint8_t rc)
{
  uint8_t reply_t_p = BGPWATCHER_MSG_TYPE_REPLY;
  zmsg_t *msg;

  fprintf(stderr, "======================================\n");
  fprintf(stderr, "DEBUG: Sending reply\n");

  if((msg = zmsg_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to malloc reply message");
      return -1;
    }

  /* add the client id */
  if(zframe_send(&client->identity, server->client_socket,
		 ZFRAME_REUSE | ZFRAME_MORE) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Failed to add client id to reply message",
			     client->id);
      zmsg_destroy(&msg);
      return -1;
    }

  /* add the reply type */
  if(zmsg_addmem(msg, &reply_t_p, sizeof(uint8_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to add type to reply message");
      zmsg_destroy(&msg);
      return -1;
    }

  /* add the seq num */
  if(zmsg_append(msg, &seq_frame) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add seq frame to reply message");
      zmsg_destroy(&msg);
      return -1;
    }

  /* add the return code */
  if(zmsg_addmem(msg, &rc, sizeof(uint8_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to add rc to reply message");
      zmsg_destroy(&msg);
      return -1;
    }

  /* DEBUG */
  zmsg_print(msg);
  fprintf(stderr, "======================================\n\n");

  if(zmsg_send(&msg, server->client_socket) != 0)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not send reply to client");
      zmsg_destroy(&msg);
      return -1;
    }

  return 0;
}

static int handle_table(bgpwatcher_server_t *server,
			bgpwatcher_server_client_t *client,
			zmsg_t *msg,
			bgpwatcher_data_msg_type_t type)
{
  zframe_t *frame;
  uint8_t tmp;

  /* set the table type and table number for this client */
  if((frame = zmsg_pop(msg)) == NULL || zframe_size(frame) != sizeof(uint8_t))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not extract table type");
      goto err;
    }
  tmp = *zframe_data(frame);
  zframe_destroy(&frame);

  if(tmp == BGPWATCHER_TABLE_TYPE_NONE || tmp > BGPWATCHER_TABLE_TYPE_MAX)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid table type");
      goto err;
    }


  if(type == BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN)
    {
      client->table_type = tmp;
      client->table_num = server->table_num++;

      if(DO_CALLBACK(table_begin, client->table_num, client->table_type) != 0)
	{
	  return -1;
	}
    }
  else if(type == BGPWATCHER_DATA_MSG_TYPE_TABLE_END)
    {
      /* make sure they are talking about the same table */
      /* @todo consider allowing both table types simultaneously */
      if(tmp != client->table_type)
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				 "Table type mismatch (expecting %d, got %d)",
				 client->table_type, tmp);
	  goto err;
	}

      if(DO_CALLBACK(table_end, client->table_num, client->table_type) != 0)
	{
	  return -1;
	}

      client->table_type = BGPWATCHER_TABLE_TYPE_NONE;
    }
  else
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid handle_table message type",
			     type);
    }

  return 0;

 err:
  return -1;
}

static int handle_pfx_record(bgpwatcher_server_t *server,
			     bgpwatcher_server_client_t *client,
			     zmsg_t *msg)
{
  bgpwatcher_pfx_record_t *rec = NULL;

  if(client->table_type != BGPWATCHER_TABLE_TYPE_PREFIX) /*DEBUG*/
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Received prefix before table start");
      goto err;
    }

  if((rec = bgpwatcher_pfx_record_deserialize(msg)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not deserialize prefix record");
      goto err;
    }

  if(DO_CALLBACK(recv_pfx_record, client->table_num, rec) != 0)
    {
      bgpwatcher_pfx_record_free(&rec);
      return -1;
    }

  bgpwatcher_pfx_record_free(&rec);
  return 0;

err:
  bgpwatcher_pfx_record_free(&rec);
  return -1;
}

/* guaranteed to get a well-structured data message.
 * must check for valid data-message type and payload.
 * OWNS MSG.
 * will send reply to client
 *
 * | SEQ NUM       |
 * | DATA MSG TYPE |
 * | Payload       |
 */
static int handle_data_message(bgpwatcher_server_t *server,
			       bgpwatcher_server_client_t *client,
			       zmsg_t *msg)
{
  zframe_t *seq_frame = NULL;
  int rc = -1;
  int hrc;
  bgpwatcher_data_msg_type_t dmt;

  /* grab the seq num and save it for later */
  if((seq_frame = zmsg_pop(msg)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not extract seq number");
      goto err;
    }
  /* just to be safe */
  if(zframe_size(seq_frame) != sizeof(uint64_t))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid seq number frame");
      goto err;
    }

  /* grab the msg type */
  dmt = bgpwatcher_data_msg_type(msg);

  switch(dmt)
    {
    case BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN:
    case BGPWATCHER_DATA_MSG_TYPE_TABLE_END:
      hrc = handle_table(server, client, msg,
			 dmt);
      if(bgpwatcher_err_is_err(ERR) != 0)
	{
	  goto err;
	}
      rc = hrc;
      break;

    case BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD:
      hrc = handle_pfx_record(server, client, msg);
      if(bgpwatcher_err_is_err(ERR) != 0)
	{
	  goto err;
	}
      rc = hrc;
      break;

    case BGPWATCHER_DATA_MSG_TYPE_UNKNOWN:
    default:
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid data msg type");
      goto err;
      break;
    }

  zmsg_destroy(&msg);
  return send_reply(server, client, seq_frame, rc);

 err:
  /* err means a broken request, just pretend we didn't get it */
  zframe_destroy(&seq_frame);
  zmsg_destroy(&msg);
  return -1;
}

/* OWNS MSG */
static int handle_message(bgpwatcher_server_t *server,
			  bgpwatcher_server_client_t *client,
			  zmsg_t *msg)
{
  bgpwatcher_msg_type_t msg_type;

  /* validate the message type and pass to the appropriate callback */
  msg_type = bgpwatcher_msg_type(msg);

  /* check each type we support (in descending order of frequency) */
  switch(msg_type)
    {
    case BGPWATCHER_MSG_TYPE_DATA:
      /* DEBUG */
      fprintf(stderr, "**************************************\n");
      fprintf(stderr, "DEBUG: Got data from client:\n");
      zmsg_print(msg);
      fprintf(stderr, "**************************************\n\n");

      /* parse the request, and then call the appropriate callback */
      /* send a reply back to the client based on the callback result */

      /* there must be at least two frames for a valid data msg:
	 1. seq number 2. data_msg_type (3. msg payload) */
      if(zmsg_size(msg) < 2)
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				 "Malformed data message received from "
				 "client");
	  goto err;
	}

      if(handle_data_message(server, client, msg) != 0)
	{
	  /* err no will already be set */
	  msg = NULL;
	  goto err;
	}

      fprintf(stderr, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");

      msg = NULL;
      /* msg was destroyed by handle_data_message */
      break;

    case BGPWATCHER_MSG_TYPE_HEARTBEAT:
      /* safe to ignore these */
      /*fprintf(stderr, "DEBUG: Got a heartbeat from %s\n", client->id);*/
    case BGPWATCHER_MSG_TYPE_READY:
      /* and these */
      break;

    case BGPWATCHER_MSG_TYPE_TERM:
      /* if we get an explicit term, we want to remove the client from our
	 hash, and also fire the appropriate callback */

      /* DEBUG */
      fprintf(stderr, "**************************************\n");
      fprintf(stderr, "DEBUG: Got disconnect from client:\n");

      /* call the "client disconnect" callback */
      if(DO_CALLBACK(client_disconnect, &client->info) != 0)
	{
	  goto err;
	}

      clients_remove(server, client);
      client_free(client);
      client = NULL;
      break;

    default:
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid message type (%d) rx'd from client",
			     msg_type);
      goto err;
      break;
    }

  zmsg_destroy(&msg);
  return 0;

 err:
  zmsg_destroy(&msg);
  return -1;
}

static int run_server(bgpwatcher_server_t *server)
{
  zmq_pollitem_t poll_items [] = {
    {server->client_socket, 0, ZMQ_POLLIN, 0}, /* POLL_ITEM_CLIENT */
  };
  int rc;

  zmsg_t *msg = NULL;
  zframe_t *frame = NULL;

  bgpwatcher_server_client_t *client = NULL;
  khiter_t k;

  uint8_t msg_type_p;

  int new_client = 0;

  /*fprintf(stderr, "DEBUG: Beginning loop cycle\n");*/

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

      /* get the client that this corresponds to */
      if((client = client_get(server, frame)) == NULL)
	{
	  /* create state for this client */
	  if((client = client_init(server, frame)) == NULL)
	    {
	      goto err;
	    }

	  fprintf(stderr, "DEBUG: Creating new client %s\n", client->id);

	  /* call the "client connect" callback */
	  if(DO_CALLBACK(client_connect, &client->info) != 0)
	    {
	      goto err;
	    }

	  frame = NULL;
	}
      else
	{
	  new_client = 0;
	  zframe_destroy(&frame);
	}

      /* by here we have a client object and it is time to handle whatever
	 message we were sent */
      if(handle_message(server, client, msg) != 0)
	{
	  goto err;
	}
      /* handle message destroyed the message, client too, maybe */
      client = NULL;
      msg = NULL;

    }

  /* time for heartbeats */
  assert(server->heartbeat_next > 0);
  if(zclock_time() >= server->heartbeat_next)
    {
      for(k = kh_begin(server->clients); k != kh_end(server->clients); ++k)
	{
	  if(kh_exist(server->clients, k) == 0)
	    {
	      continue;
	    }

	  client = kh_val(server->clients, k);

	  if(zframe_send(&client->identity, server->client_socket,
			 ZFRAME_REUSE | ZFRAME_MORE) == -1)
	    {
	      bgpwatcher_err_set_err(ERR, errno,
				     "Could not send client id to client %s",
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
	}
      server->heartbeat_next = zclock_time() + server->heartbeat_interval;
    }

  return clients_purge(server);

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
  if((server->clients = kh_init(strclient)) == NULL)
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
    {
      /* nothing here */
    }

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
