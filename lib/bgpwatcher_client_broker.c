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

static int handle_server_msg(zloop_t *loop, zsock_t *reader, void *arg);
static int handle_master_msg(zloop_t *loop, zsock_t *reader, void *arg);

#define ERR (&broker->cfg->err)
#define CFG (broker->cfg)

#define DO_CALLBACK(cbfunc, args...)					\
  do {									\
    if(CFG->callbacks.cbfunc != NULL)                                   \
      {									\
	CFG->callbacks.cbfunc(CFG->master, args,			\
                              CFG->callbacks.user);                  \
      }									\
  } while(0)

#define ISERR                                  \
  if(errno == EINTR || errno == ETERM)          \
    {                                           \
      goto interrupt;                           \
    }                                           \
  else

static void reset_heartbeat_timer(bgpwatcher_client_broker_t *broker)
{
  broker->heartbeat_next = zclock_time() + CFG->heartbeat_interval;
}

static void reset_heartbeat_liveness(bgpwatcher_client_broker_t *broker)
{
  broker->heartbeat_liveness_remaining = CFG->heartbeat_liveness;
}

static int server_connect(bgpwatcher_client_broker_t *broker)
{
  uint8_t msg_type_p;
  zframe_t *frame;

  /* connect to server socket */
  if((broker->server_socket = zsocket_new(CFG->ctx, ZMQ_DEALER)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_START_FAILED,
			     "Failed to create server connection");
      return -1;
    }

  /* up the hwm */
  zsocket_set_sndhwm(broker->server_socket, MAX_OUTSTANDING_REQ*2);
  zsocket_set_rcvhwm(broker->server_socket, MAX_OUTSTANDING_REQ*2);

  if(CFG->identity != NULL && strlen(CFG->identity) > 0)
    {
      zsock_set_identity(broker->server_socket, CFG->identity);
    }
  else
    {
      CFG->identity = zsock_identity(broker->server_socket);
    }

  if(zsocket_connect(broker->server_socket, "%s", CFG->server_uri) < 0)
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

  if(zframe_send(&frame, broker->server_socket, ZFRAME_MORE) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not send ready msg to server");
      return -1;
    }

  /* send our interests */
  if((frame = zframe_new(&CFG->interests, 1)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not create new client interests frame");
      return -1;
    }
  if(zframe_send(&frame, broker->server_socket, ZFRAME_MORE) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not send ready msg to server");
      return -1;
    }

  /* send our interests */
  if((frame = zframe_new(&CFG->intents, 1)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not create new client intents frame");
      return -1;
    }
  if(zframe_send(&frame, broker->server_socket, 0) == -1)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not send ready msg to server");
      return -1;
    }

  /* reset the time for the next heartbeat sent to the server */
  reset_heartbeat_timer(broker);

  /* create a new reader for this server socket */
  if(zloop_reader(broker->loop, broker->server_socket,
                  handle_server_msg, broker) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add server socket to reactor");
      return -1;
    }

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

static int handle_reply(bgpwatcher_client_broker_t *broker)
{
  seq_num_t seq_num;
  bgpwatcher_client_broker_req_t *req;

  /* there must be more frames for us */
  if(zsocket_rcvmore(broker->server_socket) == 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Invalid message received from server "
			     "(missing seq num)");
      goto err;
    }

  if(zmq_recv(broker->server_socket, &seq_num, sizeof(seq_num_t), 0)
     != sizeof(seq_num_t))
        {
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
				 "Invalid message received from server "
				 "(malformed sequence number)");
        }

  /* find the corresponding record from the outstanding req set */

  /* most likely, it will be at the head of the list */
  /* so, first we just blindly pop the head, and if it happens not to match, we
     reinsert and search */
  req = zlist_pop(broker->req_list);
  if(req == NULL)
    {
      fprintf(stderr,
	      "WARN: No outstanding request info for seq num %"PRIu32"\n",
	      seq_num);
      return 0;
    }
  /* slow path... */
  if(req->seq_num != seq_num)
    {
      /* need to search */
      fprintf(stderr, "WARN: Searching for request %d\n", seq_num);
      if(zlist_push(broker->req_list, req) != 0)
	{
	  goto err;
	}
      req = zlist_first(broker->req_list);
      while(req != NULL && req->seq_num != seq_num)
	{
	  req = zlist_next(broker->req_list);
	}
      if(req == NULL)
	{
	  fprintf(stderr,
		  "WARN: No outstanding request info for seq num %"PRIu32"\n",
		  seq_num);
	  return 0;
	}
      /* remove from list */
      zlist_remove(broker->req_list, req);
    }

  /*fprintf(stderr, "MATCH: req.seq: %"PRIu32", req.msg_type: %d\n",
    req->seq_num, req->msg_type);*/

  DO_CALLBACK(handle_reply, req->seq_num);

  /* destroy this req, we're done with it */
  bgpwatcher_client_broker_req_free(&req);

  return 0;

 err:
  return -1;
}

static int send_request(bgpwatcher_client_broker_t *broker,
			bgpwatcher_client_broker_req_t *req)
{
  int i = 1;
  int llm_cnt = zlist_size(req->msg_frames);
  zmq_msg_t *llm = zlist_first(req->msg_frames);
  zmq_msg_t llm_cpy;
  int mask;

  req->retry_at = zclock_time() + CFG->request_timeout;

  /* send the type */
  if(zmq_send(broker->server_socket, &req->msg_type,
              bgpwatcher_msg_type_size_t, ZMQ_SNDMORE)
     != bgpwatcher_msg_type_size_t)
    {
      return -1;
    }

  /* send the seq num */
  if(zmq_send(broker->server_socket,
              &req->seq_num, sizeof(seq_num_t),
              ZMQ_SNDMORE)
     != sizeof(seq_num_t))
    {
      return -1;
    }

  while(llm != NULL)
    {
      mask = (i < llm_cnt) ? ZMQ_SNDMORE : 0;
      zmq_msg_init(&llm_cpy);
      if(zmq_msg_copy(&llm_cpy, llm) == -1)
        {
	  bgpwatcher_err_set_err(ERR, errno,
				 "Could not copy message");
	  return -1;
        }
      if(zmq_sendmsg(broker->server_socket, &llm_cpy, mask) == -1)
	{
          zmq_msg_close(&llm_cpy);
	  bgpwatcher_err_set_err(ERR, errno,
				 "Could not pass message to server");
	  return -1;
	}

      i++;
      llm = zlist_next(req->msg_frames);
    }

  return 0;
}

static int is_shutdown_time(bgpwatcher_client_broker_t *broker)
{
  if(broker->shutdown_time > 0 &&
     ((zlist_size(broker->req_list) == 0) ||
      (broker->shutdown_time <= zclock_time())))
    {
      /* time to end */
      return 1;
    }
  return 0;
}

static int handle_timeouts(bgpwatcher_client_broker_t *broker)
{
  bgpwatcher_client_broker_req_t *req;

  /* re-tx any requests that have timed out */
  req = zlist_first(broker->req_list);
  while(req != NULL)
    {
      if(zclock_time() < req->retry_at)
	{
	  /*fprintf(stderr, "DEBUG: at %"PRIu64", waiting for %"PRIu64"\n",
	    zclock_time(), req->retry_at);*/
	  /* the list is ordered, so we are done */
	  break;
	}

      /* we are either going to discard this request, or re-tx it */
      zlist_remove(broker->req_list, req);

      if(--req->retries_remaining == 0)
	{
	  /* time to abandon this request */
	  /** @todo send notice to client */
	  fprintf(stderr,
		  "DEBUG: Request %"PRIu32" expired without reply, "
		  "abandoning\n",
		  req->seq_num);

	  /** @todo remove the request from the hash too (just for sake of
	      memory) */

	  bgpwatcher_client_broker_req_free(&req);
	  req = zlist_first(broker->req_list);
	  continue;
	}

      fprintf(stderr, "DEBUG: Retrying request %"PRIu32"\n", req->seq_num);

      if(send_request(broker, req) != 0)
	{
	  goto err;
	}

      /* add it to the end of the list (it is now the oldest time) */
      if(zlist_append(broker->req_list, req) != 0)
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
				 "Could not add request to list");
	}

      req = zlist_first(broker->req_list);
    }

  return 0;

 err:
  bgpwatcher_client_broker_req_free(&req);
  return -1;
}

static int handle_heartbeat_timer(zloop_t *loop, int timer_id, void *arg)
{
  bgpwatcher_client_broker_t *broker = (bgpwatcher_client_broker_t*)arg;

  zframe_t *frame = NULL;
  uint8_t msg_type_p;

  if(is_shutdown_time(broker) != 0)
    {
      return -1;
    }

  if(--broker->heartbeat_liveness_remaining == 0)
    {
      /* the server has been flat-lining for too long, get the paddles! */
      fprintf(stderr, "WARN: heartbeat failure, can't reach server\n");
      fprintf(stderr, "WARN: reconnecting in %"PRIu64" msec...\n",
              broker->reconnect_interval_next);

      zclock_sleep(broker->reconnect_interval_next);

      if(broker->reconnect_interval_next < CFG->reconnect_interval_max)
        {
          broker->reconnect_interval_next *= 2;
        }

      /* remove the server reader from the reactor */
      zloop_reader_end(broker->loop, broker->server_socket);
      /* destroy the server socket */
      zsocket_destroy(CFG->ctx, broker->server_socket);
      /* reconnect */
      server_connect(broker);
      assert(broker->server_socket != NULL);

      reset_heartbeat_liveness(broker);
    }

  /* send heartbeat to server if it is time */
  if(zclock_time() > broker->heartbeat_next)
    {
      msg_type_p = BGPWATCHER_MSG_TYPE_HEARTBEAT;
      if((frame = zframe_new(&msg_type_p, 1)) == NULL)
	{
	  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
				 "Could not create new heartbeat frame");
	  goto err;
	}

      if(zframe_send(&frame, broker->server_socket, 0) == -1)
	{
	  bgpwatcher_err_set_err(ERR, errno,
				 "Could not send heartbeat msg to server");
	  goto err;
	}

      reset_heartbeat_timer(broker);
    }

  if(handle_timeouts(broker) != 0)
    {
      return -1;
    }

  zframe_destroy(&frame);
  return 0;

 err:
  zframe_destroy(&frame);
  return -1;
}

static int handle_server_msg(zloop_t *loop, zsock_t *reader, void *arg)
{
  bgpwatcher_client_broker_t *broker = (bgpwatcher_client_broker_t*)arg;
  bgpwatcher_msg_type_t msg_type;

  if(is_shutdown_time(broker) != 0)
    {
      return -1;
    }

  msg_type = bgpwatcher_recv_type(broker->server_socket);

  if(zctx_interrupted != 0)
    {
      goto interrupt;
    }

  switch(msg_type)
    {
    case BGPWATCHER_MSG_TYPE_REPLY:
      reset_heartbeat_liveness(broker);
      if(handle_reply(broker) != 0)
        {
          goto err;
        }

      if(zctx_interrupted != 0)
        {
          goto interrupt;
        }
      break;

    case BGPWATCHER_MSG_TYPE_HEARTBEAT:
      reset_heartbeat_liveness(broker);
      break;

    default:
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                             "Invalid message type received from "
                             "server (%d)", msg_type);
      goto err;
    }

  broker->reconnect_interval_next =
    CFG->reconnect_interval_min;

  /* have we just processed the last reply? */
  if(is_shutdown_time(broker) != 0)
    {
      return -1;
    }
  if(handle_timeouts(broker) != 0)
    {
      return -1;
    }

  /* check if the number of outstanding requests has dropped enough to start
     accepting more from our master */
  if(broker->master_removed != 0 &&
     zlist_size(broker->req_list) < MAX_OUTSTANDING_REQ*0.8)
    {
      fprintf(stderr, "INFO: Accepting requests\n");
      if(zloop_reader(broker->loop, broker->master_pipe,
                      handle_master_msg, broker) != 0)
        {
          bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
                                 "Could not re-add master pipe to reactor");
          return -1;
        }
      broker->master_removed = 0;
    }

  return 0;

 interrupt:
  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INTERRUPT, "Caught interrupt");
  return -1;

 err:
  return -1;
}

static int handle_master_msg(zloop_t *loop, zsock_t *reader, void *arg)
{
  bgpwatcher_client_broker_t *broker = (bgpwatcher_client_broker_t*)arg;
  bgpwatcher_msg_type_t msg_type;
  zmq_msg_t *llm = NULL;
  bgpwatcher_client_broker_req_t *req;

  if(is_shutdown_time(broker) != 0)
    {
      return -1;
    }

  /* there is a message waiting for us from our master, but if we already have
     too many outstanding requests at the server, we risk filling the buffers
     and entering deadlock (not to mention it is just plain rude) */
  /* to avoid flapping, we remove ourselves at 100% of the max allowed, and the
     handle_server_msg function will add us back when the queue drops to 80% of
     allowed */
  if(zlist_size(broker->req_list) >= MAX_OUTSTANDING_REQ)
    {
      fprintf(stderr, "INFO: Rate limiting\n");
      zloop_reader_end(broker->loop, broker->master_pipe);
      broker->master_removed = 1;
    }

  /* peek at the first frame (msg type) */
  if((msg_type = bgpwatcher_recv_type(broker->master_zocket))
     != BGPWATCHER_MSG_TYPE_UNKNOWN)
    {
      /* there must be more frames for us */
      if(zsocket_rcvmore(broker->master_zocket) == 0)
        {
          bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                                 "Invalid message received from master "
                                 "(missing seq num)");
          goto err;
        }

      if((req = bgpwatcher_client_broker_req_init()) == NULL)
        {
          bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
                                 "Could not create new request structure");
          goto err;
        }

      req->msg_type = msg_type;

      /* now we need the seq number */
      if(zmq_recv(broker->master_zocket, &req->seq_num, sizeof(seq_num_t), 0)
         != sizeof(seq_num_t))
        {
          ISERR
            {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                                     "Invalid message received from master "
                                     "(malformed sequence number)");
              goto err;
            }
        }

      /* read the payload of the message into a list to send to the server */
      if(zsocket_rcvmore(broker->master_zocket) == 0)
        {
          bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                                 "Invalid message received from master "
                                 "(missing payload)");
          goto err;
        }

      /* recv messages into the req list until rcvmore is false */
      while(1)
        {
          if((llm = malloc(sizeof(zmq_msg_t))) == NULL ||
             zmq_msg_init(llm) != 0)
            {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
                                     "Could not create llm");
              goto err;
            }
          if(zmq_msg_recv(llm, broker->master_zocket, 0) == -1)
            {
              goto interrupt;
            }
          if(zlist_append(req->msg_frames, llm) != 0)
            {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
                                     "Could not add frame to req list");
            }
          if(zsocket_rcvmore(broker->master_zocket) == 0)
            {
              break;
            }
        }

      /* init the re-transmit state */
      req->retries_remaining = CFG->request_retries;
      /* retry_at is set by send_request */


      /*fprintf(stderr, "DEBUG: tx.seq: %"PRIu32", tx.msg_type: %d\n",
        req.seq_num, req.msg_type);*/

      /* add to the end of the req list */
      /* list will be ordered oldest (smallest time) to newest (largest
         time) */
      if(zlist_append(broker->req_list, req) != 0)
        {
          bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
                                 "Could not add request to list");
        }

      /* now send on to the server */
      if(send_request(broker, req) != 0)
        {
          goto err;
        }
      req = NULL;
    }
  else
    {
      /* this is a message for us, just shut down */
      fprintf(stderr,
              "INFO: Got $TERM, shutting down client broker on next "
              "cycle\n");
      if(broker->shutdown_time == 0)
        {
          broker->shutdown_time =
            zclock_time() + CFG->shutdown_linger;
        }
      if(is_shutdown_time(broker) != 0)
        {
          return -1;
        }
    }

  if(handle_timeouts(broker) != 0)
    {
      return -1;
    }

  return 0;

 interrupt:
  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INTERRUPT, "Caught interrupt");
  return -1;

 err:
  bgpwatcher_client_broker_req_free(&req);
  return -1;
}

static void req_free_wrap(void **req)
{
  bgpwatcher_client_broker_req_free((bgpwatcher_client_broker_req_t**)req);
}

static void broker_free(bgpwatcher_client_broker_t **broker_p)
{
  assert(broker_p != NULL);
  bgpwatcher_client_broker_t *broker = *broker_p;

  /* free our reactor */
  zloop_destroy(&broker->loop);

  if(zlist_size(broker->req_list) > 0)
    {
      fprintf(stderr,
	      "WARNING: At shutdown there were %"PRIuPTR" outstanding requests\n",
	      zlist_size(broker->req_list));
    }
  /* DO NOT set the destructor before this point otherwise zlist merrily free's
     records on every _remove. sigh */
  zlist_set_destructor(broker->req_list, req_free_wrap);
  zlist_destroy(&broker->req_list);

  /* free'd by zctx_destroy in master */
  broker->server_socket = NULL;

  free(broker);

  *broker_p = NULL;
  return;
}

static int init_req_state(bgpwatcher_client_broker_t *broker)
{
  /* init the outstanding req set */
  if((broker->req_list = zlist_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to create request list");
      return -1;
    }
  return 0;
}

static int init_reactor(bgpwatcher_client_broker_t *broker)
{
  /* set up the reactor */
  if((broker->loop = zloop_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not initialize reactor");
      return -1;
    }
  /* DEBUG */
  //zloop_set_verbose(broker->loop, true);

  /* add a heartbeat timer */
  if((broker->timer_id = zloop_timer(broker->loop,
                                     CFG->heartbeat_interval, 0,
                                     handle_heartbeat_timer, broker)) < 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add heartbeat timer reactor");
      return -1;
    }

  /* add master pipe to reactor */
  if(zloop_reader(broker->loop, broker->master_pipe,
                  handle_master_msg, broker) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add master pipe to reactor");
      return -1;
    }

  return 0;
}

static bgpwatcher_client_broker_t *broker_init(zsock_t *master_pipe,
                                   bgpwatcher_client_broker_config_t *cfg)
{
  bgpwatcher_client_broker_t *broker;

  if((broker = malloc_zero(sizeof(bgpwatcher_client_broker_t))) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not initialize broker state");
      return NULL;
    }

  broker->master_pipe = master_pipe;
  broker->master_zocket = zsock_resolve(master_pipe);
  assert(broker->master_zocket != NULL);
  broker->cfg = cfg;

  if(init_req_state(broker) != 0)
    {
      goto err;
    }

  /* init counters from options */
  reset_heartbeat_liveness(broker);
  broker->reconnect_interval_next = CFG->reconnect_interval_min;

  if(init_reactor(broker) != 0)
    {
      goto err;
    }

  return broker;

 err:
  broker_free(&broker);
  return NULL;
}

/* ========== PUBLIC FUNCS BELOW HERE ========== */

/* broker owns none of the memory passed to it. only responsible for what it
   mallocs itself (e.g. poller) */
void bgpwatcher_client_broker_run(zsock_t *pipe, void *args)
{
  bgpwatcher_client_broker_t *broker;

  assert(pipe != NULL);
  assert(args != NULL);

  if((broker =
      broker_init(pipe, (bgpwatcher_client_broker_config_t*)args)) == NULL)
    {
      return;
    }

  /* connect to the server */
  if(server_connect(broker) != 0)
    {
      return;
    }

  /* signal to our master that we are ready */
  if(zsock_signal(pipe, 0) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not send ready signal to master");
      return;
    }

  /* blocks until broker exits */
  zloop_start(broker->loop);

  if(server_disconnect(broker) != 0)
    {
      // err will be set
      broker_free(&broker);
      return;
    }

  broker_free(&broker);
  return;
}

bgpwatcher_client_broker_req_t *bgpwatcher_client_broker_req_init()
{
  bgpwatcher_client_broker_req_t *req;

  if((req = malloc(sizeof(bgpwatcher_client_broker_req_t))) == NULL)
    {
      return NULL;
    }

  if((req->msg_frames = zlist_new()) == NULL)
    {
      free(req);
      return NULL;
    }

  return req;
}

void bgpwatcher_client_broker_req_free(bgpwatcher_client_broker_req_t **req_p)
{
  assert(req_p != NULL);
  bgpwatcher_client_broker_req_t *req = *req_p;
  zmq_msg_t *llm;

  if(req != NULL)
    {
      if(req->msg_frames != NULL)
        {
          while(zlist_size(req->msg_frames) > 0)
            {
              llm = zlist_pop(req->msg_frames);
              zmq_msg_close(llm);
              free(llm);
            }
          zlist_destroy(&req->msg_frames);
        }
      free(req);
      *req_p = NULL;
    }
}
