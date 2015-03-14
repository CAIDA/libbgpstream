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


/* ========== PRIVATE FUNCTIONS ========== */

static char *
graphite_safe(char *p)
{
  if(p == NULL)
    {
      return p;
    }

  char *r = p;
  while(*p != '\0')
    {
      if(*p == '.')
	{
	  *p = '_';
	}
      if(*p == '*')
	{
	  *p = '-';
	}
      p++;
    }
  return r;
}

static uint32_t
get_wall_time_now()
{
  struct timeval tv;
  gettimeofday_wrap(&tv);
  return tv.tv_sec;
}

// static
perpfx_perpeer_info_t *
perpfx_perpeer_info_create(uint32_t bgp_time_last_ts,
                           uint16_t bgp_time_uc_delta_ts,
                           uint32_t uc_origin_asn)
{
  perpfx_perpeer_info_t *pfxpeeri = (perpfx_perpeer_info_t *) malloc_zero(sizeof(perpfx_perpeer_info_t));
  if(pfxpeeri != NULL)
    {
      pfxpeeri->bgp_time_last_ts = bgp_time_last_ts;
      pfxpeeri->bgp_time_uc_delta_ts = bgp_time_uc_delta_ts;
      pfxpeeri->uc_origin_asn = uc_origin_asn;
    }
  return pfxpeeri;
}

// static
perpeer_info_t *
perpeer_info_create(uint32_t peer_asnumber,
                    bgpstream_ip_addr_t *peer_ip,
                    bgpstream_elem_peerstate_t bgp_fsm_state,
                    uint32_t bgp_time_ref_rib_start, uint32_t bgp_time_ref_rib_end,
                    uint32_t bgp_time_uc_rib_start,  uint32_t bgp_time_uc_rib_end)
{
  char ip_str[INET_ADDRSTRLEN];
  perpeer_info_t *peeri = (perpeer_info_t *) malloc_zero(sizeof(perpeer_info_t));
  if(peeri != NULL)
    {
      bgpstream_addr_ntop(ip_str, INET_ADDRSTRLEN, peer_ip);
      graphite_safe(ip_str);  
      if(snprintf(peeri->peer_str, BGPSTREAM_UTILS_STR_NAME_LEN,
                  "%"PRIu32".%s", peer_asnumber, ip_str) >= BGPSTREAM_UTILS_STR_NAME_LEN)
        {
          fprintf(stderr, "Warning: could not print peer signature: truncated output\n");
        }
      peeri->bgp_fsm_state = bgp_fsm_state;
      peeri->bgp_time_ref_rib_start = bgp_time_ref_rib_start;
      peeri->bgp_time_ref_rib_end = bgp_time_ref_rib_end;
      peeri->bgp_time_uc_rib_start = bgp_time_uc_rib_start;
      peeri->bgp_time_uc_rib_end = bgp_time_uc_rib_end;
    }
  return peeri;
}

// static
perview_info_t *perview_info_create()
{
  perview_info_t *viewi = (perview_info_t *) malloc_zero(sizeof(perview_info_t));
  if(viewi != NULL)
    {
      /** @todo initialize state variables here */
    }
  return viewi;
}

/** @note:
 *  all the xxx_info_create functions do not allocate dynamic
 *  memory other than the structure itself, therefore free() 
 *  is enough to dealloc safely their memory.
 */



/** @note:
 *  In order to save memory we can use the reserved AS numbers
 *  to embed other informations associated with an AS number, i.e.:
 *  - origin as = 0 -> prefix not seen in the RIB
 *  - AS in [64496-64511] -> AS is actually an AS set
 *  - AS in [64512-65534] -> AS is actually an AS confederation  
 */
/* static void asn_mgmt_fun() */
/* { */
  /* http://www.iana.org/assignments/as-numbers/as-numbers.xhtml */
  /* 0	           Reserved */
  /* 64198-64495   Reserved by the IANA */ 
  /* 23456	   AS_TRANS RFC6793 */
  /* 64496-64511   Reserved for use in documentation and sample code RFC5398 */
  /* 64512-65534   Reserved for Private Use RFC6996 */
  /* 65535	   Reserved RFC7300 */
/* } */

static collector_t *
get_collector_data(collector_data_t *collectors, char *project, char *collector)
{
  khiter_t k;
  int khret;
  collector_t c_data;
  
  // create new collector-related structures if it is the first time
  // we see it
  if((k = kh_get(collector_data, collectors, collector))
     == kh_end(collectors))
    {

      // collector data initialization
      
      char project_name[BGPSTREAM_UTILS_STR_NAME_LEN];
      strncpy(project_name, project, BGPSTREAM_UTILS_STR_NAME_LEN);
      graphite_safe(project_name);  

      char collector_name[BGPSTREAM_UTILS_STR_NAME_LEN];
      strncpy(collector_name, collector, BGPSTREAM_UTILS_STR_NAME_LEN);
      graphite_safe(collector_name);  

      if(snprintf(c_data.collector_str , BGPSTREAM_UTILS_STR_NAME_LEN,
                  "%s.%s", project_name, collector_name) >= BGPSTREAM_UTILS_STR_NAME_LEN)
        {
          fprintf(stderr, "Warning: could not print collector signature: truncated output\n");
        }
      
      if((c_data.collector_peerids = bgpstream_id_set_create()) == NULL)
        {
          return NULL;
        }

      c_data.bgp_time_last = 0;
      c_data.wall_time_last = 0;
      c_data.bgp_time_ref_rib_dump_time = 0;
      c_data.bgp_time_ref_rib_start_time = 0;
      c_data.bgp_time_uc_rib_dump_time = 0;
      c_data.state = ROUTINGTABLES_COLLECTOR_STATE_UNKNOWN;
      
      k = kh_put(collector_data, collectors, strdup(collector), &khret);
      kh_val(collectors,k) = c_data;
    }
  
  return &kh_val(collectors,k);
}


  
static int collector_process_valid_bgpinfo(collector_t *col,
                                           bgpstream_record_t *record,
                                           bgpstream_elem_t *elem)
{

  // @todo: delete  this test functions calls
  /* perpfx_perpeer_info_create(0 /\* last_ts *\/, 0 /\* uc_delta_ts *\/, 0 /\* uc_origin_asn *\/); */
  /* perpeer_info_create(0 /\* peer_asnumber *\/, NULL /\* peer_ip *\/, */
  /*                     BGPSTREAM_ELEM_PEERSTATE_UNKNOWN /\* bgp_fsm_state *\/, */
  /*                     0 /\* bgp_time_ref_rib_start *\/, 0 /\* bgp_time_ref_rib_end *\/, */
  /*                     0 /\* bgp_time_uc_rib_start *\/,  0 /\* bgp_time_uc_rib_end *\/); */
  
  /* perview_info_create(); */
  
  return 0;
}




/* ========== PUBLIC FUNCTIONS ========== */

routingtables_t *routingtables_create()
{  
  routingtables_t *rt = (routingtables_t *)malloc_zero(sizeof(routingtables_t));
  if(rt == NULL)
    {
      goto err;
    }

  if((rt->peersigns = bgpstream_peer_sig_map_create() ) == NULL)
    {
      goto err;
    }

  if((rt->view = bgpwatcher_view_create_shared(rt->peersigns,
                                               free /* view user destructor */,
                                               NULL /* pfx destructor */,
                                               free /* peer user destructor */,
                                               free /* pfxpeer user destructor */)) == NULL)
    {
      goto err;
    }

  if((rt->collectors = kh_init(collector_data)) == NULL)
    {
      goto err;
    }

  // set the metric prefix string to the default value
  routingtables_set_metric_prefix(rt,
                                  ROUTINGTABLES_DEFAULT_METRIC_PFX);

  // set the ff thresholds to their default values
  rt->ipv4_fullfeed_th = ROUTINGTABLES_DEFAULT_IPV4_FULLFEED_THR;
  rt->ipv6_fullfeed_th = ROUTINGTABLES_DEFAULT_IPV6_FULLFEED_THR;

  rt->bgp_time_interval_start = 0;
  rt->bgp_time_interval_end = 0;
  rt->wall_time_interval_start = 0;
    
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
  rt->tables_mask = ROUTINGTABLES_ALL_FEEDS; // default: all feeds 
  if(tables_mask != 0)
    {
      rt->tables_mask = tables_mask;
    }

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
  rt->bgp_time_interval_start = (uint32_t) start_time;
  rt->wall_time_interval_start = get_wall_time_now();
  return 0;
}

int routingtables_interval_end(routingtables_t *rt,
                               int end_time)
{
  rt->bgp_time_interval_end = (uint32_t) end_time;
  uint32_t elapsed_time = get_wall_time_now() - rt->wall_time_interval_start;
  fprintf(stderr, "Interval [%"PRIu32", %"PRIu32"] processed in %"PRIu32"s\n",
          rt->bgp_time_interval_start, rt->bgp_time_interval_end, elapsed_time);

  /** @todo: print statistics and send the view to the watcher if tx is on */

  return 0;
}

int routingtables_process_record(routingtables_t *rt,
                                 bgpstream_record_t *record)
{
  int ret = 0;
  collector_t *c;
  bgpstream_elem_t *elem;
  
  /* get a pointer to the current collector data, if no data
   * exists yet, a new structure will be created */
  if((c = get_collector_data(rt->collectors,
                             record->attributes.dump_project,
                             record->attributes.dump_collector)) == NULL)
    {
      return -1;
    }

  /* if a record refer to a time prior to the current reference time,
   * then we discard it */
  if(record->attributes.record_time < c->bgp_time_ref_rib_start_time)
    {
      return 0;
    }
  
  switch(record->status)
    {
    case BGPSTREAM_RECORD_STATUS_VALID_RECORD:
      while((elem = bgpstream_record_get_next_elem(record)) != NULL)
        {
          /* @todo add some logic and think how to interpret the return value */
          ret = collector_process_valid_bgpinfo(c, record, elem);
        }
      break;
    case BGPSTREAM_RECORD_STATUS_CORRUPTED_SOURCE:
    case BGPSTREAM_RECORD_STATUS_CORRUPTED_RECORD:
      
      /* @todo invalidate data for *active* and check whether
      *  the under construction should be invalidated too */
      if(record->attributes.record_time < c->bgp_time_last)
        {
          c->bgp_time_last = record->attributes.record_time;
        }
      break;
    case BGPSTREAM_RECORD_STATUS_FILTERED_SOURCE:
    case BGPSTREAM_RECORD_STATUS_EMPTY_SOURCE:
      /** An empty or filtered source does not change the current
       *  state of a collector, however we update the last_ts
       *  observed */
      if(record->attributes.record_time < c->bgp_time_last)
        {
          c->bgp_time_last = record->attributes.record_time;
        }
      break;
    default:
      /* programming error */
      assert(0);
    }
  
  fprintf(stderr, "Processed %s record %ld\n",
          c->collector_str,
          record->attributes.record_time);
  
  return 0;
}

void routingtables_destroy(routingtables_t *rt)
{
  khiter_t k;
  if(rt != NULL)
    {
      if(rt->collectors != NULL)
        {
          for (k = kh_begin(rt->collectors);
               k != kh_end(rt->collectors); ++k)
            {          
              if (kh_exist(rt->collectors, k))
                {
                  /* deallocating value dynamic memory */
                  bgpstream_id_set_destroy(kh_val(rt->collectors, k).collector_peerids);
                  kh_val(rt->collectors, k).collector_peerids = NULL;
                  /* deallocating string dynamic memory */
                  free(kh_key(rt->collectors, k));
                }
            }   
          kh_destroy(collector_data, rt->collectors );
        }
      rt->collectors = NULL;

      if(rt->view != NULL)
        {
          bgpwatcher_view_destroy(rt->view);
        }
      rt->view =NULL;
        
      if(rt->peersigns != NULL)
        {
          bgpstream_peer_sig_map_destroy(rt->peersigns);
        }
      rt->peersigns = NULL;      

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

