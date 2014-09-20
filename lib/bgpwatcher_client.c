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

#include <bgpwatcher_client_int.h>

#include "utils.h"

#define ERR (&client->err)

enum {
  POLL_ITEM_SERVER = 0,
  POLL_ITEM_CNT    = 1,
};

static int server_connect(bgpwatcher_client_t *client)
{
  uint8_t msg_type_p;
  zframe_t *frame;

  /* connect to server socket */
  if((client->server_socket = zsocket_new(client->ctx, ZMQ_DEALER)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_START_FAILED,
			     "Failed to create server connection");
      return -1;
    }

  if(client->identity != NULL && strlen(client->identity) > 0)
    {
      zsocket_set_identity(client->server_socket, client->identity);
    }

  if(zsocket_connect(client->server_socket, "%s", client->server_uri) < 0)
    {
      bgpwatcher_err_set_err(ERR, errno, "Could not connect to server");
      return -1;
    }

  msg_type_p = BGPWATCHER_MSG_TYPE_READY;
  if((frame = zframe_new(&msg_type_p, 1)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not create new client-ready frame");
      return -1;
    }

  if(zframe_send(&frame, client->server_socket, 0) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not send ready msg to server");
      return -1;
    }

  fprintf(stderr, "DEBUG: client ready (%d)\n", msg_type_p);

  return 0;
}

zmsg_t * append_data_headers(zmsg_t *msg, bgpwatcher_data_msg_type_t type,
			     bgpwatcher_client_t *client)
{
  uint8_t type_b;

  /* now, (working backward), we prepend the request type */
  type_b = type;
  if(zmsg_pushmem(msg, &type_b, sizeof(uint8_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add request type to message");
      return NULL;
    }

  /* now prepend the sequence number */
  if(zmsg_pushmem(msg, &client->sequence_num,
		  sizeof(uint64_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add sequence number to message");
      return NULL;
    }
  client->sequence_num++;

  type_b = BGPWATCHER_MSG_TYPE_DATA;
  if(zmsg_pushmem(msg, &type_b, sizeof(uint8_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add request type to message");
      return NULL;
    }

  return msg;
}

zmsg_t *build_test_table_begin(bgpwatcher_client_t *client,
			       bgpwatcher_table_type_t table_type)
{
  zmsg_t *msg;

  if((msg = zmsg_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to create table begin message");
      return NULL;
    }

  /* append the table type */
  if(zmsg_pushmem(msg, &table_type,
		  sizeof(uint8_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add table type to message");
      return NULL;
    }

  return append_data_headers(msg,
			     BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN,
			     client);
}

zmsg_t *build_test_table_end(bgpwatcher_client_t *client,
			     bgpwatcher_table_type_t table_type)
{
  zmsg_t *msg;

  if((msg = zmsg_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to create table begin message");
      return NULL;
    }

  /* append the table type */
  if(zmsg_pushmem(msg, &table_type,
		  sizeof(uint8_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add table type to message");
      return NULL;
    }

  return append_data_headers(msg,
			     BGPWATCHER_DATA_MSG_TYPE_TABLE_END,
			     client);
}

zmsg_t *build_test_prefix(bgpwatcher_client_t *client)
{
  zmsg_t *msg;

  /* just a toy! */
  bgpwatcher_pfx_record_t rec;
  ((struct sockaddr_in*)&rec.prefix)->sin_family = AF_INET;
  ((struct sockaddr_in*)&rec.prefix)->sin_addr.s_addr = htonl(0xC0ACE200);
  rec.prefix_len = 24;
  ((struct sockaddr_in*)&rec.peer_ip)->sin_family = AF_INET;
  ((struct sockaddr_in*)&rec.peer_ip)->sin_addr.s_addr = htonl(0x82D9FA0D);
  rec.orig_asn = 0x00332211;
  rec.collector_name = "routeviews2";

  if((msg = bgpwatcher_pfx_record_serialize(&rec)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize prefix record");
      return NULL;
    }

  return append_data_headers(msg, BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD, client);
}

/* DEBUG */
static int cnt = 0;

static int run_client(bgpwatcher_client_t *client)
{
  /** @todo also poll for messages from our internal 'client' */
  zmq_pollitem_t poll_items[] = {
    {client->server_socket, 0, ZMQ_POLLIN, 0}, /* POLL_ITEM_SERVER */
  };
  int rc;

  zmsg_t *msg;
  /*zmsg_t *reply;*/
  zframe_t *frame;

  bgpwatcher_msg_type_t msg_type;
  /*uint8_t req_type;*/
  uint8_t msg_type_p;

  /*uint8_t msg_type_reply = TSMQ_MSG_TYPE_REPLY;*/

  /*fprintf(stderr, "DEBUG: Beginning loop cycle\n");*/

  if((rc = zmq_poll(poll_items, POLL_ITEM_CNT,
		    client->heartbeat_interval * ZMQ_POLL_MSEC)) == -1)
    {
      goto interrupt;
    }

  /* DEBUG */
  /* fire some requests off to the server for testing */
  if(cnt == 0)
    {
      zmsg_t *req;
      fprintf(stderr, "DEBUG: Sending test messages to server\n");



      req = build_test_table_begin(client, BGPWATCHER_TABLE_TYPE_PREFIX);
      assert(req != NULL);
      assert(zmsg_send(&req, client->server_socket) == 0);

      req = build_test_prefix(client);
      assert(req != NULL);
      assert(zmsg_send(&req, client->server_socket) == 0);

      req = build_test_table_end(client, BGPWATCHER_TABLE_TYPE_PREFIX);
      assert(req != NULL);
      assert(zmsg_send(&req, client->server_socket) == 0);

      req = build_test_table_begin(client, BGPWATCHER_TABLE_TYPE_PEER);
      assert(req != NULL);
      assert(zmsg_send(&req, client->server_socket) == 0);

      req = build_test_table_end(client, BGPWATCHER_TABLE_TYPE_PEER);
      assert(req != NULL);
      assert(zmsg_send(&req, client->server_socket) == 0);

      cnt++;
    }
  /* END DEBUG */

  if(poll_items[POLL_ITEM_SERVER].revents & ZMQ_POLLIN)
    {
      /*  Get message
       *  - >3-part: [server.id + empty + content] => reply
       *  - 1-part: HEARTBEAT => heartbeat
       */
      if((msg = zmsg_recv(client->server_socket)) == NULL)
	{
	  goto interrupt;
	}

      if(zmsg_size(msg) >= 3)
	{
	  fprintf(stderr, "DEBUG: Got reply from server\n");
	  zmsg_print(msg);

	  client->heartbeat_liveness_remaining = client->heartbeat_liveness;

	  /* parse the message and figure out what to do with it */
	  /* pass the reply back to our internal client */

	  /* for now we just handle REPLY messages */
	  /* just fire the entire message down the tube to our master */

	  zmsg_destroy(&msg);
	  if(zctx_interrupted != 0)
	    {
	      goto interrupt;
	    }
	}
      else if(zmsg_size(msg) == 1)
	{
	  /* When we get a heartbeat message from the server, it means the
	     server was (recently) alive, so we must reset our liveness
	     indicator */
	  msg_type = bgpwatcher_msg_type(msg);
	  if(msg_type == BGPWATCHER_MSG_TYPE_HEARTBEAT)
	    {
	      client->heartbeat_liveness_remaining = client->heartbeat_liveness;
	    }
	  else
	    {
	      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				     "Invalid message type received from "
				     "server (%d)", msg_type);
	      return -1;
	    }
	  zmsg_destroy(&msg);
	}
      else
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				 "Invalid message received from server");
	  return -1;
	}
      client->reconnect_interval_next =
	client->reconnect_interval_min;
    }
 else if(--client->heartbeat_liveness_remaining == 0)
    {
      fprintf(stderr, "WARN: heartbeat failure, can't reach server\n");
      fprintf(stderr, "WARN: reconnecting in %"PRIu64" msec…\n",
	      client->reconnect_interval_next);

      zclock_sleep(client->reconnect_interval_next);

      if(client->reconnect_interval_next < client->reconnect_interval_max)
	{
	  client->reconnect_interval_next *= 2;
	}

      zsocket_destroy(client->ctx, client->server_socket);
      server_connect(client);

      client->heartbeat_liveness_remaining = client->heartbeat_liveness;
    }

  /* send heartbeat to server if it is time */
  if(zclock_time () > client->heartbeat_next)
    {
      client->heartbeat_next = zclock_time() + client->heartbeat_interval;
      fprintf(stderr, "DEBUG: Sending heartbeat to server\n");

      msg_type_p = BGPWATCHER_MSG_TYPE_HEARTBEAT;
      if((frame = zframe_new(&msg_type_p, 1)) == NULL)
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
				 "Could not create new heartbeat frame");
	  return -1;
	}

      if(zframe_send(&frame, client->server_socket, 0) == -1)
	{
	  bgpwatcher_err_set_err(ERR, errno,
				 "Could not send heartbeat msg to server");
	  return -1;
	}
    }

  return 0;

 interrupt:
  /* we were interrupted */
  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INTERRUPT, "Caught interrupt");
  return -1;
}

bgpwatcher_client_t *bgpwatcher_client_init()
{
  bgpwatcher_client_t *client;
  if((client = malloc_zero(sizeof(bgpwatcher_client_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }
  /* now we are ready to set errors... */

  /* init czmq */
  if((client->ctx = zctx_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to create 0MQ context");
      goto err;
    }

  if((client->server_uri =
      strdup(BGPWATCHER_CLIENT_SERVER_URI_DEFAULT)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to duplicate server uri string");
      goto err;
    }

  client->heartbeat_interval = BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;

  client->heartbeat_liveness_remaining = client->heartbeat_liveness =
    BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;

  client->reconnect_interval_next = client->reconnect_interval_min =
    BGPWATCHER_RECONNECT_INTERVAL_MIN;

  client->reconnect_interval_max = BGPWATCHER_RECONNECT_INTERVAL_MAX;

  return client;

 err:
  if(client != NULL)
    {
      bgpwatcher_client_free(client);
    }
  return NULL;
}

int bgpwatcher_client_start(bgpwatcher_client_t *client)
{
  /** @todo fork this part of the client into a new thread and send messages
      over an internal req/rep socket */

  /* connect to the server */
  if(server_connect(client) != 0)
    {
      return -1;
    }

  /* seed the time for the next heartbeat sent to the server */
  client->heartbeat_next = zclock_time() + client->heartbeat_interval;

  /* start processing requests */
  while((client->shutdown == 0) && (run_client(client) == 0))
    {
      /* nothing here */
    }

  return -1;
}

void bgpwatcher_client_perr(bgpwatcher_client_t *client)
{
  assert(client != NULL);
  bgpwatcher_err_perr(ERR);
}

bgpwatcher_client_pfx_table_t *bgpwatcher_client_pfx_table_create(
						   bgpwatcher_client_t *client)
{
  return NULL;
}

void bgpwatcher_client_pfx_table_set_time(bgpwatcher_client_pfx_table_t *table,
					  uint32_t time)
{
  return;
}

int bgpwatcher_client_pfx_table_add(bgpwatcher_client_pfx_table_t *table,
				    bgpwatcher_pfx_record_t *pfx)
{
  return -1;
}

int bgpwatcher_client_pfx_table_flush(bgpwatcher_client_pfx_table_t *table)
{
  return -1;
}

bgpwatcher_client_peer_table_t *bgpwatcher_client_peer_table_create(
						   bgpwatcher_client_t *client)
{
  return NULL;
}

void bgpwatcher_client_peer_table_set_time(bgpwatcher_client_peer_table_t *table,
					   uint32_t time)
{
  return;
}

int bgpwatcher_client_peer_table_add(bgpwatcher_client_peer_table_t *table,
				     bgpwatcher_peer_record_t *peer)
{
  return -1;
}

int bgpwatcher_client_peer_table_flush(bgpwatcher_client_peer_table_t *table)
{
  return -1;
}

void bgpwatcher_client_stop(bgpwatcher_client_t *client)
{
  assert(client != NULL);
  client->shutdown = 1;

  /** @todo or join on the client thread here */
}

void bgpwatcher_client_free(bgpwatcher_client_t *client)
{
  assert(client != NULL);

  /** @todo join on the other thread here */

  if(client->server_uri != NULL)
    {
      free(client->server_uri);
      client->server_uri = NULL;
    }

  /* free'd by zctx_destroy */
  client->server_socket = NULL;

  /* frees our sockets */
  zctx_destroy(&client->ctx);

  free(client);

  return;
}

int bgpwatcher_client_set_server_uri(bgpwatcher_client_t *client,
				     const char *uri)
{
  assert(client != NULL);

  free(client->server_uri);

  if((client->server_uri = strdup(uri)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not set server uri");
      return -1;
    }

  return 0;
}

void bgpwatcher_client_set_heartbeat_interval(bgpwatcher_client_t *client,
					      uint64_t interval_ms)
{
  assert(client != NULL);

  client->heartbeat_interval = interval_ms;
}

void bgpwatcher_client_set_heartbeat_liveness(bgpwatcher_client_t *client,
					      int beats)
{
  assert(client != NULL);

  client->heartbeat_liveness = beats;
}

void bgpwatcher_client_set_reconnect_interval_min(bgpwatcher_client_t *client,
						  uint64_t reconnect_interval_min)
{
  assert(client != NULL);

  client->reconnect_interval_min = reconnect_interval_min;
}

void bgpwatcher_client_set_reconnect_interval_max(bgpwatcher_client_t *client,
						  uint64_t reconnect_interval_max)
{
  assert(client != NULL);

  client->reconnect_interval_max = reconnect_interval_max;
}

int bgpwatcher_client_set_identity(bgpwatcher_client_t *client,
				   const char *identity)
{
  assert(client != NULL);

  free(client->identity);

  if((client->identity = strdup(identity)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not set client identity");
      return -1;
    }

  return 0;
}
