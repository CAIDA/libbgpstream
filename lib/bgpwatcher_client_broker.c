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

#define ERR (&broker->err)

enum {
  POLL_ITEM_SERVER = 0,
  POLL_ITEM_CNT    = 1,
};

static int server_connect(bgpwatcher_client_broker_t *broker)
{
  uint8_t msg_type_p;
  zframe_t *frame;

  /* connect to server socket */
  if((broker->server_socket = zsocket_new(broker->ctx, ZMQ_DEALER)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_START_FAILED,
			     "Failed to create server connection");
      return -1;
    }

  if(broker->identity != NULL && strlen(broker->identity) > 0)
    {
      zsocket_set_identity(broker->server_socket, broker->identity);
    }

  if(zsocket_connect(broker->server_socket, "%s", broker->server_uri) < 0)
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

  if(zframe_send(&frame, broker->server_socket, 0) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not send ready msg to server");
      return -1;
    }

  fprintf(stderr, "DEBUG: broker ready (%d)\n", msg_type_p);

  return 0;
}

static int server_disconnect(bgpwatcher_client_broker_t *broker)
{
  uint8_t msg_type_p = BGPWATCHER_MSG_TYPE_TERM;
  zframe_t *frame;

  fprintf(stderr, "DEBUG: broker sending TERM\n");

  if((frame = zframe_new(&msg_type_p, 1)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not create new client-term frame");
      return -1;
    }

  if(zframe_send(&frame, broker->server_socket, 0) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not send ready msg to server");
      return -1;
    }

  return 0;
}

zmsg_t * append_data_headers(zmsg_t *msg,
			     bgpwatcher_data_msg_type_t type,
			     bgpwatcher_client_broker_t *broker)
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
  if(zmsg_pushmem(msg, &broker->sequence_num,
		  sizeof(uint64_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add sequence number to message");
      return NULL;
    }
  broker->sequence_num++;

  type_b = BGPWATCHER_MSG_TYPE_DATA;
  if(zmsg_pushmem(msg, &type_b, sizeof(uint8_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add request type to message");
      return NULL;
    }

  return msg;
}

zmsg_t *build_test_table_begin(bgpwatcher_client_broker_t *broker,
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
			     broker);
}

zmsg_t *build_test_table_end(bgpwatcher_client_broker_t *broker,
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
			     broker);
}

zmsg_t *build_test_peer(bgpwatcher_client_broker_t *broker)
{
  zmsg_t *msg;
  bgpwatcher_peer_record_t rec;

  ((struct sockaddr_in6*)&rec.ip)->sin6_family = AF_INET6;

  /*2001:48d0:101:501:ec4:7aff:fe12:1108*/
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[0] = 0x20;
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[1] = 0x01;

  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[2] = 0x48;
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[3] = 0xd0;

  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[4] = 0x01;
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[5] = 0x01;

  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[6] = 0x05;
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[7] = 0x01;

  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[8] = 0x0e;
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[9] = 0xc4;

  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[10] = 0x7a;
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[11] = 0xff;

  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[12] = 0xfe;
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[13] = 0x12;

  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[14] = 0x11;
  ((struct sockaddr_in6*)&rec.ip)->sin6_addr.s6_addr[15] = 0x08;

  rec.status = 0xF3;

  if((msg = bgpwatcher_peer_record_serialize(&rec)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize peer record");
      return NULL;
    }

  return append_data_headers(msg,
			     BGPWATCHER_DATA_MSG_TYPE_PEER_RECORD,
			     broker);
}

zmsg_t *build_test_prefix(bgpwatcher_client_broker_t *broker)
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

  return append_data_headers(msg,
			     BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD,
			     broker);
}

/* DEBUG */
static int cnt = 0;

static int event_loop(bgpwatcher_client_broker_t *broker)
{
  /** @todo also poll for messages from our master */
  zmq_pollitem_t poll_items[] = {
    {broker->server_socket, 0, ZMQ_POLLIN, 0}, /* POLL_ITEM_SERVER */
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
		    broker->heartbeat_interval * ZMQ_POLL_MSEC)) == -1)
    {
      goto interrupt;
    }

  /* DEBUG */
  /* fire some requests off to the server for testing */
  if(cnt == 0)
    {
      zmsg_t *req;
      fprintf(stderr, "DEBUG: Sending test messages to server\n");



      req = build_test_table_begin(broker, BGPWATCHER_TABLE_TYPE_PREFIX);
      assert(req != NULL);
      assert(zmsg_send(&req, broker->server_socket) == 0);

      req = build_test_prefix(broker);
      assert(req != NULL);
      assert(zmsg_send(&req, broker->server_socket) == 0);

      req = build_test_table_end(broker, BGPWATCHER_TABLE_TYPE_PREFIX);
      assert(req != NULL);
      assert(zmsg_send(&req, broker->server_socket) == 0);

      req = build_test_table_begin(broker, BGPWATCHER_TABLE_TYPE_PEER);
      assert(req != NULL);
      assert(zmsg_send(&req, broker->server_socket) == 0);

      req = build_test_peer(broker);
      assert(req != NULL);
      assert(zmsg_send(&req, broker->server_socket) == 0);

      req = build_test_table_end(broker, BGPWATCHER_TABLE_TYPE_PEER);
      assert(req != NULL);
      assert(zmsg_send(&req, broker->server_socket) == 0);

      cnt++;
    }
  /* END DEBUG */

  if(poll_items[POLL_ITEM_SERVER].revents & ZMQ_POLLIN)
    {
      /*  Get message
       *  - >3-part: [server.id + empty + content] => reply
       *  - 1-part: HEARTBEAT => heartbeat
       */
      if((msg = zmsg_recv(broker->server_socket)) == NULL)
	{
	  goto interrupt;
	}

      if(zmsg_size(msg) >= 3)
	{
	  fprintf(stderr, "DEBUG: Got reply from server\n");
	  zmsg_print(msg);

	  broker->heartbeat_liveness_remaining = broker->heartbeat_liveness;

	  /* parse the message and figure out what to do with it */
	  /* pass the reply back to our master */

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
	      broker->heartbeat_liveness_remaining = broker->heartbeat_liveness;
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
      broker->reconnect_interval_next =
	broker->reconnect_interval_min;
    }
 else if(--broker->heartbeat_liveness_remaining == 0)
    {
      fprintf(stderr, "WARN: heartbeat failure, can't reach server\n");
      fprintf(stderr, "WARN: reconnecting in %"PRIu64" msec…\n",
	      broker->reconnect_interval_next);

      zclock_sleep(broker->reconnect_interval_next);

      if(broker->reconnect_interval_next < broker->reconnect_interval_max)
	{
	  broker->reconnect_interval_next *= 2;
	}

      zsocket_destroy(broker->ctx, broker->server_socket);
      server_connect(broker);

      broker->heartbeat_liveness_remaining = broker->heartbeat_liveness;
    }

  /* send heartbeat to server if it is time */
  if(zclock_time () > broker->heartbeat_next)
    {
      broker->heartbeat_next = zclock_time() + broker->heartbeat_interval;
      fprintf(stderr, "DEBUG: Sending heartbeat to server\n");

      msg_type_p = BGPWATCHER_MSG_TYPE_HEARTBEAT;
      if((frame = zframe_new(&msg_type_p, 1)) == NULL)
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
				 "Could not create new heartbeat frame");
	  return -1;
	}

      if(zframe_send(&frame, broker->server_socket, 0) == -1)
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

/* ========== PUBLIC FUNCS BELOW HERE ========== */

/* broker owns none of the memory passed to it. only responsible for what it
   mallocs itself */
void bgpwatcher_client_broker_run(zsock_t *pipe, void *args)
{
  bgpwatcher_client_broker_t *broker = (bgpwatcher_client_broker_t*)args;
  assert(broker != NULL);

  /* connect to the server */
  if(server_connect(broker) != 0)
    {
      return;
    }

  /* seed the time for the next heartbeat sent to the server */
  broker->heartbeat_next = zclock_time() + broker->heartbeat_interval;

  /* start processing requests */
  while((broker->shutdown == 0) && (event_loop(broker) == 0))
    {
      /* nothing here */
    }

  if(server_disconnect(broker) != 0)
    {
      // err will be set
      return;
    }
}
