/*
 * bgpwatcher
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
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


#include "bgpstore_bgpview.h"


#define BW_PEER_TABLE_PENDING 0b00000001
#define BW_PFX_TABLE_PENDING  0b00000010
#define BW_PEER_TABLE_DONE    0b00000100
#define BW_CLIENT_DONE        0b00001000



bgpview_t *bgpview_create()
{
  bgpview_t *bgp_view;
  // allocate memory for the structure
  if((bgp_view = malloc_zero(sizeof(bgpview_t))) == NULL)
    {
      return NULL;
    }
  // init internal parameters
  bgp_view->test = 0;
  if((bgp_view->client_status = kh_init(strclientstatus)) == NULL)
    {
      fprintf(stderr, "Failed to create client_status in bgpview\n");
      goto err;
    }

  return bgp_view;

 err:
  if(bgp_view != NULL)
    {
      bgpview_destroy(bgp_view);
    }
  return NULL;
}


void bgpview_destroy(bgpview_t *bgp_view)
{
  if(bgp_view != NULL)
    {
      if(bgp_view->client_status != NULL)
	{
	  kh_destroy(strclientstatus, bgp_view->client_status);
	  bgp_view->client_status = NULL;
	}
      free(bgp_view);
    }
}

