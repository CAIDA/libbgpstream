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


bgpview_t *bgpview_create()
{
  bgpview_t *bgp_view;
  if((bgp_view = malloc_zero(sizeof(bgpview_t))) == NULL)
    {
      return NULL;
    }
  // init internal parameters
  bgp_view->test = 0;
  return bgp_view;
}


void bgpview_destroy(bgpview_t *bgp_view)
{
  if(bgp_view != NULL)
    {
      free(bgp_view);
    }
}

