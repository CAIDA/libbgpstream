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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "utils.h"

#include "routingtables.h"
#include "routingtables_int.h"


routingtables_t *routingtables_create()
{  
  routingtables_t *rt = (routingtables_t *)malloc_zero(sizeof(routingtables_t));
  if(rt == NULL)
    {
      goto err;
    }
  if((rt->collector_id_map = kh_init(str_id_map)) == NULL)
    {
      goto err;
    }

  // make sure all collectors point to NULL
  for(int c = 0; c < ROUTINGTABLES_MAX_COLLECTORS; c++)
    {
      rt->collectors[c] = NULL;
    }

  // set the metric prefix string to the default value
  routingtables_set_metric_prefix(rt,
                                  ROUTINGTABLES_DEFAULT_METRIC_PFX);

  // set the ff thresholds to their default values
  rt->ipv4_fullfeed_th = ROUTINGTABLES_DEFAULT_IPV4_FULLFEED_THR;
  rt->ipv6_fullfeed_th = ROUTINGTABLES_DEFAULT_IPV6_FULLFEED_THR;
    
#ifdef WITH_BGPWATCHER
  rt->watcher_tx_on = 0;
  rt->watcher_client = NULL;
  rt->tables_mask = 0;
#endif  

  return rt;

 err:
  fprintf(stderr, "routingtables_create failed\n");
  routingtables_destroy(rt);
  return NULL;
}

void routingtables_set_metric_prefix(routingtables_t *rt,
                                     char *metric_prefix)
{
  if(metric_prefix == NULL ||
     strlen(metric_prefix)-1 > ROUTINGTABLES_METRIC_PFX_LEN)
    {
      fprintf(stderr,
              "Warning: could not set metric prefix, using default %s \n",
              ROUTINGTABLES_DEFAULT_METRIC_PFX);
      strcpy(rt->metric_prefix, ROUTINGTABLES_DEFAULT_METRIC_PFX);
      return;
    }
  strcpy(rt->metric_prefix, metric_prefix);
}

char *routingtables_get_metric_prefix(routingtables_t *rt)
{
  return &rt->metric_prefix[0];
}

#ifdef WITH_BGPWATCHER
int routingtables_activate_watcher_tx(routingtables_t *rt,
                                      char *client_name,
                                      char *server_uri,
                                      uint8_t tables_mask)
{

  if((rt->watcher_client = bgpwatcher_client_init(0 /* no interests */,
                                                  BGPWATCHER_PRODUCER_INTENT_PREFIX /* peers and pfxs*/
                                                  )) == NULL)
    {
      fprintf(stderr,
              "Error: could not initialize bgpwatcher client\n");
      return -1;
    }

  if(server_uri != NULL &&
     bgpwatcher_client_set_server_uri(rt->watcher_client, server_uri) != 0)
    {
      goto err;
    }

  if(client_name != NULL &&
     bgpwatcher_client_set_identity(rt->watcher_client, client_name) != 0)
    {
      fprintf(stderr,
              "Warning: could not set client identity to %s, using random ID\n",
              client_name);
    }
  
  if(bgpwatcher_client_start(rt->watcher_client) != 0)
    {
      fprintf(stderr,
              "Error: cannot start bgpwatcher client \n");
      goto err;
    }
  
  rt->watcher_tx_on = 1;
  
  /** @todo check if this assignment is legit,
   *  remember, tables_mask = 0 means all kinds of
   *  tables are sent */
  rt->tables_mask = tables_mask;

  return 0;

    err:
  if(rt->watcher_client != NULL)
    {
      bgpwatcher_client_perr(rt->watcher_client);
      bgpwatcher_client_free(rt->watcher_client);
    }
    rt->watcher_tx_on = 0;
    rt->watcher_client = NULL;
    return -1; 
}
#endif

void routingtables_set_fullfeed_threshold(routingtables_t *rt,
                                         bgpstream_addr_version_t ip_version,
                                         uint32_t threshold)
{
  switch(ip_version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      rt->ipv4_fullfeed_th = threshold;
      break;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      rt->ipv6_fullfeed_th = threshold;
      break;
    default:
      /* programming error */
      assert(0);      
    }
}

int routingtables_get_fullfeed_threshold(routingtables_t *rt,
                                         bgpstream_addr_version_t ip_version)
{
 switch(ip_version)
    {
    case BGPSTREAM_ADDR_VERSION_IPV4:
      return rt->ipv4_fullfeed_th;
    case BGPSTREAM_ADDR_VERSION_IPV6:
      return rt->ipv6_fullfeed_th;
    default:
      /* programming error */
      assert(0);      
    }
 return -1;
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
  // fprintf(stderr, "Processing record %ld\n", record->attributes.record_time);
  return 0;
}

void routingtables_destroy(routingtables_t *rt)
{
  khiter_t k;
  if(rt != NULL)
    {
      if(rt->collector_id_map != NULL)
        {
          /* free all strings in the collectors_id hash */
          for (k = kh_begin(rt->collector_id_map);
               k != kh_end(rt->collector_id_map); ++k)
            {          
              if (kh_exist(rt->collector_id_map, k))
                {
                  free(kh_key(rt->collector_id_map, k));
                }
            }   
          kh_destroy(str_id_map, rt->collector_id_map );
        }
      rt->collector_id_map = NULL;
      for(int c = 0; c < ROUTINGTABLES_MAX_COLLECTORS; c++)
        {
          if(rt->collectors[c] != NULL)
            {
              // @todo call the collector destructor
            }
          rt->collectors[c] = NULL;
        }
#ifdef WITH_BGPWATCHER
      if(rt->watcher_client != NULL)
        {
          bgpwatcher_client_stop(rt->watcher_client);
	  bgpwatcher_client_perr(rt->watcher_client);
	  bgpwatcher_client_free(rt->watcher_client);
        }
      rt->watcher_client = NULL;
#endif
      free(rt);
    }
}

