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

#include "bgpribs_collectors_table.h"
#ifdef WITH_BGPWATCHER
#include "bgpribs_bgpwatcher_client.h"
#endif



collectors_table_wrapper_t *collectors_table_create() 
{  
  collectors_table_wrapper_t *collectors_table;
  if((collectors_table = malloc_zero(sizeof(collectors_table_wrapper_t))) == NULL)
    {
      return NULL;
    }
  if((collectors_table->table = kh_init(collectors_table_t)) == NULL)
    {
      free(collectors_table);
      return NULL;
    }
  return collectors_table;
}


/* returns 0 if everything is fine */
int collectors_table_process_record(collectors_table_wrapper_t *collectors_table,
				    bgpstream_record_t * bs_record)
{
  khiter_t k;
  char *collector_name_cpy = NULL;
  collectordata_t * collector_data;
  int khret;

  /* check if the collector associated with the bs_record
   * already exists, if not create a collectordata object, 
   * then pass the bs_record to the collectordata object */
  if((k = kh_get(collectors_table_t, collectors_table->table,
		 bs_record->attributes.dump_collector)) ==
     kh_end(collectors_table->table))
    {
      /* create a new collectordata structure */
      if((collector_data =
	collectordata_create(bs_record->attributes.dump_project,
			     bs_record->attributes.dump_collector)) == NULL) 
	{
	  // can't create collectordata
	  return -1;
	}
      /* copy the name of the collector */
      if((collector_name_cpy = strdup(bs_record->attributes.dump_collector)) == NULL)
	{
	  // can't malloc memory for collector name
	  return -1;
	}
      /* add it (key/name) to the hash */
      k = kh_put(collectors_table_t, collectors_table->table,
		 collector_name_cpy, &khret);
      /* add collectordata (value) to the hash */
      kh_value(collectors_table->table, k) = collector_data;
    }
  else
    {
      /* already exists, just get it */
      collector_data = kh_value(collectors_table->table, k);
    }
  // provide the bs_record to the right collectordata structure
  return collectordata_process_record(collector_data, bs_record);
} 


/* dump statistics for each collector */
#ifdef WITH_BGPWATCHER
int collectors_table_interval_end(collectors_table_wrapper_t *collectors_table,
				   int interval_processing_start,
				   int interval_start, int interval_end,
				   bw_client_t *bw_client)
#else
int collectors_table_interval_end(collectors_table_wrapper_t *collectors_table,
				   int interval_processing_start,
				   int interval_start, int interval_end)
#endif
{
  assert(collectors_table != NULL); 
  khiter_t k;
  collectordata_t * collector_data;
  int ret = 0;
  for (k = kh_begin(collectors_table->table);
       k != kh_end(collectors_table->table); ++k)
    {
      if (kh_exist(collectors_table->table, k))
	{
	  collector_data = kh_value(collectors_table->table, k);
	  // if the collector is in an unknown status we do not output
	  // information, this way we can merge different runs on our
	  // time series database
	  if(collector_data->status != COLLECTOR_NULL)
	    {
#ifdef WITH_BGPWATCHER
	      ret = collectordata_interval_end(collector_data,interval_start, bw_client);
#else
	      ret = collectordata_interval_end(collector_data,interval_start);
#endif
	      if(ret < 0)
		{
		  // something went wrong during the end of interval
		  return -1;
		}
	    }
#ifdef WITH_BGPWATCHER
	  else
	    {
	      // send an empty peer table (the client is already registered
	      // it is sending information, saying that no data are available
	      uint32_t peer_table_time = interval_start;
	      bgpwatcher_client_peer_table_begin(bw_client->peer_table, peer_table_time);
	      bgpwatcher_client_peer_table_end(bw_client->peer_table);
	    }
#endif
	}
    }

  // if there is only 1 collector, then output its processing statistics
  // otherwise use the word "multiple"
  char collector_str[64];
  collector_str[0] = '\0';
  if(kh_size(collectors_table->table) == 1)
    {
      for(k = kh_begin(collectors_table->table);
	  k != kh_end(collectors_table->table); ++k)
	{
	  if (kh_exist(collectors_table->table, k))
	    {
	      collector_data = kh_value(collectors_table->table, k);
	      strcat(collector_str,collector_data->dump_project);
	      strcat(collector_str,".");
	      strcat(collector_str,collector_data->dump_collector);
	      break;
	    }
	}
    }
  else
    {
      strcat(collector_str,"multiple");
    }

  // OUTPUT METRIC: Interval processing time
  time_t now = time(NULL); 
  fprintf(stdout,
	  METRIC_PREFIX".%s.interval_processing_time %ld %d\n",
	  collector_str, (now - interval_processing_start),
	  interval_start);

  return 0;

} 


void collectors_table_destroy(collectors_table_wrapper_t *collectors_table) 
{
  khiter_t k;
  if(collectors_table != NULL) 
    {
      /* free all values in the  collectors_table hash */
      for (k = kh_begin(collectors_table->table);
	   k != kh_end(collectors_table->table); ++k)
	{
	  if (kh_exist(collectors_table->table, k))
	    {
	      /* free the value */
	      collectordata_destroy(kh_value(collectors_table->table, k));
	    }
	}   
      kh_destroy(collectors_table_t, collectors_table->table);
      collectors_table->table = NULL;
      free(collectors_table);
    }
}
