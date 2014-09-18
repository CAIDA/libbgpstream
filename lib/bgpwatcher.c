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

#include <bgpwatcher_int.h>

#include "utils.h"

#define ERR (&watcher->err)
#define WATCHER(x) ((bgpwatcher_t*)(x))

static int client_connect(bgpwatcher_server_t *server,
			  bgpwatcher_server_client_info_t *client,
			  void *user)
{
  return -1;
}

static int client_disconnect(bgpwatcher_server_t *server,
			     bgpwatcher_server_client_info_t *client,
			     void *user)
{
  return -1;
}

static int recv_pfx_record(bgpwatcher_server_t *server,
			   uint64_t table_id,
			   bgpwatcher_pfx_record_t *record,
			   void *user)
{
  return -1;
}

static int recv_peer_record(bgpwatcher_server_t *server,
			    uint64_t table_id,
			    bgpwatcher_peer_record_t *record,
			    void *user)
{
  return -1;
}

static int table_end(bgpwatcher_server_t *server,
		     uint64_t table_id,
		     bgpwatcher_server_table_type_t *table_type,
		     void *user)
{
  return -1;
}

static bgpwatcher_server_callbacks_t callback_template = {
  client_connect,
  client_disconnect,
  recv_pfx_record,
  recv_peer_record,
  table_end,
  NULL, /* user: to be filled with a 'self' pointer */
};

bgpwatcher_t *bgpwatcher_init()
{
  bgpwatcher_t *watcher = NULL;
  bgpwatcher_server_callbacks_t *callbacks = NULL;

  if((watcher = malloc_zero(sizeof(bgpwatcher_t))) == NULL)
    {
      fprintf(stderr, "ERROR: Could not allocate watcher\n");
      return NULL;
    }

  /* can set errors now */

  /* grab a copy of our callback pointers */
  if((callbacks = malloc(sizeof(bgpwatcher_server_callbacks_t))) == NULL)
    {
      bgpwatcher_err_set_err(ERR, BGPWATCHER_ERR_MALLOC,
			     "Could not malloc server callback structure");
      free(watcher);
      return NULL;
    }

  /* duplicate the template */
  memcpy(callbacks, &callback_template, sizeof(bgpwatcher_server_callbacks_t));

  /* insert our 'user' data */
  callbacks->user = watcher;

  /* init the server */
  if((watcher->server = bgpwatcher_server_init(callbacks)) == NULL)
    {
      free(callbacks);
      free(watcher);
      return NULL;
    }

  return watcher;
}

/** @todo add config functions */

int bgpwatcher_start(bgpwatcher_t *watcher)
{
  int rc;

  assert(watcher != NULL);

  rc = bgpwatcher_server_start(watcher->server);

  /* need to pass back the error code */
  watcher->err = watcher->server->err;

  return rc;
}

void bgpwatcher_stop(bgpwatcher_t *watcher)
{
  assert(watcher != NULL);
  bgpwatcher_server_stop(watcher->server);
}

void bgpwatcher_free(bgpwatcher_t *watcher)
{
  assert(watcher != NULL);

  if(watcher->server != NULL)
    {
      bgpwatcher_server_free(watcher->server);
      watcher->server = NULL;
    }

  free(watcher);
}

bgpwatcher_err_t bgpwatcher_get_err(bgpwatcher_t *watcher)
{
  bgpwatcher_err_t err = watcher->err;
  watcher->err.err_num = 0; /* "OK" */
  watcher->err.problem[0]='\0';
  return err;
}

void bgpwatcher_perr(bgpwatcher_t *watcher)
{
  bgpwatcher_err_perr(ERR);
}


int bgpwatcher_set_client_uri(bgpwatcher_t *watcher,
			      const char *uri)
{
  assert(watcher != NULL);

  return bgpwatcher_server_set_client_uri(watcher->server, uri);
}

void bgpwatcher_set_heartbeat_interval(bgpwatcher_t *watcher,
				       uint64_t interval_ms)
{
  assert(watcher != NULL);

  bgpwatcher_server_set_heartbeat_interval(watcher->server, interval_ms);
}

void bgpwatcher_set_heartbeat_liveness(bgpwatcher_t *watcher, int beats)
{
  assert(watcher != NULL);

  bgpwatcher_server_set_heartbeat_liveness(watcher->server, beats);
}

