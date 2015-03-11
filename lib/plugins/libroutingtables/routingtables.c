/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2015 The Regents of the University of California.
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


#include "routingtables.h"
#include "routingtables_int.h"

#include "utils.h"

routingtables_t *routingtables_create()
{
  /** @todo */
  routingtables_t *rt = (routingtables_t *)malloc_zero(sizeof(int));
  return rt;
}

int routingtables_set_metric_prefix(routingtables_t *rt,
                                    char *metric_prefix)
{
  /** @todo */
  return 0;
}

char *routingtables_get_metric_prefix(routingtables_t *rt)
{
 /** @todo */
  return NULL;
}

#ifdef WITH_BGPWATCHER
int routingtables_activate_watcher_tx(routingtables_t *rt,
                                      char *client_name,
                                      char *server_uri,
                                      uint8_t tables_mask)
{
  /** @todo */
  return 0;
}
#endif

int routingtables_set_fullfeed_threshold(routingtables_t *rt,
                                         bgpstream_addr_version_t ip_version,
                                         int threshold)
{
  /** @todo */
  return 0;
}

int routingtables_get_fullfeed_threshold(routingtables_t *rt,
                                         bgpstream_addr_version_t ip_version)
{
  /** @todo */
  return 0;
}

int routingtables_interval_start(routingtables_t *rt,
                                 int start_time)
{
  /** @todo */
  return 0;
}


int routingtables_interval_end(routingtables_t *rt,
                               int end_time)
{
  /** @todo */
  return 0;
}

int routingtables_process_record(routingtables_t *rt,
                                 bgpstream_record_t *record)
{
  /** @todo */
  return 0;
}

void routingtables_destroy(routingtables_t *rt)
{
  if(rt != NULL)
    {
      /** @todo */
      free(rt);
    }
}

