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
#define BROKER (client->broker_state)

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

int send_table(bgpwatcher_client_t *client,
	       bgpwatcher_table_type_t table_type,
	       bgpwatcher_data_msg_type_t begin_end,
	       uint32_t table_time)
{
  zmsg_t *msg;

  assert(begin_end == BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN ||
	 begin_end == BGPWATCHER_DATA_MSG_TYPE_TABLE_END);

  if((msg = zmsg_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to create table begin/end message");
      return -1;
    }

  /* append the table type */
  if(zmsg_addmem(msg, &table_type,
		  bgpwatcher_table_type_size_t) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add table type to message");
      zmsg_destroy(&msg);
    }

  /* append the table start time */
  table_time = htonl(table_time);
  if(zmsg_addmem(msg, &table_time, sizeof(table_time)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add table time to message");
      zmsg_destroy(&msg);
    }

  return send_data_message(&msg, begin_end, client);
}

/* ========== PUBLIC FUNCS BELOW HERE ========== */

bgpwatcher_client_t *bgpwatcher_client_init()
{
  bgpwatcher_client_t *client;
  if((client = malloc_zero(sizeof(bgpwatcher_client_t))) == NULL)
    {
      /* cannot set an err at this point */
      return NULL;
    }
  /* now we are ready to set errors... */

  /* init the outstanding req set */
  if((BROKER.outstanding_req = kh_init(reqset)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to create request set");
      goto err;
    }

  /* init czmq */
  if((BROKER.ctx = zctx_new()) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to create 0MQ context");
      goto err;
    }

  if((BROKER.server_uri =
      strdup(BGPWATCHER_CLIENT_SERVER_URI_DEFAULT)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to duplicate server uri string");
      goto err;
    }

  BROKER.heartbeat_interval =
    BGPWATCHER_HEARTBEAT_INTERVAL_DEFAULT;

  BROKER.heartbeat_liveness_remaining =
    BROKER.heartbeat_liveness =
    BGPWATCHER_HEARTBEAT_LIVENESS_DEFAULT;

  BROKER.reconnect_interval_next =
    BROKER.reconnect_interval_min =
    BGPWATCHER_RECONNECT_INTERVAL_MIN;

  BROKER.reconnect_interval_max =
    BGPWATCHER_RECONNECT_INTERVAL_MAX;

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
  /* crank up the broker */
  if((client->broker =
      zactor_new(bgpwatcher_client_broker_run, &BROKER)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to start broker");
      return -1;
    }

  return 0;
}

void bgpwatcher_client_perr(bgpwatcher_client_t *client)
{
  assert(client != NULL);
  bgpwatcher_err_perr(ERR);
}

bgpwatcher_client_pfx_table_t *bgpwatcher_client_pfx_table_create(
						   bgpwatcher_client_t *client)
{
  bgpwatcher_client_pfx_table_t *table;

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
				      uint32_t time)
{
  int rc;
  assert(table != NULL);
  assert(table->started == 0);

  table->time = time;

  if((rc = send_table(table->client,
		      BGPWATCHER_TABLE_TYPE_PREFIX,
		      BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN,
		      table->time)) >= 0)
    {
      table->started = 1;
    }

  return rc;
}

int bgpwatcher_client_pfx_table_add(bgpwatcher_client_pfx_table_t *table,
				    bgpwatcher_pfx_record_t *pfx)
{
  zmsg_t *msg;

  assert(table != NULL);
  assert(pfx != NULL);

  if((msg = bgpwatcher_pfx_record_serialize(pfx)) == NULL)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize prefix record");
      return -1;
    }

  return send_data_message(&msg, BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD,
			   table->client);
}

int bgpwatcher_client_pfx_table_end(bgpwatcher_client_pfx_table_t *table)
{
  assert(table != NULL);
  assert(table->started == 1);
  int rc;

  /* send a table end message */
  if((rc = send_table(table->client,
		      BGPWATCHER_TABLE_TYPE_PREFIX,
		      BGPWATCHER_DATA_MSG_TYPE_TABLE_END,
		      table->time)) <= 0)
    {
      table->started = 0;
    }

  return rc;
}

/** consider making the table create/free/add/flush code more generic (a
    macro?) */
bgpwatcher_client_peer_table_t *bgpwatcher_client_peer_table_create(
						   bgpwatcher_client_t *client)
{
  bgpwatcher_client_peer_table_t *table;

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
					uint32_t time)
{
  int rc;
  assert(table != NULL);
  assert(table->started == 0);

  table->time = time;

  if((rc = send_table(table->client,
		      BGPWATCHER_TABLE_TYPE_PEER,
		      BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN,
		      table->time)) >= 0)
    {
      table->started = 1;
    }

  return rc;
}

int bgpwatcher_client_peer_table_add(bgpwatcher_client_peer_table_t *table,
				     bgpwatcher_peer_record_t *peer)
{
  zmsg_t *msg;

  assert(table != NULL);
  assert(peer != NULL);

  if((msg = bgpwatcher_peer_record_serialize(peer)) == NULL)
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

  /* send a table end message */
  if((rc = send_table(table->client,
		      BGPWATCHER_TABLE_TYPE_PEER,
		      BGPWATCHER_DATA_MSG_TYPE_TABLE_END,
		      table->time)) <= 0)
    {
      table->started = 0;
    }

  return rc;
}

void bgpwatcher_client_stop(bgpwatcher_client_t *client)
{
  /* shuts the broker down */
  zactor_destroy(&client->broker);

  /* grab the error message from the broker */
  if(bgpwatcher_err_is_err(&client->broker_state.err) != 0)
    {
      client->err = client->broker_state.err;
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

  /* broker now guaranteed to be shut down */

  if(BROKER.outstanding_reqs != NULL)
    {
      if(kh_size(BROKER.outstanding_reqs) > 0)
	{
	  fprintf(stderr,
		  "WARNING: At shutdown there were %d outstanding requests\n",
		  kh_size(BROKER.outstanding_req));
	}
      kh_destroy(reqset, BROKER.outstanding_req);
      BROKER.outstanding_req = NULL;
    }

  if(BROKER.server_uri != NULL)
    {
      free(BROKER.server_uri);
      BROKER.server_uri = NULL;
    }

  /* free'd by zctx_destroy */
  BROKER.server_socket = NULL;

  /* frees our sockets */
  zctx_destroy(&BROKER.ctx);

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

  free(BROKER.server_uri);

  if((BROKER.server_uri = strdup(uri)) == NULL)
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

  BROKER.heartbeat_interval = interval_ms;
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

  BROKER.heartbeat_liveness = beats;
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

  BROKER.reconnect_interval_min = reconnect_interval_min;
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

  BROKER.reconnect_interval_max = reconnect_interval_max;
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

  free(BROKER.identity);

  if((BROKER.identity = strdup(identity)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not set client identity");
      return -1;
    }

  return 0;
}
