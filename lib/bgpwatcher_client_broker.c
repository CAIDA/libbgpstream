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

  if(zpoller_add(broker->poller, broker->server_socket) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_START_FAILED,
			     "Could not add server socket to poller");
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

static int event_loop(bgpwatcher_client_broker_t *broker)
{
  zsock_t *poll_sock;

  zmsg_t *msg;
  zframe_t *frame;

  bgpwatcher_msg_type_t msg_type;
  uint8_t msg_type_p;

  /*fprintf(stderr, "DEBUG: Beginning loop cycle\n");*/

  if((poll_sock =
      zpoller_wait(broker->poller, broker->heartbeat_interval)) == NULL)
    {
      /* either we were interrupted, or we timed out */
      if(zpoller_expired(broker->poller) != 0 &&
	 --broker->heartbeat_liveness_remaining == 0)
	{
	  /* the server has been flat-lining for too long, get the paddles! */
	  fprintf(stderr, "WARN: heartbeat failure, can't reach server\n");
	  fprintf(stderr, "WARN: reconnecting in %"PRIu64" msec…\n",
		  broker->reconnect_interval_next);

	  zclock_sleep(broker->reconnect_interval_next);

	  if(broker->reconnect_interval_next < broker->reconnect_interval_max)
	    {
	      broker->reconnect_interval_next *= 2;
	    }

	  zsocket_destroy(broker->ctx, broker->server_socket);
	  if(zpoller_remove(broker->poller, broker->server_socket) != 0)
	    {
	      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_UNHANDLED,
				  "Could not remove server socket from poller");
	      return -1;
	    }
	  server_connect(broker);

	  broker->heartbeat_liveness_remaining = broker->heartbeat_liveness;
	}
      else if(zpoller_terminated(broker->poller) != 0)
	{
	  goto interrupt;
	}
    }

  if(poll_sock == broker->server_socket)
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
  /* check for a message from our master */
  else if(poll_sock == broker->master_pipe)
    {
      if((msg = zmsg_recv(broker->master_pipe)) == NULL)
	{
	  goto interrupt;
	}

      /* peek at the first frame */
      if((frame = zmsg_first(msg)) == NULL)
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				 "Invalid message received from master");
	  return -1;
	}

      /* if the frame is the size of our message types, we pass it on to the
	 server */
      if(zframe_size(frame) == bgpwatcher_msg_type_size_t)
	{
	  /* just pass this along to the server for now */
	  if(zmsg_send(&msg, broker->server_socket) != 0)
	    {
	      bgpwatcher_err_set_err(ERR, errno,
				     "Could not pass message to server");
	      return -1;
	    }
	}
      else
	{
	  /* this is a message for us */
	  /** @todo figure out how to wait until our current reqs are ackd */
	  if(zframe_streq(frame, "$TERM") == 1)
	    {
	      fprintf(stderr,
		      "INFO: Got $TERM, shutting down client broker on next "
		      "cycle\n");
	      broker->shutdown = 1;
	    }
	}
      zmsg_destroy(&msg);
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
   mallocs itself (e.g. poller) */
void bgpwatcher_client_broker_run(zsock_t *pipe, void *args)
{
  bgpwatcher_client_broker_t *broker = (bgpwatcher_client_broker_t*)args;
  assert(broker != NULL);
  assert(pipe != NULL);

  broker->master_pipe = pipe;

  /* init our poller */
  /* server_connect will add the server socket */
  if((broker->poller = zpoller_new(broker->master_pipe, NULL)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not initialize poller");
      return;
    }

  /* connect to the server */
  if(server_connect(broker) != 0)
    {
      return;
    }

  /* seed the time for the next heartbeat sent to the server */
  broker->heartbeat_next = zclock_time() + broker->heartbeat_interval;

  /* signal to our master that we are ready */
  if(zsock_signal(pipe, 0) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not send ready signal to master");
      return;
    }

  /* start processing requests */
  while((broker->shutdown == 0) && (event_loop(broker) == 0))
    {
      /* nothing here */
    }

  /* free our poller */
  zpoller_destroy(&broker->poller);

  if(server_disconnect(broker) != 0)
    {
      // err will be set
      return;
    }
}
