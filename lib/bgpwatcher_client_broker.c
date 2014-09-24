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

#include "khash.h"
#include "utils.h"

#define ERR (&broker->err)

#define DO_CALLBACK(cbfunc, args...)					\
  do {									\
    if(broker->callbacks.cbfunc != NULL)				\
      {									\
	broker->callbacks.cbfunc(broker->master, args,			\
				 broker->callbacks.user);		\
      }									\
  } while(0)

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
  assert(broker->server_socket != NULL);

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

static int handle_reply(bgpwatcher_client_broker_t *broker, zmsg_t **msg_p)
{
  zmsg_t *msg = *msg_p;
  zframe_t *frame;
  assert(msg != NULL);
  *msg_p = NULL;

  bgpwatcher_client_broker_req_t rx_rep = {0, 0};
  bgpwatcher_client_broker_req_t req;
  int rc;

  khiter_t khiter;

  /* frame 1: seq num */
  if((frame = zmsg_first(msg)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid reply message (missing sequence number)");
      goto err;
    }
  if(zframe_size(frame) != sizeof(seq_num_t))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid message received from master "
			     "(malformed sequence number)");
      return -1;
    }
  memcpy(&rx_rep.seq_num, zframe_data(frame), sizeof(seq_num_t));

  /* frame 3: return code */
  if((frame = zmsg_next(msg)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid reply message (missing return code)");
      goto err;
    }
  if(zframe_size(frame) != sizeof(uint8_t))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid message received from master "
			     "(malformed return code)");
      return -1;
    }
  rc = *zframe_data(frame);
  if(rc != 0)
    {
      rc = -1;
    }

  /* grab the corresponding record from the outstanding req set */
  if((khiter = kh_get(reqset, broker->outstanding_req, rx_rep)) ==
     kh_end(broker->outstanding_req))
    {
      /* err, a reply for a non-existent request? */
      fprintf(stderr, "WARN: No outstanding request info for seq num %"PRIu32,
	      rx_rep.seq_num);
      zmsg_destroy(&msg);
      return 0;
    }

  req = kh_key(broker->outstanding_req, khiter);
  assert(req.seq_num == rx_rep.seq_num);
  kh_del(reqset, broker->outstanding_req, khiter);

  /*fprintf(stderr, "MATCH: req.seq: %"PRIu32", req.msg_type: %d\n",
    req.seq_num, req.msg_type);*/

  DO_CALLBACK(handle_reply, req.seq_num, rc);

  zmsg_destroy(&msg);
  return 0;

 err:
  zmsg_destroy(&msg);
  zframe_destroy(&frame);
  return -1;
}

static int event_loop(bgpwatcher_client_broker_t *broker)
{
  zsock_t *poll_sock;

  zmsg_t *msg = NULL;
  zframe_t *frame;

  bgpwatcher_msg_type_t msg_type;
  uint8_t msg_type_p;
  bgpwatcher_client_broker_req_t req;
  int khret;

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
	      goto err;
	    }
	  server_connect(broker);
	  assert(broker->server_socket != NULL);

	  broker->heartbeat_liveness_remaining = broker->heartbeat_liveness;
	}
      else if(zpoller_terminated(broker->poller) != 0)
	{
	  goto interrupt;
	}
    }

  /* is there an incoming message from the server? */
  if(poll_sock == broker->server_socket)
    {
      if((msg = zmsg_recv(broker->server_socket)) == NULL)
	{
	  goto interrupt;
	}

      msg_type = bgpwatcher_msg_type(msg, 0);

      switch(msg_type)
	{
	case BGPWATCHER_MSG_TYPE_REPLY:
	  broker->heartbeat_liveness_remaining = broker->heartbeat_liveness;

	  /*
	  fprintf(stderr, "DEBUG: Got reply from server\n");
	  zmsg_print(msg);
	  */

	  if(handle_reply(broker, &msg) != 0)
	    {
	      goto err;
	    }

	  if(zctx_interrupted != 0)
	    {
	      goto interrupt;
	    }
	  break;

	case BGPWATCHER_MSG_TYPE_HEARTBEAT:
	  broker->heartbeat_liveness_remaining = broker->heartbeat_liveness;
	  break;

	default:
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				     "Invalid message type received from "
				     "server (%d)", msg_type);
	  goto err;
	}

      broker->reconnect_interval_next =
	broker->reconnect_interval_min;
    }

  /* is there an incoming message from our master? */
  else if(poll_sock == broker->master_pipe)
    {
      if((msg = zmsg_recv(broker->master_pipe)) == NULL)
	{
	  goto interrupt;
	}

      /* peek at the first frame (msg type) */
      if((msg_type =
	  bgpwatcher_msg_type(msg, 1)) != BGPWATCHER_MSG_TYPE_UNKNOWN)
	{
	  req.msg_type = msg_type;

	  /* now we need the seq number */
	  if((frame = zmsg_next(msg)) == NULL)
	    {
	      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				     "Invalid message received from master");
	      goto err;
	    }
	  if(zframe_size(frame) != sizeof(seq_num_t))
	    {
	      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				     "Invalid message received from master "
				     "(malformed sequence number)");
	      goto err;
	    }
	  memcpy(&req.seq_num, zframe_data(frame), sizeof(seq_num_t));

	  /*fprintf(stderr, "DEBUG: tx.seq: %"PRIu32", tx.msg_type: %d\n",
	    req.seq_num, req.msg_type);*/

	  /* add to the req hash */
	  kh_put(reqset, broker->outstanding_req, req, &khret);
	  if(khret == -1)
	    {
	      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
				     "Could not add request to set");
	      goto err;
	    }

	  /* now send on to the server */
	  /** @todo add re-tx here somewhere */
	  if(zmsg_send(&msg, broker->server_socket) != 0)
	    {
	      bgpwatcher_err_set_err(ERR, errno,
				     "Could not pass message to server");
	      goto err;
	    }
	}
      else
	{
	  /* this is a message for us */
	  /** @todo figure out how to wait until our current reqs are ackd */
	  if((frame = zmsg_pop(msg)) == NULL)
	    {
	      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				     "Invalid message received from client");
	      goto err;
	    }

	  if(zframe_streq(frame, "$TERM") == 1)
	    {
	      fprintf(stderr,
		      "INFO: Got $TERM, shutting down client broker on next "
		      "cycle\n");
	      if(broker->shutdown_time == 0)
		{
		  broker->shutdown_time =
		    zclock_time() + broker->shutdown_linger;
		}
	    }

	  zframe_destroy(&frame);
	}
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

  zmsg_destroy(&msg);
  return 0;

 interrupt:
  /* we were interrupted */
  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INTERRUPT, "Caught interrupt");
  return -1;

 err:
  zmsg_destroy(&msg);
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
  while(1)
    {
      /* if we have been asked to shutdown, we wait until we are done with our
	 requests, or until the linger timeout has passed */
      if(broker->shutdown_time > 0 &&
	 ((kh_size(broker->outstanding_req) == 0) ||
	  (broker->shutdown_time <= zclock_time())))
	{
	  break;
	}
      if(event_loop(broker) != 0)
	{
	  break;
	}
    }

  if(server_disconnect(broker) != 0)
    {
      // err will be set
      return;
    }

  /* free our poller */
  zpoller_destroy(&broker->poller);
}
