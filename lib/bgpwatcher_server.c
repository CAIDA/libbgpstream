/*
 * This file is part of bgpwatcher
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#include "config.h"

#include <stdint.h>
#include <stdio.h>

#include "bgpwatcher_common_int.h"
#include "bgpwatcher_server_int.h"
#include "bgpwatcher_view_io.h"

#include "khash.h"
#include "utils.h"

#define ERR (&server->err)

enum {
  POLL_ITEM_CLIENT = 0,
  POLL_ITEM_CNT    = 1,
};


#define SERVER_METRIC_FORMAT "%s.meta.bgpwatcher.server"

#define DUMP_METRIC(metric_prefix, value, time, fmt, ...)               \
  do {                                                                  \
    fprintf(stdout, SERVER_METRIC_FORMAT"."fmt" %"PRIu64" %"PRIu32"\n", \
            metric_prefix, __VA_ARGS__, value, time);                   \
  } while(0)                                                            \



/* after how many heartbeats should we ask the store to check timeouts */
#define STORE_HEARTBEATS_PER_TIMEOUT 60

/** Number of zmq I/O threads */
#define SERVER_ZMQ_IO_THREADS 3


static void client_free(bgpwatcher_server_client_t **client_p)
{
  bgpwatcher_server_client_t *client = *client_p;

  if(client == NULL)
    {
      return;
    }

  zmq_msg_close(&client->identity);

  free(client->id);
  client->id = NULL;
  free(client->hexid);
  client->hexid = NULL;

  free(client);

  *client_p = NULL;
  return;
}

/* because the hash calls with only the pointer, not the local ref */
static void client_free_wrap(bgpwatcher_server_client_t *client)
{
  client_free(&client);
}

static char *msg_strhex(zmq_msg_t *msg)
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

static char *msg_str(zmq_msg_t *msg)
{
  char *str;

  byte *data = zmq_msg_data(msg);
  size_t size = zmq_msg_size(msg);

  if((str = malloc(sizeof(char)*(size+1))) == NULL)
    {
      return NULL;
    }

  memcpy(str, data, size);
  str[size] = '\0';

  return str;
}

static int msg_isbinary(zmq_msg_t *msg)
{
  size_t size = zmq_msg_size(msg);
  byte *data = zmq_msg_data(msg);
  size_t i;

  for(i = 0; i < size; i++)
    {
      if(data[i] < 9 || data[i] > 127)
        {
          return 1;
        }
    }
  return 0;
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

  client->hexid = msg_strhex(&client->identity);
  assert(client->hexid);

  if(msg_isbinary(&client->identity) != 0)
    {
      client->id = msg_strhex(&client->identity);
    }
  else
    {
      client->id = msg_str(&client->identity);
    }
  if(client->id == NULL)
    {
      return NULL;
    }
  client->expiry = zclock_time() +
    (server->heartbeat_interval * server->heartbeat_liveness);

  client->info.name = client->id;

  /* insert client into the hash */
  khiter = kh_put(strclient, server->clients, client->hexid, &khret);
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
  if((khiter = kh_get(strclient, server->clients, client->hexid)) ==
     kh_end(server->clients))
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
	  if(bgpwatcher_store_client_disconnect(server->store,
                                                &client->info) != 0)
	    {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_STORE,
                                     "Store failed to handle client disconnect");
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
      fprintf(stderr, "Failed to send reply client id for %s - %d\n", client->id, errno);
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

static int handle_recv_view(bgpwatcher_server_t *server,
                            bgpwatcher_server_client_t *client)
{
  uint32_t view_time;
  bgpwatcher_view_t *view;

  /* first receive the time of the view */
  if(zmq_recv(server->client_socket, &view_time, sizeof(view_time), 0)
     != sizeof(view_time))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not recieve view time header");
      goto err;
    }
  view_time = ntohl(view_time);

  DUMP_METRIC(server->metric_prefix,
              zclock_time()/1000 - view_time,
              view_time,
              "view_receive.%s.begin_delay", client->id);

#ifdef DEBUG
  fprintf(stderr, "**************************************\n");
  fprintf(stderr, "DEBUG: Getting view from client (%"PRIu32"):\n",
          view_time);
  fprintf(stderr, "**************************************\n\n");
#endif

  /* ask the store for a pointer to the view to recieve into */
  view = bgpwatcher_store_get_view(server->store, view_time);

  /* temporarily store the truncated time so that we can fix the view after it
     has been rx'd */
  if(view != NULL)
    {
      view_time = bgpwatcher_view_get_time(view);
    }

  /* receive the view */
  if(bgpwatcher_view_recv(server->client_socket, view) != 0)
    {
      goto err;
    }

  if(view != NULL)
    {
      /* now reset the time to what the store wanted it to be */
      bgpwatcher_view_set_time(view, view_time);
    }

  DUMP_METRIC(server->metric_prefix,
              zclock_time()/1000-view_time,
              view_time,
              "view_receive.%s.receive_delay", client->id);

  /* tell the store that the view has been updated */
  if(bgpwatcher_store_view_updated(server->store, view, &client->info) != 0)
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
static int handle_view_message(bgpwatcher_server_t *server,
			       bgpwatcher_server_client_t *client)
{
  zmq_msg_t seq_msg;

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

  /* regardless of what they asked for, let them know that we got the request */
  if(send_reply(server, client, &seq_msg) != 0)
    {
      goto err;
    }

  if(handle_recv_view(server, client) != 0)
    {
      goto err;
    }

  return 0;

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

  uint8_t new_interests;
  uint8_t new_intents;

  /* first frame is their interests */
  if(zsocket_rcvmore(server->client_socket) == 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Message missing interests");
      goto err;
    }
  if(zmq_recv(server->client_socket, &new_interests,
	      sizeof(new_interests), 0)
     != sizeof(new_interests))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not extract client interests");
      goto err;
    }

 /* next is the intents */
  if(zsocket_rcvmore(server->client_socket) == 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Message missing intents");
      goto err;
    }
  if(zmq_recv(server->client_socket, &new_intents,
	      sizeof(new_intents), 0)
     != sizeof(new_intents))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Could not extract client intents");
      goto err;
    }

  /* we already knew about this client, don't re-add */
  if(client->info.interests == new_interests &&
     client->info.intents == new_intents)
    {
      return 0;
    }

  client->info.interests = new_interests;
  client->info.intents = new_intents;

  /* call the "client connect" callback */
  if(bgpwatcher_store_client_connect(server->store, &client->info) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_STORE,
                             "Store failed to handle client connect");
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
  uint64_t begin_time;
  assert(client_p != NULL);
  bgpwatcher_server_client_t *client = *client_p;
  assert(client != NULL);
  zmq_msg_t msg;


  /* check each type we support (in descending order of frequency) */
  switch(msg_type)
    {
    case BGPWATCHER_MSG_TYPE_VIEW:
      begin_time = zclock_time();

      /* every data now begins with interests and intents */
      if(handle_ready_message(server, client) != 0)
        {
          goto err;
        }

      /* parse the request, and then call the appropriate callback */
      if(handle_view_message(server, client) != 0)
	{
	  /* err no will already be set */
	  goto err;
	}

      fprintf(stderr, "DEBUG: handle_view_message from %s %"PRIu64"\n",
              client->id, zclock_time()-begin_time);

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
      if(bgpwatcher_store_client_disconnect(server->store, &client->info) != 0)
        {
          bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_STORE,
                                 "Store failed to handle client disconnect");
          goto err;
        }

      clients_remove(server, client);
      client_free(&client);
      break;

    default:
      fprintf(stderr, "Invalid message type (%d) rx'd from client, ignoring",
              msg_type);
      /* need to recv remainder of message */
      while(zsocket_rcvmore(server->client_socket) != 0)
        {
          if(zmq_msg_init(&msg) == -1)
            {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
                                     "Could not init proxy message");
              goto err;
            }
          if(zmq_msg_recv(&msg, server->client_socket, 0) == -1)
            {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
                                     "Failed to clear message from socket");
              goto err;
            }
          zmq_msg_close(&msg);
        }
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

  uint64_t begin_time = zclock_time();

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
  msg_type = bgpwatcher_recv_type(server->client_socket, 0);

  /* check if this client is already registered */
  if((client = client_get(server, &client_id)) == NULL)
    {
      /* create state for this client */
      if((client = client_init(server, &client_id)) == NULL)
	{
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

      /* should we ask the store to check its timeouts? */
      if(server->store_timeout_cnt == STORE_HEARTBEATS_PER_TIMEOUT)
        {
          fprintf(stderr, "DEBUG: Checking store timeouts\n");
          if(bgpwatcher_store_check_timeouts(server->store) != 0)
            {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_STORE,
                                     "Failed to check store timeouts");
              goto err;
            }
          server->store_timeout_cnt = 0;
        }
      else
        {
          server->store_timeout_cnt++;
        }
    }

  if(clients_purge(server) != 0)
    {
      goto err;
    }

  fprintf(stderr, "DEBUG: run_server in %"PRIu64"\n", zclock_time()-begin_time);

  return 0;

 err:
  return -1;

 interrupt:
  /* we were interrupted */
  bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INTERRUPT, "Caught SIGINT");
  return -1;
}

bgpwatcher_server_t *bgpwatcher_server_init()
{
  bgpwatcher_server_t *server = NULL;

  if((server = malloc_zero(sizeof(bgpwatcher_server_t))) == NULL)
    {
      fprintf(stderr, "ERROR: Could not allocate server structure\n");
      return NULL;
    }

  /* init czmq */
  if((server->ctx = zctx_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to create 0MQ context");
      goto err;
    }

  zsys_set_io_threads(SERVER_ZMQ_IO_THREADS);
    
  /* set default config */

  if((server->client_uri =
      strdup(BGPWATCHER_CLIENT_URI_DEFAULT)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to duplicate client uri string");
      goto err;
    }

  if((server->client_pub_uri =
      strdup(BGPWATCHER_CLIENT_PUB_URI_DEFAULT)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to duplicate client pub uri string");
      goto err;
    }

  server->heartbeat_interval = BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;

  server->heartbeat_liveness = BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;

  server->store_window_len = BGPWATCHER_SERVER_WINDOW_LEN;

  /* create an empty client list */
  if((server->clients = kh_init(strclient)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not create client list");
      goto err;
    }

  strcpy(server->metric_prefix, BGPWATCHER_METRIC_PREFIX_DEFAULT);
  
  return server;

 err:
  if(server != NULL)
    {
      bgpwatcher_server_free(server);
    }
  return NULL;
}


void bgpwatcher_server_set_metric_prefix(bgpwatcher_server_t *server, char *metric_prefix)
{
  if(metric_prefix != NULL && strlen(metric_prefix) < BGPWATCHER_METRIC_PREFIX_LEN-1)
    {
      strcpy(server->metric_prefix, metric_prefix);
    }
}

int bgpwatcher_server_start(bgpwatcher_server_t *server)
{
  if((server->store =
      bgpwatcher_store_create(server, server->store_window_len)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not create store");
      return -1;
    }

  /* bind to client socket */
  if((server->client_socket = zsocket_new(server->ctx, ZMQ_ROUTER)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_START_FAILED,
			     "Failed to create client socket");
      return -1;
    }
  /*zsocket_set_router_mandatory(server->client_socket, 1);*/
  zsocket_set_rcvtimeo(server->client_socket, server->heartbeat_interval);
  zsocket_set_sndhwm(server->client_socket, 0);
  zsocket_set_rcvhwm(server->client_socket, 0);
  if(zsocket_bind(server->client_socket, "%s", server->client_uri) < 0)
    {
      bgpwatcher_err_set_err(ERR, errno, "Could not bind to client socket");
      return -1;
    }

  /* bind to the pub socket */
  if((server->client_pub_socket = zsocket_new(server->ctx, ZMQ_PUB)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_START_FAILED,
			     "Failed to create client PUB socket");
      return -1;
    }
  if(zsocket_bind(server->client_pub_socket, "%s", server->client_pub_uri) < 0)
    {
      bgpwatcher_err_set_err(ERR, errno,
                             "Could not bind to client PUB socket (%s)",
                             server->client_pub_uri);
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

  free(server->client_uri);
  server->client_uri = NULL;

  free(server->client_pub_uri);
  server->client_pub_uri = NULL;

  clients_free(server);
  server->clients = NULL;

  bgpwatcher_store_destroy(server->store);
  server->store = NULL;

  /* free'd by zctx_destroy */
  server->client_socket = NULL;

  zctx_destroy(&server->ctx);

  free(server);

  return;
}

void bgpwatcher_server_set_window_len(bgpwatcher_server_t *server,
				      int window_len)
{
  assert(server != NULL);
  server->store_window_len = window_len;
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

int bgpwatcher_server_set_client_pub_uri(bgpwatcher_server_t *server,
                                         const char *uri)
{
  assert(server != NULL);

  /* remember, we set one by default */
  assert(server->client_pub_uri != NULL);
  free(server->client_pub_uri);

  if((server->client_pub_uri = strdup(uri)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not malloc client pub uri string");
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

/* ========== PUBLISH FUNCTIONS ========== */

int bgpwatcher_server_publish_view(bgpwatcher_server_t *server,
                                   bgpwatcher_view_t *view,
                                   int interests)
{
  const char *pub = NULL;
  size_t pub_len = 0;

  uint32_t time = bgpwatcher_view_get_time(view);

#ifdef DEBUG
  fprintf(stderr, "DEBUG: Publishing view:\n");
  if(bgpwatcher_view_pfx_cnt(view, BGPWATCHER_VIEW_FIELD_ACTIVE) < 100)
    {
      bgpwatcher_view_dump(view);
    }
#endif

  /* get the publication message prefix */
  if((pub = bgpwatcher_consumer_interest_pub(interests)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Failed to publish view (Invalid interests)");
      goto err;
    }
  pub_len = strlen(pub);

  DUMP_METRIC(server->metric_prefix,
              (uint64_t)interests,
              time,
              "%s", "publication.interests");

  if(zmq_send(server->client_pub_socket, pub, pub_len, ZMQ_SNDMORE) != pub_len)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to send publication string");
      goto err;
    }

  /* NULL -> no peer filtering */
  if(bgpwatcher_view_send(server->client_pub_socket, view, NULL) != 0)
    {
      return -1;
    }

  DUMP_METRIC(server->metric_prefix,
              zclock_time()/1000 - time,
              time,
              "%s", "publication.delay");

  return 0;

 err:
  return -1;
}
