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
#include "bgpwatcher_client_broker.h"

#include "khash.h"
#include "utils.h"

#define ERR (&client->err)
#define BCFG (client->broker_config)

/* create and send headers for a data message */
int send_data_hdrs(void *dest,
                   bgpwatcher_data_msg_type_t type,
                   bgpwatcher_client_t *client,
                   seq_num_t *seq_num)
{
  uint8_t type_b;
  *seq_num = client->seq_num;

  type_b = BGPWATCHER_MSG_TYPE_DATA;
  if(zmq_send(dest, &type_b, bgpwatcher_msg_type_size_t, ZMQ_SNDMORE)
     != bgpwatcher_msg_type_size_t)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add request type to message");
      goto err;
    }

  /* sequence number */
  if(zmq_send(dest, seq_num, sizeof(seq_num_t), ZMQ_SNDMORE)
     != sizeof(seq_num_t))
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add sequence number to message");
      goto err;
    }

  /* request type */
  type_b = type;
  if(zmq_send(dest, &type_b, bgpwatcher_data_msg_type_size_t, ZMQ_SNDMORE)
     != bgpwatcher_data_msg_type_size_t)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add request type to message");
      goto err;
    }

  client->seq_num++;
  return 0;

 err:
  return -1;
}

/* given a formed data message, push on the needed headers and send */
int send_data_message(zmsg_t **msg_p,
		      bgpwatcher_data_msg_type_t type,
		      bgpwatcher_client_t *client)
{
  uint8_t type_b;
  zmsg_t *msg = *msg_p;
  seq_num_t seq_num = client->seq_num;
  assert(msg != NULL);
  *msg_p = NULL;

  /* (working backward), we prepend the request type */
  type_b = type;
  if(zmsg_pushmem(msg, &type_b, bgpwatcher_data_msg_type_size_t) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add request type to message");
      goto err;
    }

  /* now prepend the sequence number */
  if(zmsg_pushmem(msg, &seq_num, sizeof(seq_num_t)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add sequence number to message");
      goto err;
    }

  type_b = BGPWATCHER_MSG_TYPE_DATA;
  if(zmsg_pushmem(msg, &type_b, bgpwatcher_msg_type_size_t) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add request type to message");
      goto err;
    }

  if(zmsg_send(&msg, client->broker) != 0)
    {
      bgpwatcher_err_set_err(ERR, errno,
			     "Could not send data message to broker");
      return -1;
    }

  client->seq_num++;
  return seq_num;

 err:
  zmsg_destroy(&msg);
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

void bgpwatcher_client_set_cb_handle_reply(bgpwatcher_client_t *client,
					bgpwatcher_client_cb_handle_reply_t *cb)
{
  assert(client != NULL);
  BCFG.callbacks.handle_reply = cb;
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

bgpwatcher_client_pfx_table_t *bgpwatcher_client_pfx_table_create(
						   bgpwatcher_client_t *client)
{
  bgpwatcher_client_pfx_table_t *table;

  ASSERT_INTENT(BGPWATCHER_PRODUCER_INTENT_PREFIX);

  if((table = malloc_zero(sizeof(bgpwatcher_client_pfx_table_t))) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not malloc pfx table");
      return NULL;
    }

  table->client = client;

  return table;
}

void bgpwatcher_client_pfx_table_free(bgpwatcher_client_pfx_table_t **table_p)
{
  bgpwatcher_client_pfx_table_t *table = *table_p;
  if(table != NULL)
    {
      free(table);
      *table_p = NULL;
    }
}

int bgpwatcher_client_pfx_table_begin(bgpwatcher_client_pfx_table_t *table,
                                      const char *collector_name,
                                      bgpstream_ip_address_t *peer_ip,
				      uint32_t time)
{
  zmsg_t *msg = NULL;
  int rc;
  assert(table != NULL);
  assert(table->started == 0);

  /* fill the table */
  table->info.time = time;
  table->info.collector = collector_name;
  table->info.peer_ip = *peer_ip;

  if((msg = bgpwatcher_pfx_table_msg_create(&table->info)) == NULL)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize prefix table");
      return -1;
    }

  if((rc = send_data_message(&msg,
                             BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN,
                             table->client)) >= 0)
    {
      table->started = 1;
    }

  zmsg_destroy(&msg);
  return rc;
}

int bgpwatcher_client_pfx_table_add(bgpwatcher_client_pfx_table_t *table,
				    bgpstream_prefix_t *prefix,
                                    uint32_t orig_asn)
{
  assert(table != NULL);
  seq_num_t seq;

  if(send_data_hdrs(table->client->broker_zocket,
                    BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD,
                    table->client, &seq) != 0)
    {
      return -1;
    }

  if(bgpwatcher_pfx_record_send(table->client->broker_zocket,
                                prefix, orig_asn, 0) != 0)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to send prefix record");
      return -1;
    }

  return seq;
}

int bgpwatcher_client_pfx_table_end(bgpwatcher_client_pfx_table_t *table)
{
  assert(table != NULL);
  assert(table->started == 1);
  int rc;
  zmsg_t *msg = NULL;

  if((msg = bgpwatcher_pfx_table_msg_create(&table->info)) == NULL)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize prefix table");
      return -1;
    }

  if((rc = send_data_message(&msg, BGPWATCHER_DATA_MSG_TYPE_TABLE_END,
                             table->client)) >= 0)
    {
      table->started = 0;
    }

  zmsg_destroy(&msg);
  return rc;
}

bgpwatcher_client_peer_table_t *bgpwatcher_client_peer_table_create(
						   bgpwatcher_client_t *client)
{
  bgpwatcher_client_peer_table_t *table;

  ASSERT_INTENT(BGPWATCHER_PRODUCER_INTENT_PEER);

  if((table = malloc_zero(sizeof(bgpwatcher_client_peer_table_t))) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not malloc peer table");
      return NULL;
    }

  table->client = client;

  return table;
}

void bgpwatcher_client_peer_table_free(bgpwatcher_client_peer_table_t **table_p)
{
  bgpwatcher_client_peer_table_t *table = *table_p;
  if(table != NULL)
    {
      free(table);
      *table_p = NULL;
    }
}

int bgpwatcher_client_peer_table_begin(bgpwatcher_client_peer_table_t *table,
                                       const char *collector_name,
                                       uint32_t time)
{
  int rc;
  zmsg_t *msg = NULL;
  assert(table != NULL);
  assert(table->started == 0);

  table->info.time = time;
  table->info.collector = collector_name;

  if((msg = bgpwatcher_peer_table_msg_create(&table->info)) == NULL)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize peer table");
      return -1;
    }

  if((rc = send_data_message(&msg,
                             BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN,
                             table->client)) >= 0)
    {
      table->started = 1;
    }

  zmsg_destroy(&msg);
  return rc;
}

int bgpwatcher_client_peer_table_add(bgpwatcher_client_peer_table_t *table,
                                     bgpstream_ip_address_t *peer_ip,
                                     uint8_t status)
{
  zmsg_t *msg;

  assert(table != NULL);
  assert(peer_ip != NULL);

  if((msg = bgpwatcher_peer_msg_create(peer_ip, status)) == NULL)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize peer record");
      return -1;
    }

  return send_data_message(&msg, BGPWATCHER_DATA_MSG_TYPE_PEER_RECORD,
			   table->client);
}

int bgpwatcher_client_peer_table_end(bgpwatcher_client_peer_table_t *table)
{
  assert(table != NULL);
  assert(table->started == 1);
  int rc;
  zmsg_t *msg = NULL;

  if((msg = bgpwatcher_peer_table_msg_create(&table->info)) == NULL)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize peer table");
      return -1;
    }

  if((rc = send_data_message(&msg,
                             BGPWATCHER_DATA_MSG_TYPE_TABLE_END,
                             table->client)) >= 0)
    {
      table->started = 0;
    }

  zmsg_destroy(&msg);
  return rc;
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
