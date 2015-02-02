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

#include "bgpwatcher_client_int.h"

#include "bgpwatcher_client_broker.h"
#include "bgpwatcher_view_int.h"

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
int send_data_hdrs(bgpwatcher_client_t *client)
{
  uint8_t   type_b = BGPWATCHER_MSG_TYPE_DATA;
  seq_num_t seq_num = client->seq_num++;

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

  return 0;

 err:
  return -1;
}

static void pfx_peer_info_free(bgpwatcher_pfx_peer_info_t *info)
{
  free(info);
}

static int init_pfx_hashes(bgpwatcher_client_t *client)
{
  if((TBL.v4pfx_peers = kh_init(v4pfx_peers)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to init v4 pfx peers set");
      goto err;
    }

  if((TBL.v6pfx_peers = kh_init(v6pfx_peers)) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_INIT_FAILED,
			     "Failed to init v6 pfx peers set");
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

  if(init_pfx_hashes(client) != 0)
    {
      goto err;
    }

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

int bgpwatcher_client_pfx_table_begin(bgpwatcher_client_t *client,
                                      uint32_t time,
                                      char *collector,
                                      int peer_cnt)
{
  assert(TBL.started == 0);

  /* fill the table */
  TBL.info.time = time;
  TBL.info.collector = collector;

  /* reset the peer cnt */
  TBL.info.peers_cnt = peer_cnt;
  TBL.peers_added = 0;
  TBL.info.prefix_cnt = 0;

  /* ensure we have enough space to store all the peers */
  if(TBL.info.peers_alloc_cnt < peer_cnt)
    {
      if((TBL.info.peers = realloc(TBL.info.peers,
                                   sizeof(bgpwatcher_peer_t)*peer_cnt)) == NULL)
        {
          return -1;
        }
      TBL.info.peers_alloc_cnt = peer_cnt;
    }

  /* free the peer info structures */
  kh_free_vals(v4pfx_peers, TBL.v4pfx_peers, pfx_peer_info_free);
  kh_free_vals(v6pfx_peers, TBL.v6pfx_peers, pfx_peer_info_free);

  /* we allow the actual hash to be reused for a while */
  if(TBL.reuse_cnt < TABLE_MAX_REUSE_CNT)
    {
      /* reset the pfx_peers hash */
      kh_clear(v4pfx_peers, TBL.v4pfx_peers);
      kh_clear(v6pfx_peers, TBL.v6pfx_peers);
      TBL.reuse_cnt++;
    }
  else
    {
      fprintf(stderr, "DEBUG: Forcing hard-clear of pfx table\n");
      kh_destroy(v4pfx_peers, TBL.v4pfx_peers);
      kh_destroy(v6pfx_peers, TBL.v6pfx_peers);
      if(init_pfx_hashes(client) != 0)
	{
	  return -1;
	}
      TBL.reuse_cnt = 0;
    }

  TBL.started = 1;

  /* delay tx until table end */

  DUMP_METRIC(zclock_time()/1000-time,
              time,
              "producer.%s.begin_delay", collector);

  return 0;
}

int bgpwatcher_client_pfx_table_add_peer(bgpwatcher_client_t *client,
                                         bl_addr_storage_t *peer_ip,
                                         uint8_t status)
{
  int peer_id;
  assert(client != NULL);
  /* make sure they aren't trying to add more peers than they promised */
  assert(TBL.info.peers_cnt > TBL.peers_added);

  assert(TBL.info.peers_cnt <= TBL.info.peers_alloc_cnt);

  peer_id = TBL.peers_added++;
  TBL.info.peers[peer_id].ip = *peer_ip;
  TBL.info.peers[peer_id].status = status;
  return peer_id;
}

int bgpwatcher_client_pfx_table_add(bgpwatcher_client_t *client,
                                    int peer_id,
				    bl_pfx_storage_t *prefix,
                                    uint32_t orig_asn)
{
  int khret;
  khiter_t k;

  bgpwatcher_pfx_peer_info_t *peer_infos = NULL;

  assert(client != NULL);
  assert(peer_id >= 0 && peer_id < TBL.peers_added);
  assert(prefix != NULL);

  // it can cause segfault, as it could potentially access
  // a non allocated space
  // findme.prefix = *prefix;

  switch(prefix->address.version)
    {
    case BL_ADDR_IPV4:
      /* either get or insert this prefix */
      if((k = kh_get(v4pfx_peers, TBL.v4pfx_peers,
                     *bl_pfx_storage2ipv4(prefix)))
         == kh_end(TBL.v4pfx_peers))
        {
          /* allocate a peer info array */
          if((peer_infos =
              malloc_zero(sizeof(bgpwatcher_pfx_peer_info_t)
			  *TBL.info.peers_cnt)) == NULL)
            {
              return -1;
            }
          k = kh_put(v4pfx_peers, TBL.v4pfx_peers,
                     *bl_pfx_storage2ipv4(prefix), &khret);
          kh_val(TBL.v4pfx_peers, k) = peer_infos;
        }
      else
        {
          peer_infos = kh_val(TBL.v4pfx_peers, k);
        }
      break;
    case BL_ADDR_IPV6:
      if((k = kh_get(v6pfx_peers, TBL.v6pfx_peers,
                     *bl_pfx_storage2ipv6(prefix)))
         == kh_end(TBL.v6pfx_peers))
        {
          /* allocate a peer info array */
          if((peer_infos =
              malloc_zero(sizeof(bgpwatcher_pfx_peer_info_t)
			  *TBL.info.peers_cnt)) == NULL)
            {
              return -1;
            }
          k = kh_put(v6pfx_peers, TBL.v6pfx_peers,
                     *bl_pfx_storage2ipv6(prefix), &khret);
          kh_val(TBL.v6pfx_peers, k) = peer_infos;
        }
      else
        {
          peer_infos = kh_val(TBL.v6pfx_peers, k);
        }
      break;
    default:
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Wrong prefix version");
      return -1;
    }

  assert(peer_infos != NULL);

  /* now update the peer info */
  peer_infos[peer_id].orig_asn = orig_asn;
  peer_infos[peer_id].in_use = 1;

  return 0;
}

int bgpwatcher_client_pfx_table_end(bgpwatcher_client_t *client)
{
  khiter_t k;
  bl_ipv4_pfx_t *v4pfx;
  bl_ipv6_pfx_t *v6pfx;
  bgpwatcher_pfx_peer_info_t *peer_infos;

  assert(TBL.peers_added == TBL.info.peers_cnt);

  TBL.info.prefix_cnt = kh_size(TBL.v4pfx_peers) + kh_size(TBL.v6pfx_peers);

  DUMP_METRIC(zclock_time()/1000-TBL.info.time,
              TBL.info.time,
              "producer.%s.prefix_delay", TBL.info.collector);

  /* send table begin message */
  if(send_data_hdrs(client) != 0)
    {
      goto err;
    }
  if(bgpwatcher_pfx_table_begin_send(client->broker_zocket,
                                     &TBL.info) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to send prefix table begin");
      goto err;
    }

  /* send all the prefixes */
  for(k = kh_begin(TBL.v4pfx_peers); k != kh_end(TBL.v4pfx_peers); ++k)
    {
      if(kh_exist(TBL.v4pfx_peers, k))
        {
          v4pfx = &kh_key(TBL.v4pfx_peers, k);
          peer_infos = kh_val(TBL.v4pfx_peers, k);

          if(bgpwatcher_pfx_row_send(client->broker_zocket,
                                     bl_pfx_ipv42storage(v4pfx),
                                     peer_infos,
                                     TBL.info.peers_cnt) != 0)
            {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
                                     "Failed to send prefix row");
              goto err;
            }
        }
    }

  for(k = kh_begin(TBL.v6pfx_peers); k != kh_end(TBL.v6pfx_peers); ++k)
    {
      if(kh_exist(TBL.v6pfx_peers, k))
        {
          v6pfx = &kh_key(TBL.v6pfx_peers, k);
          peer_infos = kh_val(TBL.v6pfx_peers, k);

          if(bgpwatcher_pfx_row_send(client->broker_zocket,
                                     bl_pfx_ipv62storage(v6pfx),
                                     peer_infos,
                                     TBL.info.peers_cnt) != 0)
            {
              bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
                                     "Failed to send prefix row");
              goto err;
            }
        }
    }

  /* send table end */
  if(bgpwatcher_pfx_table_end_send(client->broker_zocket, &TBL.info) != 0)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Failed to send prefix table end");
      goto err;
    }

  DUMP_METRIC(zclock_time()/1000-TBL.info.time,
              TBL.info.time,
              "producer.%s.end_delay", TBL.info.collector);

  TBL.started = 0;

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

  /* destroy the peers array */
  free(TBL.info.peers);

  kh_free_vals(v4pfx_peers, TBL.v4pfx_peers, pfx_peer_info_free);
  kh_destroy(v4pfx_peers, TBL.v4pfx_peers);
  kh_free_vals(v6pfx_peers, TBL.v6pfx_peers, pfx_peer_info_free);
  kh_destroy(v6pfx_peers, TBL.v6pfx_peers);

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
