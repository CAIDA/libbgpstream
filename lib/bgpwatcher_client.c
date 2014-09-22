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
  if(zmsg_pushmem(msg, &client->sequence_num,
		  sizeof(client->sequence_num)) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add sequence number to message");
      goto err;
    }
  client->sequence_num++;

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

  return 0;

 err:
  zmsg_destroy(&msg);
  return -1;
}

int send_table(bgpwatcher_client_t *client,
		     bgpwatcher_table_type_t table_type,
		     bgpwatcher_data_msg_type_t begin_end)
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
  if(zmsg_pushmem(msg, &table_type,
		  bgpwatcher_table_type_size_t) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add table type to message");
      zmsg_destroy(&msg);
    }

  if(send_data_message(&msg, begin_end, client) != 0)
    {
      return -1;
    }

  return 0;
}

#if 0
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
		  bgpwatcher_table_type_size_t) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not add table type to message");
      return NULL;
    }

  return append_data_headers(msg,
			     BGPWATCHER_DATA_MSG_TYPE_TABLE_END,
			     client);
}

zmsg_t *build_test_peer(bgpwatcher_client_t *client)
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

  return append_data_headers(msg,
			     BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD,
			     client);
}
#endif

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

void bgpwatcher_client_pfx_table_set_time(bgpwatcher_client_pfx_table_t *table,
					  uint32_t time)
{
  assert(table != NULL);

  table->time = time;
}

int bgpwatcher_client_pfx_table_add(bgpwatcher_client_pfx_table_t *table,
				    bgpwatcher_pfx_record_t *pfx)
{
  zmsg_t *msg;

  assert(table != NULL);
  assert(pfx != NULL);

  /* check if we need to send a table start message */
  if(table->started == 0)
    {
      if(send_table(table->client,
		    BGPWATCHER_TABLE_TYPE_PREFIX,
		    BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN) != 0)
	{
	  /* err set */
	  return -1;
	}
      table->started = 1;
    }

  /* send off the prefix */
  if((msg = bgpwatcher_pfx_record_serialize(pfx)) == NULL)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize prefix record");
      goto err;
    }

  if(send_data_message(&msg, BGPWATCHER_DATA_MSG_TYPE_PREFIX_RECORD,
		       table->client) != 0)
    {
      goto err;
    }

  return 0;

 err:
  zmsg_destroy(&msg);
  return -1;
}

int bgpwatcher_client_pfx_table_flush(bgpwatcher_client_pfx_table_t *table)
{
  assert(table != NULL);

  /* to allow for empty tables */
  if(table->started == 0)
    {
      if(send_table(table->client,
		    BGPWATCHER_TABLE_TYPE_PREFIX,
		    BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN) != 0)
	{
	  /* err set */
	  return -1;
	}
      table->started = 1;
    }

  /* send a table end message */
  if(send_table(table->client,
		BGPWATCHER_TABLE_TYPE_PREFIX,
		BGPWATCHER_DATA_MSG_TYPE_TABLE_END) != 0)
    {
      /* err set */
      return -1;
    }
  /* mark as clean so the next 'add' call will trigger a table start message */
  table->started = 0;
  return 0;
}

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

void bgpwatcher_client_peer_table_set_time(bgpwatcher_client_peer_table_t *table,
					   uint32_t time)
{
  assert(table != NULL);

  table->time = time;
}

int bgpwatcher_client_peer_table_add(bgpwatcher_client_peer_table_t *table,
				     bgpwatcher_peer_record_t *peer)
{
  zmsg_t *msg;

  assert(table != NULL);
  assert(peer != NULL);

  /* check if we need to send a table start message */
  if(table->started == 0)
    {
      if(send_table(table->client,
		    BGPWATCHER_TABLE_TYPE_PEER,
		    BGPWATCHER_DATA_MSG_TYPE_TABLE_BEGIN) != 0)
	{
	  /* err set */
	  return -1;
	}
      table->started = 1;
    }

  /* send off the prefix */
  if((msg = bgpwatcher_peer_record_serialize(peer)) == NULL)
    {
      bgpwatcher_err_set_err(&table->client->err, BGPWATCHER_ERR_MALLOC,
			     "Failed to serialize peer record");
      goto err;
    }

  if(send_data_message(&msg, BGPWATCHER_DATA_MSG_TYPE_PEER_RECORD,
		       table->client) != 0)
    {
      goto err;
    }

  return 0;

 err:
  zmsg_destroy(&msg);
  return -1;
}

int bgpwatcher_client_peer_table_flush(bgpwatcher_client_peer_table_t *table)
{
  return -1;
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
  return;
}

void bgpwatcher_client_free(bgpwatcher_client_t *client)
{
  assert(client != NULL);

  /* broker is already shut down */

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
