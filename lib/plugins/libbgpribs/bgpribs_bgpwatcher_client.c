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

#include "config.h"
#include "utils.h"
#include <assert.h>
#include <bgpribs_bgpwatcher_client.h>

#ifdef WITH_BGPWATCHER



bw_client_t *bw_client_create() 
{
  bw_client_t * bwc = NULL;

  if((bwc = malloc_zero(sizeof(bw_client_t))) == NULL)
    {
      return NULL;
    }
  
  // init interests (no interests, this client is just a producer)
  bwc->interests = 0;
  // init intents: peer and prefix tables will be sent
  bwc->intents = BGPWATCHER_PRODUCER_INTENT_PREFIX | BGPWATCHER_PRODUCER_INTENT_PEER;

  if((bwc->client = bgpwatcher_client_init(bwc->interests, bwc->intents)) == NULL)
    {
      goto err;
    }
  
  
  // OPTIONAL settings HERE!

  if(bgpwatcher_client_start(bwc->client) != 0)
    {
      goto err;
    }

  return bwc;

 err:
  if(bwc!= NULL)
    {  
      bw_client_destroy(bwc);
    }
  return NULL;
}


void bw_client_destroy(bw_client_t * bwc) 
{  
  if(bwc!= NULL)
    {
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

