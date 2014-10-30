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


#include "bgpstore_common.h"


int compatible_intents(khash_t(strclientstatus) *active_clients, char* client_str, uint8_t mask)
{
  assert(active_clients);
  khiter_t k;
  clientstatus_t cs;
  if((k = kh_get(strclientstatus, active_clients, client_str)) != kh_end(active_clients))
    {
      cs = kh_value(active_clients,k);
      if(cs.producer_intents && mask)
	{
	  return 1;
	}
      return 0;
    }
  return -1;
}

int compatible_interests(khash_t(strclientstatus) *active_clients, char* client_str, uint8_t mask)
{
  assert(active_clients);
  khiter_t k;
  clientstatus_t cs;
  if((k = kh_get(strclientstatus, active_clients, client_str)) != kh_end(active_clients))
    {
      cs = kh_value(active_clients,k);
      if(cs.consumer_interests && mask)
	{
	  return 1;
	}
      return 0;
    }
  return -1;
}

