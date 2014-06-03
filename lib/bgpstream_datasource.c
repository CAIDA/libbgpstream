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

#include "bgpstream_datasource.h"
#include "debug.h"



bgpstream_datasource_mgr_t *bgpstream_datasource_mgr_create(){
  debug("\tBSDS_MGR: create start");
  bgpstream_datasource_mgr_t *datasource_mgr = (bgpstream_datasource_mgr_t*) malloc(sizeof(bgpstream_datasource_mgr_t));
  if(datasource_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  // default values
  memset(datasource_mgr->datasource_name, 0, BGPSTREAM_DS_MAX_LEN);   
  strcpy(datasource_mgr->datasource_name, "mysql");   
  datasource_mgr->blocking = 0;
  // datasources (none of them is active)
  datasource_mgr->mysql_ds = NULL;
  datasource_mgr->status = DS_OFF;
  debug("\tBSDS_MGR: create end");
  return datasource_mgr;
}


void bgpstream_datasource_mgr_init(bgpstream_datasource_mgr_t *datasource_mgr,
				   const char *datasource_name,
				   bgpstream_filter_mgr_t *filter_mgr){
  debug("\tBSDS_MGR: init start");
  if(datasource_mgr == NULL) {
    return; // no manager
  }
  strcpy(datasource_mgr->datasource_name, datasource_name);   
  // datasource_mgr->blocking can be set at any time
  if (strcmp(datasource_name, "mysql") == 0) {
    datasource_mgr->mysql_ds = bgpstream_mysql_datasource_create(filter_mgr);
    if(datasource_mgr->mysql_ds == NULL) {
      datasource_mgr->status = DS_ERROR;
    } 
    else {
      datasource_mgr->status = DS_ON;
    }
  }
  debug("\tBSDS_MGR: init end");
}


void bgpstream_datasource_mgr_set_blocking(bgpstream_datasource_mgr_t *datasource_mgr){
  debug("\tBSDS_MGR: set blocking start");
  if(datasource_mgr == NULL) {
    return; // no manager
  }
  datasource_mgr->blocking = 1;
  debug("\tBSDS_MGR: set blocking end");
}


int bgpstream_datasource_mgr_update_input_queue(bgpstream_datasource_mgr_t *datasource_mgr,
						bgpstream_input_mgr_t *input_mgr) {
  debug("\tBSDS_MGR: get data start");
  if(datasource_mgr == NULL) {
    return -1; // no datasource manager
  }
  int results = -1;
  if (strcmp(datasource_mgr->datasource_name, "mysql") == 0) {
    do{
      if(results == 0) { // results = 0 => 2+ time and database did not give any error
	sleep(30);
      }
      results = bgpstream_mysql_datasource_update_input_queue(datasource_mgr->mysql_ds, input_mgr);
      debug("\tBSDS_MGR: got %d (blocking: %d)", results, datasource_mgr->blocking);
    } while(datasource_mgr->blocking && results == 0);
  }
  debug("\tBSDS_MGR: get data end");
  return results; 
}


void bgpstream_datasource_mgr_close(bgpstream_datasource_mgr_t *datasource_mgr) {
  debug("\tBSDS_MGR: close start");
  if(datasource_mgr == NULL) {
    return; // no manager to destroy
  }
  if (strcmp(datasource_mgr->datasource_name, "mysql") == 0) {
    bgpstream_mysql_datasource_destroy(datasource_mgr->mysql_ds);
    datasource_mgr->mysql_ds = NULL;
  }
  datasource_mgr->status = DS_OFF;
  debug("\tBSDS_MGR: close end");
}


void bgpstream_datasource_mgr_destroy(bgpstream_datasource_mgr_t *datasource_mgr) {
  debug("\tBSDS_MGR: destroy start");
  if(datasource_mgr == NULL) {
    return; // no manager to destroy
  }
  // destroy any active datasource (if they have not been destroyed before
  if(datasource_mgr->mysql_ds != NULL) {
    bgpstream_mysql_datasource_destroy(datasource_mgr->mysql_ds);
    datasource_mgr->mysql_ds = NULL;
  }
  free(datasource_mgr);  
  debug("\tBSDS_MGR: destroy end");
}




/* ----------- mysql related functions ----------- */

bgpstream_mysql_datasource_t *bgpstream_mysql_datasource_create(bgpstream_filter_mgr_t *filter_mgr) {
  debug("\t\tBSDS_MYSQL: create mysql_ds start");
  bgpstream_mysql_datasource_t *mysql_ds = (bgpstream_mysql_datasource_t*) malloc(sizeof(bgpstream_mysql_datasource_t));
  if(mysql_ds == NULL) {
    return NULL; // can't allocate memory
  }
  // Initialize a MySQL object suitable for connection
  debug("\t\tBSDS_MYSQL: create mysql_ds mysql connection init");
  mysql_ds->mysql_con = mysql_init(NULL);
  if (mysql_ds->mysql_con == NULL) {
    fprintf(stderr, "%s\n", mysql_error(mysql_ds->mysql_con));
    mysql_close(mysql_ds->mysql_con);
    free(mysql_ds);
    mysql_ds = NULL;
    return NULL;
  }  
  // Establish a connection to the database
  debug("\t\tBSDS_MYSQL: create mysql_ds mysql connection establishment");
  if (mysql_real_connect(mysql_ds->mysql_con, "localhost", "routing", NULL, 
			   "bgparchive", 0,
			   "/usr/local/jail/charthouse/tmp/mysql.sock", 0) == NULL) {
    fprintf(stderr, "%s\n", mysql_error(mysql_ds->mysql_con));
    mysql_close(mysql_ds->mysql_con);
    free(mysql_ds);
    mysql_ds = NULL;
    return NULL;  
  } 

  // set time_zone = UTC
  if(mysql_query(mysql_ds->mysql_con, "set time_zone='+0:0'") == 0) {
    debug("\t\tBSDS_MYSQL: create mysql_ds set time_zone");
  }
  else{
    debug("\t\tBSDS_MYSQL: create mysql_ds set time_zone something wrong"); 
  }
    
  // initialize sql query
  strcpy(mysql_ds->sql_query, "SELECT CONCAT_WS('', \
                                CONCAT_WS('/',projects.path, collectors.path, bgp_types.path, \
                                         FROM_UNIXTIME(file_time,'%Y/%m/%d/')), \
                                CONCAT_WS('.',projects.name, collectors.name, bgp_types.name, \
                                          file_time, projects.file_ext)) as filename, \
                                projects.name, collectors.name, bgp_types.name, file_time, \
                                UNIX_TIMESTAMP(NOW())-1 \
                         FROM bgp_data join bgp_types join collectors join projects join \
                              on_web_frequency \
                         WHERE bgp_data.collector_id=collectors.id AND \
                               bgp_data.bgp_type_id=bgp_types.id   AND \
                               collectors.project_id=projects.id   AND \
                               collectors.project_id=on_web_frequency.project_id AND \
                               bgp_data.bgp_type_id= on_web_frequency.bgp_type_id");

  // project, collector, and bgp_type are used as filters
  // only if they are provided by the user
  if(strcmp(filter_mgr->project,"") != 0) {
    strcat (mysql_ds->sql_query," AND projects.name LIKE '");
    strcat (mysql_ds->sql_query, filter_mgr->project);
    strcat (mysql_ds->sql_query,"'");
  }
  if(strcmp(filter_mgr->collector,"") != 0) {
    strcat (mysql_ds->sql_query," AND collectors.name LIKE '");
    strcat (mysql_ds->sql_query, filter_mgr->collector);
    strcat (mysql_ds->sql_query,"'");
  }
  if(strcmp(filter_mgr->bgp_type,"") != 0) {
    strcat (mysql_ds->sql_query," AND bgp_types.name LIKE '");
    strcat (mysql_ds->sql_query, filter_mgr->bgp_type);
    strcat (mysql_ds->sql_query,"'");
  }
  
  // filetime is always provided
  strcat (mysql_ds->sql_query," AND file_time >=  ");
  strcat (mysql_ds->sql_query, filter_mgr->time_interval_start_str);
  // comment on 120 seconds:
  // sometimes it happens that ribs or updates carry a filetime which is not
  // compliant with the expected filetime (e.g. :
  //  rib.23.59 instead of rib.00.00
  // in order to compensate for this kind of situations we 
  // retrieve data that are 120 seconds older than the requested 
  // file_start_time
  strcat (mysql_ds->sql_query,"  - on_web_frequency.offset - 120");
  strcat (mysql_ds->sql_query," AND file_time <=  ");
  strcat (mysql_ds->sql_query, filter_mgr->time_interval_stop_str);

  // minimum timestamp is a placeholder
  strcat (mysql_ds->sql_query," AND UNIX_TIMESTAMP(ts) > ? AND UNIX_TIMESTAMP(ts) <= UNIX_TIMESTAMP(NOW())-1");


  // DEBUG HEEEEEEEEEEEEEEEREEEEE - remove this line after debug
  strcat (mysql_ds->sql_query," AND (collectors.name LIKE 'route-views.jinx' OR collectors.name LIKE 'route-views.saopaulo')");



  // order by filetime and bgptypes in reverse order: this way the 
  // input insertions are always "head" insertions, i.e. queue insertion is
  // faster
  strcat (mysql_ds->sql_query," ORDER BY file_time, bgp_types.name DESC");


  // printf("%s\n",mysql_ds->sql_query);
  debug("\t\tBSDS_MYSQL:  mysql query created");


  // the first last_timestamp is 0
  mysql_ds->last_timestamp = 0;
  
// Initialize the statement 
  mysql_ds->stmt = mysql_stmt_init(mysql_ds->mysql_con);
  if (!mysql_ds->stmt) {
    fprintf(stderr, " mysql_stmt_init(), out of memory\n");
    mysql_close(mysql_ds->mysql_con);
    free(mysql_ds);
    mysql_ds = NULL;
    return NULL;
  }
  
  // prepare the statement in database
  if (mysql_stmt_prepare(mysql_ds->stmt, mysql_ds->sql_query, strlen(mysql_ds->sql_query))) {
    fprintf(stderr, " mysql_stmt_prepare(), SELECT failed\n");
    fprintf(stderr, " %s\n", mysql_stmt_error(mysql_ds->stmt));
    mysql_stmt_close(mysql_ds->stmt);
    mysql_close(mysql_ds->mysql_con);
    free(mysql_ds);
    mysql_ds = NULL;
    return NULL;
  }

  /* Zero out the parameters structure and result data structures */
  memset(mysql_ds->parameters, 0, sizeof(mysql_ds->parameters));

  /* bind the parameters to bgpstream_sql variables */
  mysql_ds->parameters[0].buffer_type = MYSQL_TYPE_LONG;
  mysql_ds->parameters[0].buffer = (void *) &(mysql_ds->last_timestamp);
  mysql_ds->parameters[0].is_unsigned = 0;
  mysql_ds->parameters[0].is_null = 0;
  mysql_ds->parameters[0].length = 0;

  if (mysql_stmt_bind_param(mysql_ds->stmt, mysql_ds->parameters)) {
    fprintf(stderr, " mysql_stmt_bind_param() failed\n");
    fprintf(stderr, " %s\n", mysql_stmt_error(mysql_ds->stmt));
    mysql_stmt_close(mysql_ds->stmt);
    mysql_close(mysql_ds->mysql_con);
    free(mysql_ds);
    mysql_ds = NULL;
    return NULL;
  }

  /* bind results to bgpstream_sql variables */

  mysql_ds->results[0].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[0].buffer = mysql_ds->filename_res;
  mysql_ds->results[0].buffer_length = BGPSTREAM_DUMP_MAX_LEN;
  mysql_ds->results[0].is_null = 0;
  //  mysql_ds->results[0].length = &data_length;

  mysql_ds->results[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[1].buffer = mysql_ds->project_res;
  mysql_ds->results[1].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[1].is_null = 0;
  //  mysql_ds->results[0].length = &data_length;

  mysql_ds->results[2].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[2].buffer = mysql_ds->collector_res;
  mysql_ds->results[2].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[2].is_null = 0;
  //  mysql_ds->results[0].length = &data_length;

  mysql_ds->results[3].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[3].buffer = mysql_ds->bgp_type_res;
  mysql_ds->results[3].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[3].is_null = 0;
  //  mysql_ds->results[0].length = &data_length;

  mysql_ds->results[4].buffer_type = MYSQL_TYPE_LONG;
  mysql_ds->results[4].buffer = (void *) &(mysql_ds->filetime_res);
  mysql_ds->results[4].is_unsigned = 0;
  mysql_ds->results[4].is_null = 0;
  mysql_ds->results[4].length = 0;

  mysql_ds->results[5].buffer_type = MYSQL_TYPE_LONG;
  mysql_ds->results[5].buffer = (void *) &(mysql_ds->max_timestamp_res);
  mysql_ds->results[5].is_unsigned = 0;
  mysql_ds->results[5].is_null = 0;
  mysql_ds->results[5].length = 0;

  /* Bind the results buffer */
  if (mysql_stmt_bind_result(mysql_ds->stmt, mysql_ds->results) != 0) {
    fprintf(stderr, " mysql_stmt_bind_result() failed\n");
    fprintf(stderr, " %s\n", mysql_stmt_error(mysql_ds->stmt));
    mysql_stmt_close(mysql_ds->stmt);
    mysql_close(mysql_ds->mysql_con);
    free(mysql_ds);
    mysql_ds = NULL;
    return NULL;
  }

  debug("\t\tBSDS_MYSQL: create mysql_ds end");
  return mysql_ds;
}


int bgpstream_mysql_datasource_update_input_queue(bgpstream_mysql_datasource_t* mysql_ds,
						  bgpstream_input_mgr_t *input_mgr) {
  debug("\t\tBSDS_MYSQL: mysql_ds update input queue start ");
  
  int num_results = 0;
  // memset binded results
  memset(mysql_ds->filename_res, 0, BGPSTREAM_DUMP_MAX_LEN);
  memset(mysql_ds->project_res, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(mysql_ds->collector_res, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(mysql_ds->bgp_type_res, 0, BGPSTREAM_PAR_MAX_LEN);
  // mysql_ds->last_timestamp is the only parameter for the query
  /* Execute the statement */
  if (mysql_stmt_execute(mysql_ds->stmt)) {
    fprintf(stderr, " mysql_stmt_execute(), failed\n");
    fprintf(stderr, " %s\n", mysql_stmt_error(mysql_ds->stmt));
    return -1;
  }
  else{
    /* Print our results */
    while(mysql_stmt_fetch (mysql_ds->stmt) == 0) {
      num_results += bgpstream_input_mgr_push_sorted_input(input_mgr,
						       mysql_ds->filename_res,
						       mysql_ds->project_res,
						       mysql_ds->collector_res,
						       mysql_ds->bgp_type_res,
						       mysql_ds->filetime_res);
      //DEBUG printf("%s\n", mysql_ds->filename_res);
      debug("\t\tBSDS_MYSQL: added %d new inputs to input queue", num_results);
      debug("\t\tBSDS_MYSQL: %s - %s - %d", 
	    mysql_ds->filename_res, mysql_ds->bgp_type_res, mysql_ds->filetime_res);
      // here
      memset(mysql_ds->filename_res, 0, BGPSTREAM_DUMP_MAX_LEN);
      memset(mysql_ds->project_res, 0, BGPSTREAM_PAR_MAX_LEN);
      memset(mysql_ds->collector_res, 0, BGPSTREAM_PAR_MAX_LEN);
      memset(mysql_ds->bgp_type_res, 0, BGPSTREAM_PAR_MAX_LEN);
    }
    // the next time we will pull data that has been written 
    // after this timestamp
    mysql_ds->last_timestamp = mysql_ds->max_timestamp_res;
  }
  debug("\t\tBSDS_MYSQL: mysql_ds update input queue end");
  return num_results;
}




void bgpstream_mysql_datasource_destroy(bgpstream_mysql_datasource_t* mysql_ds) {
  debug("\t\tBSDS_MYSQL: destroy mysql_ds start");
  if(mysql_ds == NULL) {
    return; // nothing to destroy
  }
  // closing statement
  mysql_stmt_close(mysql_ds->stmt);
  mysql_ds->stmt = NULL;
  // closing mysql connection
  mysql_close(mysql_ds->mysql_con);
  mysql_ds->mysql_con = NULL;
  // free memory allocated for mysql datasource
  free(mysql_ds);
  debug("\t\tBSDS_MYSQL: destroy mysql_ds end");
  return;
}

