/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef WITH_BGPWATCHER

#include <bgpribs_bgpwatcher_client.h>

#include <assert.h>
#include "utils.h"

/* bw_client_t related functions */

static void handle_reply(bgpwatcher_client_t *client, seq_num_t seq_num,
			 int rc, void *user)
{
  assert(rc == 0);  // assert the reply is correct
}


bw_client_t *bw_client_create() 
{
  bw_client_t * bwc = malloc_zero(sizeof(bw_client_t));
  if((bwc->client = bgpwatcher_client_init()) == NULL)
    {
      goto err;
    }
  
  // handle server reply using "handle_reply" callback: just assert reply is correct
  bgpwatcher_client_set_cb_handle_reply(bwc->client, handle_reply);
  
  // OPTIONAL settings HERE!

  if((bwc->pfx_table = bgpwatcher_client_pfx_table_create(bwc->client)) == NULL)
    {
      goto err;
    }

  if((bwc->pfx_record = bgpwatcher_pfx_record_init()) == NULL)
    {
      goto err;
    }

  if((bwc->peer_table = bgpwatcher_client_peer_table_create(bwc->client)) == NULL)
    {
      goto err;
    }

  if((bwc->peer_record = bgpwatcher_peer_record_init()) == NULL)
    {
      goto err;
    }

  if(bgpwatcher_client_start(bwc->client) != 0)
    {
      goto err;
    }

  return bwc;

 err:
  if(bwc!= NULL)
    {  
      bgpwatcher_client_perr(bwc->client);
      bgpwatcher_pfx_record_free(&bwc->pfx_record);
      bgpwatcher_client_pfx_table_free(&bwc->pfx_table);
      bgpwatcher_peer_record_free(&bwc->peer_record);
      bgpwatcher_client_peer_table_free(&bwc->peer_table);
      if(bwc->client != NULL) {
	bgpwatcher_client_free(bwc->client);
      }
      free(bwc);
    }
  return NULL;
}


void bw_client_destroy(bw_client_t * bwc) 
{  
  if(bwc!= NULL)
    {
      bgpwatcher_pfx_record_free(&bwc->pfx_record);
      bgpwatcher_client_pfx_table_free(&bwc->pfx_table);
      bgpwatcher_peer_record_free(&bwc->peer_record);
      bgpwatcher_client_peer_table_free(&bwc->peer_table);
      if(bwc->client != NULL)
	{
	  bgpwatcher_client_stop(bwc->client);
	  bgpwatcher_client_perr(bwc->client);
	  bgpwatcher_client_free(bwc->client);
	}
      free(bwc);
    }
}

#endif

