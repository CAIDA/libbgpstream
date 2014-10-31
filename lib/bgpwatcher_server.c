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

#include <stdint.h>
#include <stdio.h>

#include <bgpwatcher_common_int.h>
#include <bgpwatcher_server.h>

#include "khash.h"
#include "utils.h"

#define ERR (&server->err)

#define VA_ARGS(...) , ##__VA_ARGS__

/* evaluates to true IF there is a callback AND it fails */
#define DO_CALLBACK(cbfunc, args...)					\
  ((server->callbacks != NULL) && (server->callbacks->cbfunc != NULL) && \
   (server->callbacks->cbfunc(server, (&client->info) VA_ARGS(args),    \
			      server->callbacks->user) != 0))

enum {
  POLL_ITEM_CLIENT = 0,
  POLL_ITEM_CNT    = 1,
};

static void client_free(bgpwatcher_server_client_t **client_p)
{
  bgpwatcher_server_client_t *client = *client_p;

  if(client == NULL)
    {
      return;
    }

  zmq_msg_close(&client->identity);

  if(client->id != NULL)
    {
      free(client->id);
      client->id = NULL;
    }

  free(client->pfx_table.collector);
  client->pfx_table.collector = NULL;

  free(client);

  *client_p = NULL;
  return;
}

/* because the hash calls with only the pointer, not the local ref */
static void client_free_wrap(bgpwatcher_server_client_t *client)
{
  client_free(&client);
}

char *msg_strhex(zmq_msg_t *msg)
{
    assert(msg != NULL);

    static const char hex_char [] = "0123456789ABCDEF";

    size_t size = zmq_msg_size(msg);
    byte *data = zmq_msg_data(msg);
    char *hex_str = (char *) malloc (size * 2 + 1);
    if(hex_str == NULL)
      {
	return NULL;
      }

    uint byte_nbr;
    for (byte_nbr = 0; byte_nbr < size; byte_nbr++) {
        hex_str [byte_nbr * 2 + 0] = hex_char [data [byte_nbr] >> 4];
        hex_str [byte_nbr * 2 + 1] = hex_char [data [byte_nbr] & 15];
    }
    hex_str [size * 2] = 0;
    return hex_str;
}

static bgpwatcher_server_client_t *client_init(bgpwatcher_server_t *server,
					       zmq_msg_t *id_msg)
{
  bgpwatcher_server_client_t *client;
  int khret;
  khiter_t khiter;

  if((client = malloc_zero(sizeof(bgpwatcher_server_client_t))) == NULL)
    {
      return NULL;
    }

  if(zmq_msg_init(&client->identity) == -1 ||
     zmq_msg_copy(&client->identity, id_msg) == -1)
    {
      goto err;
    }
  zmq_msg_close(id_msg);

  client->id = msg_strhex(&client->identity);
  client->expiry = zclock_time() +
    (server->heartbeat_interval * server->heartbeat_liveness);

  client->info.name = client->id;

  /* insert client into the hash */
  khiter = kh_put(strclient, server->clients, client->id, &khret);
  if(khret == -1)
    {
      goto err;
    }
  kh_val(server->clients, khiter) = client;

  return client;

 err:
  client_free(&client);
  return NULL;
}

/** @todo consider using something other than the hex id as the key */
static bgpwatcher_server_client_t *client_get(bgpwatcher_server_t *server,
					      zmq_msg_t *id_msg)
{
  bgpwatcher_server_client_t *client;
  khiter_t khiter;
  char *id;

  if((id = msg_strhex(id_msg)) == NULL)
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

	  if(zclock_time() < client->expiry)
	    {
	      break; /* client is alive, we're done here */
	    }

	  fprintf(stderr, "INFO: Removing dead client (%s)\n", client->id);
	  fprintf(stderr, "INFO: Expiry: %"PRIu64" Time: %"PRIu64"\n",
		  client->expiry, zclock_time());
	  if(DO_CALLBACK(client_disconnect) != 0)
	    {
	      return -1;
	    }
	  /* the key string is actually owned by the client, dont free */
	  client_free(&client);
	  kh_del(strclient, server->clients, k);
	}
    }

  return 0;
}

static void clients_free(bgpwatcher_server_t *server)
{
  assert(server != NULL);
  assert(server->clients != NULL);

  kh_free_vals(strclient, server->clients, client_free_wrap);
  kh_destroy(strclient, server->clients);
  server->clients = NULL;
}

static int send_reply(bgpwatcher_server_t *server,
		      bgpwatcher_server_client_t *client,
		      zmq_msg_t *seq_msg)
{
  uint8_t reply_t_p = BGPWATCHER_MSG_TYPE_REPLY;
  zmq_msg_t id_cpy;

#ifdef DEBUG
  fprintf(stderr, "======================================\n");
  fprintf(stderr, "DEBUG: Sending reply\n");
#endif

  /* add the client id */
  /** @todo pass received client id thru, save copying */
  if(zmq_msg_init(&id_cpy) == -1 ||
     zmq_msg_copy(&id_cpy, &client->identity) == -1)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to duplicate client id");
      goto err;
    }
  if(zmq_msg_send(&id_cpy,
		  server->client_socket,
		  ZMQ_SNDMORE) == -1)
    {
      zmq_msg_close(&id_cpy);
      bgpwatcher_err_set_err(ERR, errno,
			     "Failed to send reply client id for %s",
			     client->id);
      goto err;
    }

  /* add the reply type */
  if(zmq_send(server->client_socket, &reply_t_p,
	      bgpwatcher_msg_type_size_t, ZMQ_SNDMORE)
     != bgpwatcher_msg_type_size_t)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to send reply message type");
      goto err;
    }

  /* add the seq num */
  if(zmq_msg_send(seq_msg, server->client_socket, 0) == -1)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not send reply seq frame");
      goto err;
    }

#ifdef DEBUG
  fprintf(stderr, "======================================\n\n");
#endif

  return 0;

 err:
  return -1;
}

static int handle_table_prefix_begin(bgpwatcher_server_t *server,
                                     bgpwatcher_server_client_t *client)
{
  /* deserialize the table into the appropriate structure */
  if(bgpwatcher_pfx_table_begin_recv(server->client_socket,
                                     &client->pfx_table) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                             "Failed to deserialized prefix table");
      goto err;
    }

  /* has this table already been started? */
  if(client->pfx_table_started != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                             "Prefix table already started");
      goto err;
    }

  /* set the table number */
  client->pfx_table.id = server->table_num++;

  /* now the table is started */
  client->pfx_table_started = 1;

  if(DO_CALLBACK(table_begin_prefix,
                 &client->pfx_table) != 0)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

static int handle_table_prefix_end(bgpwatcher_server_t *server,
                                   bgpwatcher_server_client_t *client)
{
  /* deserialize the table into the appropriate structure */
  if(bgpwatcher_pfx_table_end_recv(server->client_socket,
                                   &client->pfx_table) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                             "Failed to receive prefix table");
      goto err;
    }

  /* this table must already be started */
  if(client->pfx_table_started == 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                             "Prefix table not started");
      goto err;
    }

  /* now the table is not started */
  client->pfx_table_started = 0;

  if(DO_CALLBACK(table_end_prefix,
                 &client->pfx_table) != 0)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

static int handle_table(bgpwatcher_server_t *server,
			bgpwatcher_server_client_t *client,
			bgpwatcher_data_msg_type_t type)
{
  uint8_t table_type;

  /* get the table type for this client (always prefix) */
  if(zmq_recv(server->client_socket, &table_type,
	      bgpwatcher_table_type_size_t, 0)
     != bgpwatcher_table_type_size_t)
    {
      goto err;
    }

  if(zsocket_rcvmore(server->client_socket) == 0)
    {
      goto err;
    }

  switch(table_type)
    {
    case BGPWATCHER_TABLE_TYPE_PREFIX:
      switch(type)
        {
        case BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN:
          if(handle_table_prefix_begin(server, client) != 0)
            {
              goto err;
            }
          break;

        case BGPWATCHER_DATA_MSG_TYPE_TABLE_END:
          if(handle_table_prefix_end(server, client) != 0)
            {
              goto err;
            }
          break;
        default:
          bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                             "Invalid prefix table message");
          goto err;
        }
      break;

    default:
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                             "Invalid table type");
      goto err;
      break;
    }

  return 0;

 err:
  return -1;
}

static int handle_pfx_record(bgpwatcher_server_t *server,
			     bgpwatcher_server_client_t *client)
{
  /** @todo test moving into server structure */
  bgpwatcher_pfx_row_t row;

  if(client->pfx_table_started == 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Received prefix before table start");
      goto err;
    }

  if(bgpwatcher_pfx_row_recv(server->client_socket, &row,
                             client->pfx_table.peers_cnt) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not receive prefix record");
      goto err;
    }

  if(DO_CALLBACK(prefix_row,
                 &client->pfx_table,
                 &row) != 0)
    {
      goto err;
    }

  return 0;

err:
  return -1;
}

/*
 * | SEQ NUM       |
 * | DATA MSG TYPE |
 * | Payload       |
 */
static int handle_data_message(bgpwatcher_server_t *server,
			       bgpwatcher_server_client_t *client)
{
  zmq_msg_t seq_msg;
  int rc = -1;
  int hrc;
  bgpwatcher_data_msg_type_t dmt;

  assert(msg != NULL);

  /* grab the seq num and save it for later */
  if(zmq_msg_init(&seq_msg) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not init seq num msg");
      goto err;
    }

  if(zmq_msg_recv(&seq_msg, server->client_socket, 0) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not extract seq number");
      goto err;
    }
  /* just to be safe */
  if(zmq_msg_size(&seq_msg) != sizeof(seq_num_t))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid seq number frame");
      goto err;
    }

  if(zsocket_rcvmore(server->client_socket) == 0)
    {
      goto err;
    }

  /* grab the msg type */
  dmt = bgpwatcher_recv_data_type(server->client_socket);

  /* regardless of what they asked for, let them know that we got the request */
  if(send_reply(server, client, &seq_msg) != 0)
    {
      goto err;
    }

  switch(dmt)
    {
    case BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN:
    case BGPWATCHER_DATA_MSG_TYPE_TABLE_END:
      hrc = handle_table(server, client,dmt);
      if(bgpwatcher_err_is_err(ERR) != 0)
	{
	  goto err;
	}
      rc = hrc;
      break;

    case BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD:
      hrc = handle_pfx_record(server, client);
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

  return rc;

 err:
  zmq_msg_close(&seq_msg);
  return -1;
}

static int handle_ready_message(bgpwatcher_server_t *server,
                                bgpwatcher_server_client_t *client)
{
#ifdef DEBUG
  fprintf(stderr, "DEBUG: Creating new client %s\n", client->id);
#endif

  if(client->info.interests != 0 || client->info.intents != 0)
    {
      fprintf(stderr, "WARN: Client is redefining their interests/intents\n");
    }

  /* first frame is their interests */
  if(zsocket_rcvmore(server->client_socket) == 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Ready message missing interests");
      goto err;
    }
  if(zmq_recv(server->client_socket, &client->info.interests,
	      sizeof(client->info.interests), 0)
     != sizeof(client->info.interests))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not extract client interests");
      goto err;
    }

 /* next is the intents */
  if(zsocket_rcvmore(server->client_socket) == 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Ready message missing intents");
      goto err;
    }
  if(zmq_recv(server->client_socket, &client->info.intents,
	      sizeof(client->info.intents), 0)
     != sizeof(client->info.intents))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not extract client intents");
      goto err;
    }

  /* call the "client connect" callback */
  if(DO_CALLBACK(client_connect) != 0)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

static int handle_message(bgpwatcher_server_t *server,
			  bgpwatcher_server_client_t **client_p,
                          bgpwatcher_msg_type_t msg_type)
{
  assert(client_p != NULL);
  bgpwatcher_server_client_t *client = *client_p;
  assert(client != NULL);

  /* check each type we support (in descending order of frequency) */
  switch(msg_type)
    {
    case BGPWATCHER_MSG_TYPE_DATA:
#ifdef DEBUG
      fprintf(stderr, "**************************************\n");
      fprintf(stderr, "DEBUG: Got data from client:\n");
      fprintf(stderr, "**************************************\n\n");
#endif

      /* parse the request, and then call the appropriate callback */
      if(handle_data_message(server, client) != 0)
	{
	  /* err no will already be set */
	  goto err;
	}

      break;

    case BGPWATCHER_MSG_TYPE_HEARTBEAT:
      /* safe to ignore these */
      break;

    case BGPWATCHER_MSG_TYPE_READY:
      if(handle_ready_message(server, client) != 0)
        {
          goto err;
        }
      break;

    case BGPWATCHER_MSG_TYPE_TERM:
      /* if we get an explicit term, we want to remove the client from our
	 hash, and also fire the appropriate callback */

#ifdef DEBUG
      fprintf(stderr, "**************************************\n");
      fprintf(stderr, "DEBUG: Got disconnect from client:\n");
#endif

      /* call the "client disconnect" callback */
      if(DO_CALLBACK(client_disconnect) != 0)
	{
	  goto err;
	}

      clients_remove(server, client);
      client_free(&client);
      break;

    default:
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid message type (%d) rx'd from client",
			     msg_type);
      goto err;
      break;
    }

  *client_p = NULL;
  return 0;

 err:
  *client_p = NULL;
  return -1;
}

static int run_server(bgpwatcher_server_t *server)
{
  bgpwatcher_msg_type_t msg_type;
  bgpwatcher_server_client_t *client = NULL;
  khiter_t k;

  uint8_t msg_type_p;

  zmq_msg_t client_id;
  zmq_msg_t id_cpy;

  /* get the client id frame */
  if(zmq_msg_init(&client_id) == -1)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to init msg");
      goto err;
    }

  if(zmq_msg_recv(&client_id, server->client_socket, 0) == -1)
    {
      switch(errno)
	{
	case EAGAIN:
	  goto timeout;
	  break;

	case ETERM:
	case EINTR:
	  goto interrupt;
	  break;

	default:
	  bgpwatcher_err_set_err(ERR, errno, "Could not recv from client");
	  goto err;
	  break;
	}
    }

  /* any kind of message from a client means that it is alive */
  /* treat the first frame as an identity frame */

  if(zsocket_rcvmore(server->client_socket) == 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid message received from client "
			     "(missing seq num)");
      goto err;
    }

  /* now grab the message type */
  msg_type = bgpwatcher_recv_type(server->client_socket);

  /* check if this client is already registered */
  if((client = client_get(server, &client_id)) == NULL)
    {
      if(msg_type == BGPWATCHER_MSG_TYPE_READY)
	{
	  /* create state for this client */
	  if((client = client_init(server, &client_id)) == NULL)
	    {
	      goto err;
	    }
	}
      else
	{
	  /* somehow the client state was lost but the client didn't
	     reconnect */
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				 "Unknown client found");
	  goto err;
	}
    }

  /* by here we have a client object and it is time to handle whatever
     message we were sent */
  if(handle_message(server, &client, msg_type) != 0)
    {
      goto err;
    }

 timeout:
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

	  if(zmq_msg_init(&id_cpy) == -1 ||
	     zmq_msg_copy(&id_cpy, &client->identity) == -1)
	    {
	      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
				     "Failed to duplicate client id");
	      goto err;
	    }
	  if(zmq_msg_send(&id_cpy,
			  server->client_socket,
			  ZMQ_SNDMORE) == -1)
	    {
	      zmq_msg_close(&id_cpy);
	      bgpwatcher_err_set_err(ERR, errno,
				     "Could not send client id to client %s",
				     client->id);
	      goto err;
	    }

	  msg_type_p = BGPWATCHER_MSG_TYPE_HEARTBEAT;
	  if(zmq_send(server->client_socket, &msg_type_p,
		      bgpwatcher_msg_type_size_t, 0)
	     != bgpwatcher_msg_type_size_t)
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
  return -1;

 interrupt:
  /* we were interrupted */
  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INTERRUPT, "Caught SIGINT");
  return -1;
}

bgpwatcher_server_t *bgpwatcher_server_init(
				       bgpwatcher_server_callbacks_t **cb_p)
{
  bgpwatcher_server_t *server = NULL;
  assert(cb_p != NULL);

  if((server = malloc_zero(sizeof(bgpwatcher_server_t))) == NULL)
    {
      fprintf(stderr, "ERROR: Could not allocate server structure\n");
      free(*cb_p);
      return NULL;
    }
  
  server->callbacks = *cb_p;
  *cb_p = NULL;

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

  zsocket_set_router_mandatory(server->client_socket, 1);

  zsocket_set_rcvtimeo(server->client_socket, server->heartbeat_interval);

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

  free(server->callbacks);
  server->callbacks = NULL;

  free(server->client_uri);
  server->client_uri = NULL;

  clients_free(server);
  server->clients = NULL;

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
