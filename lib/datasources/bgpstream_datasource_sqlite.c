/*
 * This file is part of bgpstream
 *
 * Copyright (C) 2015 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * All rights reserved.
 *
 * This code has been developed by CAIDA at UC San Diego.
 * For more information, contact bgpstream-info@caida.org
 *
 * This source code is proprietary to the CAIDA group at UC San Diego and may
 * not be redistributed, published or disclosed without prior permission from
 * CAIDA.
 *
 * Report any bugs, questions or comments to bgpstream-info@caida.org
 *
 */

#include "bgpstream_datasource_sqlite.h"
#include "bgpstream_debug.h"
#include "utils.h"

#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errmsg.h>
#include <assert.h>


struct struct_bgpstream_sqlite_datasource_t {
  bgpstream_filter_mgr_t * filter_mgr;
  /* sqlite connection handler */
  sqlite3 *db;
  sqlite3_stmt *stmt;
  char sql_query[2048];
  char * sqlite_file;
  uint32_t current_ts;
  uint32_t last_ts;
};


static int
prepare_db(bgpstream_sqlite_datasource_t * sqlite_ds)
{
  assert(sqlite_ds);
  int rc = 0;
  if(sqlite3_open_v2(sqlite_ds->sqlite_file, &sqlite_ds->db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
    {
      bgpstream_log_err("\t\tBSDS_SQLITE: can't open database: %s", sqlite3_errmsg(sqlite_ds->db));  
      sqlite3_close(sqlite_ds->db);
      return -1;
    }

  rc = sqlite3_prepare_v2(sqlite_ds->db, sqlite_ds->sql_query, -1, &sqlite_ds->stmt, NULL);
  if (rc != SQLITE_OK) {
    bgpstream_log_err("\t\tBSDS_SQLITE: failed to execute statement: %s", sqlite3_errmsg(sqlite_ds->db));
    return -1;
  }
  return 0;
}
  


bgpstream_sqlite_datasource_t *
bgpstream_sqlite_datasource_create(bgpstream_filter_mgr_t *filter_mgr,
                                   char * sqlite_file)
{
  bgpstream_debug("\t\tBSDS_SQLITE: create sqlite_ds start");  
  bgpstream_sqlite_datasource_t *sqlite_ds = (bgpstream_sqlite_datasource_t*) malloc_zero(sizeof(bgpstream_sqlite_datasource_t));
  if(sqlite_ds == NULL) {
    bgpstream_log_err("\t\tBSDS_SQLITE: create sqlite_ds can't allocate memory");
    goto err;
  }  
  if(sqlite_file == NULL)
    {
      bgpstream_log_err("\t\tBSDS_SQLITE: create sqlite_ds no file provided");
      goto err;
    }
  sqlite_ds->sqlite_file = strdup(sqlite_file);

  sqlite_ds->filter_mgr = filter_mgr;
  sqlite_ds->current_ts = 0;
  sqlite_ds->last_ts = 0;
  
  strcpy(sqlite_ds->sql_query,
         "SELECT bgp_data.file_path, collectors.project, collectors.name, "
         "bgp_types.name, time_span.time_span, bgp_data.file_time, bgp_data.ts "
         "FROM  collectors JOIN bgp_data JOIN bgp_types JOIN time_span " 
         "WHERE bgp_data.collector_id = collectors.id  AND "
         "bgp_data.collector_id = time_span.collector_id AND "
         "bgp_data.type_id = bgp_types.id AND "
         "bgp_data.type_id = time_span.bgp_type_id "
         );

  // projects, collectors, bgp_types, and time_intervals are used as filters
  // only if they are provided by the user
  bgpstream_string_filter_t * sf;
  bgpstream_interval_filter_t * tif;
  
  // projects
  if(filter_mgr->projects != NULL) {
    sf = filter_mgr->projects;
    strcat (sqlite_ds->sql_query," AND collectors.project IN (");
    while(sf != NULL) {
      strcat (sqlite_ds->sql_query, "'");
      strcat (sqlite_ds->sql_query, sf->value);
      strcat (sqlite_ds->sql_query, "'");
      sf = sf->next;
      if(sf!= NULL) {
	strcat (sqlite_ds->sql_query, ", ");      
      }
    }
    strcat (sqlite_ds->sql_query," )");
  }
  // collectors
  if(filter_mgr->collectors != NULL) {
    sf = filter_mgr->collectors;
    strcat (sqlite_ds->sql_query," AND collectors.name IN (");
    while(sf != NULL) {
      strcat (sqlite_ds->sql_query, "'");
      strcat (sqlite_ds->sql_query, sf->value);
      strcat (sqlite_ds->sql_query, "'");
      sf = sf->next;
      if(sf!= NULL) {
	strcat (sqlite_ds->sql_query, ", ");      
      }
    }
    strcat (sqlite_ds->sql_query," )");
  }
  // bgp_types
  if(filter_mgr->bgp_types != NULL) {
    sf = filter_mgr->bgp_types;
    strcat (sqlite_ds->sql_query," AND bgp_types.name IN (");
    while(sf != NULL) {
      strcat (sqlite_ds->sql_query, "'");
      strcat (sqlite_ds->sql_query, sf->value);
      strcat (sqlite_ds->sql_query, "'");
      sf = sf->next;
      if(sf!= NULL) {
	strcat (sqlite_ds->sql_query, ", ");      
      }
    }
    strcat (sqlite_ds->sql_query," )");
  }

  // time_intervals
  if(filter_mgr->time_intervals != NULL) {
    tif = filter_mgr->time_intervals;
    strcat (sqlite_ds->sql_query," AND ( ");
    while(tif != NULL) {
      strcat (sqlite_ds->sql_query," ( ");

      // BEGIN TIME
      strcat (sqlite_ds->sql_query," (bgp_data.file_time >=  ");
      sprintf(sqlite_ds->sql_query + strlen(sqlite_ds->sql_query),
              "%"PRIu32, tif->begin_time);
      strcat (sqlite_ds->sql_query,"  - time_span.time_span - 120 )");

      strcat (sqlite_ds->sql_query,"  AND  ");

      // END TIME
      strcat (sqlite_ds->sql_query," (bgp_data.file_time <=  ");
      sprintf(sqlite_ds->sql_query + strlen(sqlite_ds->sql_query),
              "%"PRIu32, tif->end_time);
      strcat (sqlite_ds->sql_query,") ");

      strcat (sqlite_ds->sql_query," ) ");
      tif = tif->next;
      if(tif!= NULL) {
	strcat (sqlite_ds->sql_query, " OR ");      
      }
    }
    strcat (sqlite_ds->sql_query," )");
  }
  
  /*  comment on 120 seconds: */
  /*  sometimes it happens that ribs or updates carry a filetime which is not */
  /*  compliant with the expected filetime (e.g. : */
  /*   rib.23.59 instead of rib.00.00 */
  /*  in order to compensate for this kind of situations we  */
  /*  retrieve data that are 120 seconds older than the requested  */

  // minimum timestamp and current timestamp are the two placeholders
  strcat (sqlite_ds->sql_query," AND bgp_data.ts > ? AND bgp_data.ts <= ?");

  // order by filetime and bgptypes in reverse order: this way the 
  // input insertions are always "head" insertions, i.e. queue insertion is
  // faster
  strcat(sqlite_ds->sql_query," ORDER BY file_time DESC, bgp_types.name DESC");

  if(prepare_db(sqlite_ds) != 0)
    {
      goto err;
    }

  /* printf("%s\n", sqlite_ds->sql_query); */
  
  bgpstream_debug("\t\tBSDS_SQLITE: create sqlite_ds end");

  return sqlite_ds;
 err:
  bgpstream_sqlite_datasource_destroy(sqlite_ds);
  return NULL;
}

int
bgpstream_sqlite_datasource_update_input_queue(bgpstream_sqlite_datasource_t* sqlite_ds,
                                                bgpstream_input_mgr_t *input_mgr)
{
  int rc;
  int num_results = 0;
  sqlite_ds->last_ts = sqlite_ds->current_ts;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  // update current_timestamp - we always ask for data 1 second old at least
  sqlite_ds->current_ts = tv.tv_sec - 1; // now() - 1 second
  
  sqlite3_bind_int(sqlite_ds->stmt, 1, sqlite_ds->last_ts);
  sqlite3_bind_int(sqlite_ds->stmt, 2, sqlite_ds->current_ts);

  /* printf("%d - %d \n", sqlite_ds->last_ts, sqlite_ds->current_ts); */
  while((rc = sqlite3_step(sqlite_ds->stmt)) != SQLITE_DONE)
    {
      if(rc == SQLITE_ROW)
        {
          /* printf("%s: %d\n", sqlite3_column_text(sqlite_ds->stmt, 0), sqlite3_column_int(sqlite_ds->stmt, 6)); */
          num_results +=
            bgpstream_input_mgr_push_sorted_input(input_mgr,
                                                  strdup((const char *)sqlite3_column_text(sqlite_ds->stmt, 0)) /* path */,
                                                  strdup((const char *)sqlite3_column_text(sqlite_ds->stmt, 1)) /* project */,
                                                  strdup((const char *)sqlite3_column_text(sqlite_ds->stmt, 2)) /* collector */,
                                                  strdup((const char *)sqlite3_column_text(sqlite_ds->stmt, 3)) /* type */,
                                                  sqlite3_column_int(sqlite_ds->stmt, 5) /* file time */,
                                                  sqlite3_column_int(sqlite_ds->stmt, 4) /* time span */ );
          
        }
      else
        {
          bgpstream_log_err("\t\tBSDS_SQLITE: error while stepping through results");
          return -1;
        }
    }
  sqlite3_reset(sqlite_ds->stmt);
  return num_results;
}


void
bgpstream_sqlite_datasource_destroy(bgpstream_sqlite_datasource_t* sqlite_ds)
{
  if(sqlite_ds != NULL)
    {
      if(sqlite_ds->sqlite_file != NULL)
        {
          free(sqlite_ds->sqlite_file);
          sqlite_ds->sqlite_file = NULL;
        }
      
      sqlite3_finalize(sqlite_ds->stmt);
      sqlite3_close(sqlite_ds->db);
      free(sqlite_ds);
    }
}

