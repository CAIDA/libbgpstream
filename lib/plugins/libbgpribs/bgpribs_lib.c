/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#include "bgpribs_lib.h"
#include "bgpribs_int.h"

#include <assert.h>
#include "utils.h"
#include <time.h>

#include "config.h"
#ifdef WITH_BGPWATCHER
#include "bgpribs_bgpwatcher_client.h"
#endif


bgpribs_t *bgpribs_create(char *metric_pfx)
{
  bgpribs_t *bgp_ribs;
  if((bgp_ribs = malloc_zero(sizeof(bgpribs_t))) == NULL)
    {
      return NULL;
    }
  bgp_ribs->interval_start = 0;
  bgp_ribs->interval_end = 0;
  bgp_ribs->interval_processing_start = 0;
  if((bgp_ribs->collectors_table = collectors_table_create()) == NULL)
    {
      bgpribs_destroy(bgp_ribs);
      return NULL;
    }
  if((bgp_ribs->metric_pfx = strdup(metric_pfx)) == NULL)
    {
      bgpribs_destroy(bgp_ribs);
      return NULL;
    }
  
#ifdef WITH_BGPWATCHER
  if((bgp_ribs->bw_client = bw_client_create()) == NULL)
    {
      bgpribs_destroy(bgp_ribs);
      return NULL;
    }
#endif

  return bgp_ribs;
}


void bgpribs_set_metric_pfx(bgpribs_t *bgp_ribs, char* met_pfx)
{
  if(bgp_ribs->metric_pfx != NULL)
    {
      free(bgp_ribs->metric_pfx);
    }
  bgp_ribs->metric_pfx = strdup(met_pfx);
}


#ifdef WITH_BGPWATCHER
int bgpribs_set_watcher(bgpribs_t *bgp_ribs)
{
  return bw_client_start(bgp_ribs->bw_client);
}
#endif


void bgpribs_interval_start(bgpribs_t *bgp_ribs, int interval_start)
{
  bgp_ribs->interval_start = interval_start;
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/time.html
  time_t now = time(NULL);
  bgp_ribs->interval_processing_start = now; // uintmax_t
}


int bgpribs_process_record(bgpribs_t *bgp_ribs, bgpstream_record_t *bs_record)
{
  return collectors_table_process_record(bgp_ribs->collectors_table, bs_record);
}


int bgpribs_interval_end(bgpribs_t *bgp_ribs, int interval_end)
{
  bgp_ribs->interval_end = interval_end;
#ifdef WITH_BGPWATCHER
  return collectors_table_interval_end(bgp_ribs->collectors_table, 
				       bgp_ribs->interval_processing_start,
				       bgp_ribs->interval_start,
				       bgp_ribs->interval_end,
				       bgp_ribs->metric_pfx,
				       bgp_ribs->bw_client);
#else
  return collectors_table_interval_end(bgp_ribs->collectors_table, 
				       bgp_ribs->interval_processing_start,
				       bgp_ribs->interval_start,
				       bgp_ribs->interval_end,
				       bgp_ribs->metric_pfx);
#endif
}


void bgpribs_destroy(bgpribs_t *bgp_ribs)
{
  bgp_ribs->interval_start = 0;
  bgp_ribs->interval_end = 0;
  bgp_ribs->interval_processing_start = 0;
  if(bgp_ribs->collectors_table != NULL)
    {
      collectors_table_destroy(bgp_ribs->collectors_table);
      bgp_ribs->collectors_table = NULL;
    }
  if(bgp_ribs->metric_pfx != NULL)
    {
      free(bgp_ribs->metric_pfx);
      bgp_ribs->metric_pfx = NULL;
    }
#ifdef WITH_BGPWATCHER
  if(bgp_ribs->bw_client != NULL)
    {
      bw_client_destroy(bgp_ribs->bw_client);
      bgp_ribs->bw_client = NULL;
    }     
#endif
  free(bgp_ribs);
}



