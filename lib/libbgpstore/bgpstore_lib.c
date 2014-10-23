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


#include "bgpstore_int.h"


bgpstore_t *bgpstore_create()
{
  bgpstore_t *bgp_store;
  if((bgp_store = malloc_zero(sizeof(bgpstore_t))) == NULL)
    {
      return NULL;
    }
  bgp_store->registered_clients = 0;
#ifdef DEBUG
  fprintf(stderr, "DEBUG: bgpstore created\n");
#endif
  return bgp_store;
}


void bgpstore_destroy(bgpstore_t *bgp_store)
{
  if(bgp_store != NULL)
    {
      free(bgp_store);
#ifdef DEBUG
      fprintf(stderr, "DEBUG: bgpstore destroyed\n");
#endif
    }
}

