/*
 * libbgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2013 The Regents of the University of California.
 *
 * This file is part of libbgpstream.
 *
 * libbgpstream is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libbgpstream is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bgpstream_feeder_plugins.h"
#include "debug.h"
#include <sqlite3.h>
#include <unistd.h> // sleep
#include <stdlib.h>



static int file_read_by_cb = 0;

int feeder_default(bgpstream_input_mgr_t * const input_mgr) {
  debug("\t\tBSI: callback start");
  int query_results = 0;
  /* DO SOMETHING i.e.: populate input_mgr with new data
   * and return the number of data inserted */
  if(file_read_by_cb == 0) { 
    // populate 
    query_results += bgpstream_input_push_sorted_input(input_mgr,
    						       "./latest-bview.gz", "ribs", 1);
    query_results += bgpstream_input_push_sorted_input(input_mgr,
    						       "./updates.example.bz2", "updates", 0);
    query_results += bgpstream_input_push_sorted_input(input_mgr,
    						       "./latest-update.gz", "updates", 2);
    query_results += bgpstream_input_push_sorted_input(input_mgr,
    						       "./another_rib.bz2", "ribs", 2);
    debug("\t\tBSI: added %d new inputs to input queue", query_results);
  }
  if(file_read_by_cb == 1) { 
    // populate 
    //bgpstream_input_push_input(input_mgr, "./updates.example.bz2", "updates", 1); 
    query_results = 1;
  }
  if(file_read_by_cb > 1) {
    query_results = 0;
  }
  file_read_by_cb++; 
  debug("\t\tBSI: callback called");
  return query_results;
}


typedef struct struct_sqlite_callback_parameter_t {
  bgpstream_input_mgr_t * imgr;
  int input_added;
} sqlite_callback_parameter_t;

static int pushsqliteresults(void *input_struct, int argc, char **argv, char **azColName){
  int i;
  char filepath[BGPSTREAM_MAX_FILE_LEN];
  char filetype[BGPSTREAM_MAX_TYPE_LEN];
  int epoch_filetime;
  int epoch_ts;
  for(i=0; i<argc; i++){
    debug("\t\t\t%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
    if(strcmp(azColName[i],"filepath") == 0) {
      strcpy(filepath, argv[i] ? argv[i] : "NULL");
    }
    if(strcmp(azColName[i],"filetype") == 0) {
      strcpy(filetype, argv[i] ? argv[i] : "NULL");
    }
    if(strcmp(azColName[i],"filetime") == 0) {
      epoch_filetime = atoi(argv[i]);     
    }
    if(strcmp(azColName[i],"ts") == 0) {
      epoch_ts = atoi(argv[i]);
    }
  }
  sqlite_callback_parameter_t *scp = (sqlite_callback_parameter_t*)input_struct;
  // update the last ts input to the most recent value
  if(scp->imgr->epoch_last_ts_input < epoch_ts) {
    scp->imgr->epoch_last_ts_input = epoch_ts;
  }
  // insert new values and record the number of added inputs
  int inserted = bgpstream_input_push_sorted_input(scp->imgr, filepath, filetype,
						   epoch_filetime);
  scp->input_added += inserted;
  
  return 0;
}


int sqlite_feeder(bgpstream_input_mgr_t * const input_mgr) {
  debug("\t\tBSI: sqlite callback start");
  int query_results = 0;  
  // Opening connection to sqlite database
  sqlite3 *db;
  char *zErrMsg = 0;
  int rc = sqlite3_open(input_mgr->feeder_name, &db);
  if(rc){
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 0;
  }
  sleep(1); // new db entries will be at least one second older than
  // the latest entry read during this DB connection
  /* Creating sql query
   * select filepath,filetype,filetime
   * from downloaded_bgp_data
   * where filetime >= 1388534400 and ts > 0
   * group by filetime,filetype; */
  char query[BGPSTREAM_MAX_FILE_LEN];
  sprintf(query, "select filepath,filetype, filetime, ts \
                  from downloaded_bgp_data \
                  where filetime >= %d and ts > %d \
                  group by filetime,filetype", 
	  input_mgr->epoch_minimum_date, input_mgr->epoch_last_ts_input);
  // creating callback parameter to insert sql results int
  // the input queue and save the number of inputs inserted
  sqlite_callback_parameter_t * scp;
  scp = (sqlite_callback_parameter_t*) malloc(sizeof(sqlite_callback_parameter_t));
  if(scp == NULL) {
    sqlite3_close(db);
    return 0; // can't allocate memory for scp
  }
  scp->imgr = input_mgr;
  scp->input_added = 0; // number of input added
  // running query and callback
  debug("\t\tBSI: sqlite query results");
  rc = sqlite3_exec(db, query, pushsqliteresults, (void*)scp, &zErrMsg);
  if( rc!=SQLITE_OK ){
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
  }
  // recording query results successfully added
  
  query_results = scp->input_added;
  // cleaning up
  scp->imgr = NULL;
  scp->input_added = 0;
  free(scp);
  // closing database connection
  sqlite3_close(db);
  debug("\t\tBSI: sqlite callback called");
  return query_results;
}


