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

#include <stdint.h>

#include "bgpwatcher_client_int.h"

#include "bgpwatcher_client_broker.h"
#include "bgpwatcher_view_io.h"

#include "khash.h"
#include "utils.h"

#define ERR (&client->err)
#define BCFG (client->broker_config)
#define TBL (client->pfx_table)

/* allow the table hash to be reused for 1 day */
#define TABLE_MAX_REUSE_CNT 1440

#define METRIC_PREFIX "bgp.meta.bgpwatcher.client"

#define DUMP_METRIC(value, time, fmt, ...)                      \
do {                                                            \
  fprintf(stdout, METRIC_PREFIX"."fmt" %"PRIu64" %"PRIu32"\n",  \
          __VA_ARGS__, value, time);                            \
 } while(0)                                                     \

/* create and send headers for a data message */
int send_view_hdrs(bgpwatcher_client_t *client, bgpwatcher_view_t *view)
{
  uint8_t   type_b = BGPWATCHER_MSG_TYPE_VIEW;
  seq_num_t seq_num = client->seq_num++;
  uint32_t u32;

  /* message type */
  if(zmq_send(client->broker_zocket, &type_b,
              bgpwatcher_msg_type_size_t, ZMQ_SNDMORE)
     != bgpwatcher_msg_type_size_t)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add request type to message");
      goto err;
    }

  /* sequence number */
  if(zmq_send(client->broker_zocket, &seq_num, sizeof(seq_num_t), ZMQ_SNDMORE)
     != sizeof(seq_num_t))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add sequence number to message");
      goto err;
    }

  /* view time */
  u32 = htonl(bgpwatcher_view_get_time(view));
  if(zmq_send(client->broker_zocket, &u32, sizeof(u32), ZMQ_SNDMORE)
     != sizeof(u32))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not send view time header");
      goto err;
    }

  return 0;

 err:
  return -1;
}

/* ========== PUBLIC FUNCS BELOW HERE ========== */

bgpwatcher_client_t *bgpwatcher_client_init(uint8_t interests, uint8_t intents)
{
  bgpwatcher_client_t *client;
  if((client = malloc_zero(sizeof(bgpwatcher_client_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }
  /* now we are ready to set errors... */

  /* now init the shared state for our broker */

  BCFG.master = client;

  BCFG.interests = interests;
  BCFG.intents = intents;

  /* init czmq */
  if((BCFG.ctx = zctx_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to create 0MQ context");
      goto err;
    }

  if((BCFG.server_uri =
      strdup(BGPWATCHER_CLIENT_SERVER_URI_DEFAULT)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to duplicate server uri string");
      goto err;
    }

  if((BCFG.server_sub_uri =
      strdup(BGPWATCHER_CLIENT_SERVER_SUB_URI_DEFAULT)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to duplicate server SUB uri string");
      goto err;
    }

  BCFG.heartbeat_interval = BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;

  BCFG.heartbeat_liveness = BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;

  BCFG.reconnect_interval_min = BGPWATCHER_RECONNECT_INTERVAL_MIN;

  BCFG.reconnect_interval_max = BGPWATCHER_RECONNECT_INTERVAL_MAX;

  BCFG.shutdown_linger = BGPWATCHER_CLIENT_SHUTDOWN_LINGER_DEFAULT;

  BCFG.request_timeout = BGPWATCHER_CLIENT_REQUEST_TIMEOUT_DEFAULT;
  BCFG.request_retries = BGPWATCHER_CLIENT_REQUEST_RETRIES_DEFAULT;

  return client;

 err:
  if(client != NULL)
    {
      bgpwatcher_client_free(client);
    }
  return NULL;
}

void bgpwatcher_client_set_cb_userdata(bgpwatcher_client_t *client,
				       void *user)
{
  assert(client != NULL);
  BCFG.callbacks.user = user;
}

int bgpwatcher_client_start(bgpwatcher_client_t *client)
{
  /* crank up the broker */
  if((client->broker =
      zactor_new(bgpwatcher_client_broker_run, &BCFG)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to start broker");
      return -1;
    }

  /* by the time the zactor_new function returns, the broker has been
     initialized, so lets check for any error messages that it has signaled */
  if(bgpwatcher_err_is_err(&BCFG.err) != 0)
    {
      client->err = BCFG.err;
      client->shutdown = 1;
      return -1;
    }

  /* store a pointer to the socket we will use to talk with the broker */
  client->broker_zocket = zactor_resolve(client->broker);
  assert(client->broker_zocket != NULL);

  return 0;
}

void bgpwatcher_client_perr(bgpwatcher_client_t *client)
{
  assert(client != NULL);
  bgpwatcher_err_perr(ERR);
}

#define ASSERT_INTENT(intent) assert((BCFG.intents & intent) != 0);

int bgpwatcher_client_send_view(bgpwatcher_client_t *client,
                                bgpwatcher_view_t *view)
{
  if(send_view_hdrs(client, view) != 0)
    {
      goto err;
    }

  /* now just transmit the view */
  if(bgpwatcher_view_send(client->broker_zocket, view) != 0)
    {
      goto err;
    }

  return 0;

 err:
  return -1;
}

int bgpwatcher_client_recv_view(bgpwatcher_client_t *client,
				bgpwatcher_client_recv_mode_t blocking,
				bgpwatcher_view_t *view)

{
  uint8_t interests = 0;

  assert(view != NULL);

  /* attempt to get the set of interests */
  if(zmq_recv(client->broker_zocket,
	      &interests, sizeof(interests),
	      (blocking == BGPWATCHER_CLIENT_RECV_MODE_NONBLOCK) ?
	        ZMQ_DONTWAIT : 0
	      ) != sizeof(interests))
        {
	  /* likely this means that we have shut the broker down */
	  return -1;
        }

  if(bgpwatcher_view_recv(client->broker_zocket, view) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_PROTOCOL,
			     "Failed to receive view");
      return -1;
    }

  return interests;
}

void bgpwatcher_client_stop(bgpwatcher_client_t *client)
{
  /* shuts the broker down */
  zactor_destroy(&client->broker);

  /* grab the error message from the broker */
  if(bgpwatcher_err_is_err(&BCFG.err) != 0)
    {
      client->err = BCFG.err;
    }

  client->shutdown = 1;
  return;
}

void bgpwatcher_client_free(bgpwatcher_client_t *client)
{
  assert(client != NULL);

  /* @todo figure out a more elegant way to deal with this */
  if(client->shutdown == 0)
    {
      bgpwatcher_client_stop(client);
    }

  free(BCFG.server_uri);
  BCFG.server_uri = NULL;

  free(BCFG.server_sub_uri);
  BCFG.server_sub_uri = NULL;

  free(BCFG.identity);
  BCFG.identity = NULL;

  /* frees our sockets */
  zctx_destroy(&BCFG.ctx);

  free(client);

  return;
}

int bgpwatcher_client_set_server_uri(bgpwatcher_client_t *client,
				     const char *uri)
{
  assert(client != NULL);

  if(client->broker != NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not set server uri (broker started)");
      return -1;
    }

  free(BCFG.server_uri);

  if((BCFG.server_uri = strdup(uri)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not set server uri");
      return -1;
    }

  return 0;
}

int bgpwatcher_client_set_server_sub_uri(bgpwatcher_client_t *client,
                                         const char *uri)
{
  assert(client != NULL);

  if(client->broker != NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not set server SUB uri (broker started)");
      return -1;
    }

  free(BCFG.server_sub_uri);

  if((BCFG.server_sub_uri = strdup(uri)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not set server SUB uri");
      return -1;
    }

  return 0;
}

void bgpwatcher_client_set_heartbeat_interval(bgpwatcher_client_t *client,
					      uint64_t interval_ms)
{
  assert(client != NULL);

  if(client->broker != NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not set heartbeat interval (broker started)");
      return;
    }

  BCFG.heartbeat_interval = interval_ms;
}

void bgpwatcher_client_set_heartbeat_liveness(bgpwatcher_client_t *client,
					      int beats)
{
  assert(client != NULL);

  if(client->broker != NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not set heartbeat liveness (broker started)");
      return;
    }

  BCFG.heartbeat_liveness = beats;
}

void bgpwatcher_client_set_reconnect_interval_min(bgpwatcher_client_t *client,
						  uint64_t reconnect_interval_min)
{
  assert(client != NULL);

  if(client->broker != NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not set min reconnect interval "
			     "(broker started)");
      return;
    }

  BCFG.reconnect_interval_min = reconnect_interval_min;
}

void bgpwatcher_client_set_reconnect_interval_max(bgpwatcher_client_t *client,
						  uint64_t reconnect_interval_max)
{
  assert(client != NULL);

  if(client->broker != NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not set max reconnect interval "
			     "(broker started)");
      return;
    }

  BCFG.reconnect_interval_max = reconnect_interval_max;
}

void bgpwatcher_client_set_shutdown_linger(bgpwatcher_client_t *client,
					   uint64_t linger)
{
  assert(client != NULL);

  BCFG.shutdown_linger = linger;
}

void bgpwatcher_client_set_request_timeout(bgpwatcher_client_t *client,
					   uint64_t timeout_ms)
{
  assert(client != NULL);

  BCFG.request_timeout = timeout_ms;
}

void bgpwatcher_client_set_request_retries(bgpwatcher_client_t *client,
					   int retry_cnt)
{
  assert(client != NULL);

  BCFG.request_retries = retry_cnt;
}

int bgpwatcher_client_set_identity(bgpwatcher_client_t *client,
				   const char *identity)
{
  assert(client != NULL);

  if(client->broker != NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Could not set identity (broker started)");
      return -1;
    }

  free(BCFG.identity);

  if((BCFG.identity = strdup(identity)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not set client identity");
      return -1;
    }

  return 0;
}
