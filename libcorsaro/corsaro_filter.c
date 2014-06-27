/*
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "corsaro_int.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#include "corsaro_log.h"

#include "corsaro_filter.h"

/** ==== PUBLIC API FUNCTIONS BELOW HERE ==== */

corsaro_filter_manager_t *corsaro_filter_manager_init(corsaro_t *corsaro)
{
  corsaro_filter_manager_t *manager;

  if((manager = malloc_zero(sizeof(corsaro_filter_manager_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc filter manager");
      return NULL;
    }

  /* annnnd, we're done */
  return manager;
}

void corsaro_filter_manager_free(corsaro_filter_manager_t *manager)
{
  int i;

  assert(manager != NULL);

  /* free all the filters that we have allocated */
  if(manager->filters != NULL)
    {
      for(i=0; i<manager->filters_cnt; i++)
	{
	  corsaro_filter_free(manager->filters[i]);
	  manager->filters[i] = NULL;
	}
      free(manager->filters);
      manager->filters = NULL;
      manager->filters_cnt = 0;
    }

  free(manager);

  return;
}

corsaro_filter_t *corsaro_filter_init(corsaro_t *corsaro, const char *name,
				      void *user)
{
  assert(corsaro != NULL);

  corsaro_filter_t *filter;
  corsaro_filter_manager_t *manager = corsaro->filter_manager;

  assert(manager != NULL);

  /* first check if we have enough space to alloc this filter */
  if(manager->filters_cnt == CORSARO_FILTER_ID_MAX)
    {
      corsaro_log(__func__, corsaro, "Maximum number of filters (%d) exceeded, "
		  "cannot allocate filter (%s)\n",
		  CORSARO_FILTER_ID_MAX,
		  name);
      return NULL;
    }

  if((filter = malloc(sizeof(corsaro_filter_init))) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc filter");
      return NULL;
    }

  /* get the next available plugin number */
  filter->id = manager->filters_cnt++;

  /* save the name */
  filter->name = strdup(name);

  /* save us */
  filter->manager = manager;

  /* save them */
  filter->user = user;

  /* resize the array of filters to hold this one */
  if((manager->filters = realloc(corsaro, sizeof(corsaro_filter_t*) *
				 manager->filters_cnt)) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc filter");
      return NULL;
    }

  manager->filters[filter->id] = filter;

  return filter;
}

corsaro_filter_t *corsaro_filter_get(corsaro_t *corsaro, const char *name)
{
  assert(corsaro != NULL);
  assert(corsaro->filter_manager != NULL);
  corsaro_filter_manager_t *manager = corsaro->filter_manager;

  int i;

  for(i=0; i<manager->filters_cnt; i++)
    {
      if(manager->filters[i] != NULL && manager->filters[i]->name)
	{
	  return manager->filters[i];
	}
    }

  return NULL;
}

int corsaro_filter_get_all(corsaro_t *corsaro, corsaro_filter_t ***filters)
{
  assert(corsaro != NULL);
  assert(corsaro->filter_manager != NULL);
  *filters = corsaro->filter_manager->filters;
  return corsaro->filter_manager->filters_cnt;
}

void corsaro_filter_free(corsaro_filter_t *filter)
{
  assert(filter != NULL);
  assert(filter->manager != NULL);

  filter->manager->filters[filter->id] = NULL;

  if(filter->name != NULL)
    {
      free(filter->name);
      filter->name = NULL;
    }

  /* we do not own 'manager' */

  /* we do not own 'user' */

  free(filter);
}

int corsaro_filter_is_match(corsaro_packet_t *packet, corsaro_filter_t *filter)
{
  assert(packet != NULL);
  assert(filter != NULL);
  assert(filter->id <= CORSARO_FILTER_MAX_ID && filter->id > 0);

  return packet->state.filter_matches & (1<<filter->id);
}

int corsaro_filter_is_match_any(corsaro_packet_t *packet)
{
  return packet->state.filter_matches;
}

void corsaro_filter_set_match(corsaro_packet_t *packet, corsaro_filter_t *filter,
			     int match)
{
  assert(packet != NULL);
  assert(filter != NULL);
  assert(filter->id <= CORSARO_FILTER_MAX_ID && filter->id > 0);

  packet->state.filter_matches |= (1<<filter->id);
  return;
}
