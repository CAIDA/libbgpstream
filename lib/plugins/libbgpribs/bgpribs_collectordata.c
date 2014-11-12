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

#include "bgpribs_collectordata.h"



collectordata_t *collectordata_create(const char *project,
				      const char *collector)
{
  collectordata_t *collector_data;
  assert(project != NULL);
  assert(collector != NULL);

  if((collector_data = malloc_zero(sizeof(collectordata_t))) == NULL)
    {
      return NULL;
    }
  /* all data are set to zero by malloc_zero, thereby:
   *  - most_recent_ts = 0
   *  - status = 0 = COLLECTOR_NULL
   *  - record_types = [0,0,0,0,0]
   */

  if((collector_data->dump_project = strdup(project)) == NULL)
    {
      free(collector_data);
      return NULL;
    }

  if((collector_data->dump_collector = strdup(collector)) == NULL)
    {
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }

  if((collector_data->peers_table = peers_table_create()) == NULL)
    {
      free(collector_data->dump_collector);
      collector_data->dump_collector = NULL;
      free(collector_data->dump_project);
      collector_data->dump_project = NULL;
      free(collector_data);
      return NULL;
    }

  // aggregatable data
  if((collector_data->aggr_stats = malloc_zero(sizeof(aggregated_bgp_stats_t))) == NULL)
    {
      peers_table_destroy(collector_data->peers_table);
      collector_data->peers_table = NULL;
      free(collector_data->dump_collector);
      collector_data->dump_collector = NULL;
      free(collector_data->dump_project);
      collector_data->dump_project = NULL;
      free(collector_data);
      return NULL;
    }

  // TODO: fix this (it may cause leaks)
  if((collector_data->aggr_stats->unique_ipv4_prefixes = bl_ipv4_pfx_set_create()) == NULL ||
     (collector_data->aggr_stats->unique_ipv6_prefixes = bl_ipv6_pfx_set_create()) == NULL ||
     (collector_data->aggr_stats->affected_ipv4_prefixes = bl_ipv4_pfx_set_create()) == NULL ||
     (collector_data->aggr_stats->affected_ipv6_prefixes = bl_ipv6_pfx_set_create()) == NULL ||
     (collector_data->aggr_stats->unique_origin_ases = bl_id_set_create()) == NULL ||
     (collector_data->aggr_stats->announcing_origin_ases = bl_id_set_create()) == NULL )
    {
      free(collector_data->aggr_stats);
      collector_data->aggr_stats = NULL;
      peers_table_destroy(collector_data->peers_table);
      collector_data->peers_table = NULL;
      free(collector_data->dump_collector);
      free(collector_data->dump_project);
      free(collector_data);
      return NULL;
    }
 
  /* make the project name graphite-safe */
  graphite_safe(collector_data->dump_project);

  /* make the collector name graphite-safe */
  graphite_safe(collector_data->dump_collector);
  
  return collector_data;
}


int collectordata_process_record(collectordata_t *collector_data,
				 bgpstream_record_t * bs_record)
{
  assert(collector_data);
  assert(bs_record);
  
  // register what kind of record types we receive
  collector_data->record_types[bs_record->status]++;

  // update the most recent timestamp
  if(bs_record->attributes.record_time > collector_data->most_recent_ts) 
    {
      collector_data->most_recent_ts = bs_record->attributes.record_time;
    }

  /* send the record to the peers_table and get the number
   * of active peers */
  collector_data->active_peers = 
    peers_table_process_record(collector_data->peers_table, bs_record);

  // some error occurred during the computation
  if(collector_data->active_peers < 0) 
    {
      return -1;
    }
   
  // some peers are active => the collector is active too 
  if(collector_data->active_peers > 0)
    {
      collector_data->status = COLLECTOR_UP;    
    }
  else
    { // assert(collector_data->active_peers == 0) 
      // collector was in unknown state => it remains there
      if(collector_data->status == COLLECTOR_NULL)
	{
	  collector_data->status = COLLECTOR_NULL;
	}
      else // collector was in known state
	{  // now, no peer is active => the collector must be DOWN      
	  collector_data->status = COLLECTOR_DOWN;    
	}
    }
  return 0;
}


#ifdef WITH_BGPWATCHER
int collectordata_interval_end(collectordata_t *collector_data, 
			       int interval_start, bw_client_t *bw_client)
#else
int collectordata_interval_end(collectordata_t *collector_data, 
			       int interval_start)
#endif
{
  assert(collector_data);

  // OUTPUT METRIC: status
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_status %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  // (collector_data->status-1) => { -1 NULL, 0 DOWN, 1 UP }
	  (collector_data->status - 1),
	  interval_start);

  // OUTPUT METRIC: active_peers
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.active_peers_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->active_peers,
	  interval_start);


  // OUTPUT METRIC: record_types[]
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_valid_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[VALID_RECORD],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_filtered_source_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[FILTERED_SOURCE],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_empty_source_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[EMPTY_SOURCE],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_corrupted_source_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[CORRUPTED_SOURCE],
	  interval_start);
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.record_corrupted_record_cnt %"PRIu64" %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  collector_data->record_types[CORRUPTED_RECORD],
	  interval_start);
  // reset record type array
  memset(collector_data->record_types, 0, sizeof(collector_data->record_types));

  // the following function call the peer interval_end functions for 
  // each peer and populate the collector_data aggregated stats
  int ret = 0;
#ifdef WITH_BGPWATCHER
  ret = peers_table_interval_end(collector_data->dump_project, collector_data->dump_collector,
				 collector_data->peers_table,
				 collector_data->aggr_stats,
				 bw_client,
				 interval_start);
#else
  ret = peers_table_interval_end(collector_data->dump_project, collector_data->dump_collector,
				 collector_data->peers_table,
				 collector_data->aggr_stats,
				 interval_start);
#endif
  if(ret < 0)
    {
      // something went wrong during the peer_table end of interval call
      return -1;
    }
  
  // OUTPUT METRIC: collector_affected_ipv4_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_affected_ipv4_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->aggr_stats->affected_ipv4_prefixes),
	  interval_start);
  bl_ipv4_pfx_set_reset(collector_data->aggr_stats->affected_ipv4_prefixes);

  // OUTPUT METRIC: collector_affected_ipv6_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_affected_ipv6_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->aggr_stats->affected_ipv6_prefixes),
	  interval_start);
  bl_ipv6_pfx_set_reset(collector_data->aggr_stats->affected_ipv6_prefixes);

  // OUTPUT METRIC: collector_announcing_origin_ases_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_announcing_origin_ases_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->aggr_stats->announcing_origin_ases),
	  interval_start);
  bl_id_set_reset(collector_data->aggr_stats->announcing_origin_ases);


  // OUTPUT METRIC: unique_ipv4_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_unique_ipv4_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->aggr_stats->unique_ipv4_prefixes),
	  interval_start);
  bl_ipv4_pfx_set_reset(collector_data->aggr_stats->unique_ipv4_prefixes);

  // OUTPUT METRIC: unique_ipv6_prefixes_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_unique_ipv6_prefixes_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->aggr_stats->unique_ipv6_prefixes),
	  interval_start);
  bl_ipv6_pfx_set_reset(collector_data->aggr_stats->unique_ipv6_prefixes);


  // OUTPUT METRIC: unique_std_origin_ases_cnt
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_unique_std_origin_ases_cnt %d %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  kh_size(collector_data->aggr_stats->unique_origin_ases),
	  interval_start);
  
  bl_id_set_reset(collector_data->aggr_stats->unique_origin_ases);


  // Note: this metric has to be the last one
  // so it embeds the processing time at the end 
  // of each interval

  // OUTPUT METRIC: collector_realtime_delay
  time_t now = time(NULL); 
  fprintf(stdout,
	  METRIC_PREFIX".%s.%s.collector_realtime_delay %ld %d\n",
	  collector_data->dump_project,
	  collector_data->dump_collector,
	  // difference between last record processed and now
	  (now - collector_data->most_recent_ts),
	  interval_start); 

  return 0;
}


void collectordata_destroy(collectordata_t *collector_data)
{
  if(collector_data != NULL)
    {
      if(collector_data->dump_project != NULL)
	{
	  free(collector_data->dump_project);
	  collector_data->dump_project = NULL;
	}
      if(collector_data->dump_collector != NULL)
	{
	  free(collector_data->dump_collector);
	  collector_data->dump_collector = NULL;
	}
      if(collector_data->peers_table != NULL)
	{
	  peers_table_destroy(collector_data->peers_table);
	  collector_data->peers_table = NULL;
	}
      if(collector_data->aggr_stats != NULL)
	{
	  // TODO fix this later
	  bl_ipv4_pfx_set_destroy(collector_data->aggr_stats->unique_ipv4_prefixes);
	  bl_ipv6_pfx_set_destroy(collector_data->aggr_stats->unique_ipv6_prefixes);
	  
	  bl_ipv4_pfx_set_destroy(collector_data->aggr_stats->affected_ipv4_prefixes);
	  bl_ipv6_pfx_set_destroy(collector_data->aggr_stats->affected_ipv6_prefixes);

	  bl_id_set_destroy(collector_data->aggr_stats->unique_origin_ases);
	  bl_id_set_destroy(collector_data->aggr_stats->announcing_origin_ases);
	  
	  free(collector_data->aggr_stats);
	  collector_data->aggr_stats = NULL;
	}
      free(collector_data);
    }
}

