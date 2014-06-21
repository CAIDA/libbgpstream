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


/* datasource specific functions declarations */

// customlist datasource functions
static bgpstream_customlist_datasource_t *bgpstream_customlist_datasource_create(bgpstream_filter_mgr_t *filter_mgr);
static int bgpstream_customlist_datasource_update_input_queue(bgpstream_customlist_datasource_t* customlist_ds,
							      bgpstream_input_mgr_t *input_mgr);
static void bgpstream_customlist_datasource_destroy(bgpstream_customlist_datasource_t* customlist_ds);

// mysql datasource functions
static bgpstream_mysql_datasource_t *bgpstream_mysql_datasource_create(bgpstream_filter_mgr_t *filter_mgr);
static int bgpstream_mysql_datasource_update_input_queue(bgpstream_mysql_datasource_t* mysql_ds,
							 bgpstream_input_mgr_t *input_mgr);
static void bgpstream_mysql_datasource_destroy(bgpstream_mysql_datasource_t* mysql_ds);


/* datasource mgr related functions */


bgpstream_datasource_mgr_t *bgpstream_datasource_mgr_create(){
  debug("\tBSDS_MGR: create start");
  bgpstream_datasource_mgr_t *datasource_mgr = (bgpstream_datasource_mgr_t*) malloc(sizeof(bgpstream_datasource_mgr_t));
  if(datasource_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  // default values
  memset(datasource_mgr->datasource_name, 0, BGPSTREAM_DS_MAX_LEN);   
  strcpy(datasource_mgr->datasource_name, "");   
  datasource_mgr->blocking = 0;
  // datasources (none of them is active)
  datasource_mgr->mysql_ds = NULL;
  datasource_mgr->customlist_ds = NULL;
  datasource_mgr->status = DS_OFF;
  debug("\tBSDS_MGR: create end");
  return datasource_mgr;
}


void bgpstream_datasource_mgr_set_data_interface(bgpstream_datasource_mgr_t *datasource_mgr,
						 const char *datasource_name) {
  debug("\tBSDS_MGR: set data interface start");
  if(datasource_mgr == NULL) {
    return; // no manager
  }
  strcpy(datasource_mgr->datasource_name, datasource_name);   
  debug("\tBSDS_MGR: set  data interface end");
}


void bgpstream_datasource_mgr_init(bgpstream_datasource_mgr_t *datasource_mgr,
				   bgpstream_filter_mgr_t *filter_mgr){
  debug("\tBSDS_MGR: init start");
  if(datasource_mgr == NULL) {
    return; // no manager
  }
  // datasource_mgr->blocking can be set at any time
  if (strcmp(datasource_mgr->datasource_name, "mysql") == 0) {
    datasource_mgr->mysql_ds = bgpstream_mysql_datasource_create(filter_mgr);
    if(datasource_mgr->mysql_ds == NULL) {
      datasource_mgr->status = DS_ERROR;
    } 
    else {
      datasource_mgr->status = DS_ON;
    }
  }
  if (strcmp(datasource_mgr->datasource_name, "customlist") == 0) {
    datasource_mgr->customlist_ds = bgpstream_customlist_datasource_create(filter_mgr);
    if(datasource_mgr->customlist_ds == NULL) {
      datasource_mgr->status = DS_ERROR;
    } 
    else {
      datasource_mgr->status = DS_ON;
    }
  }
  // if none of the datasources is matched the status of the DS is not set to ON
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
      results = bgpstream_mysql_datasource_update_input_queue(datasource_mgr->mysql_ds, input_mgr);
      if(results == 0 && datasource_mgr->blocking) {
	// results = 0 => 2+ time and database did not give any error
	sleep(30);
      }
      debug("\tBSDS_MGR: got %d (blocking: %d)", results, datasource_mgr->blocking);
    } while(datasource_mgr->blocking && results == 0);
  }
  if (strcmp(datasource_mgr->datasource_name, "customlist") == 0) {
    results = bgpstream_customlist_datasource_update_input_queue(datasource_mgr->customlist_ds, input_mgr);
    debug("\tBSDS_MGR: got %d (blocking: %d)", results, datasource_mgr->blocking);
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
  if (strcmp(datasource_mgr->datasource_name, "customlist") == 0) {
    bgpstream_customlist_datasource_destroy(datasource_mgr->customlist_ds);
    datasource_mgr->customlist_ds = NULL;
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



/* ----------- customlist related functions ----------- */

static bgpstream_customlist_datasource_t *bgpstream_customlist_datasource_create(bgpstream_filter_mgr_t *filter_mgr) {
  debug("\t\tBSDS_CLIST: create customlist_ds start");  
  bgpstream_customlist_datasource_t *customlist_ds = (bgpstream_customlist_datasource_t*) malloc(sizeof(bgpstream_customlist_datasource_t));
  if(customlist_ds == NULL) {
    log_err("\t\tBSDS_CLIST: create customlist_ds can't allocate memory");    
    return NULL; // can't allocate memory
  }
  customlist_ds->filter_mgr = filter_mgr;
  customlist_ds->list_read = 0;
  debug("\t\tBSDS_CLIST: create customlist_ds end");
  return customlist_ds;
}


static bool bgpstream_customlist_datasource_filter_ok(bgpstream_customlist_datasource_t* customlist_ds) {
  debug("\t\tBSDS_CLIST: customlist_ds apply filter start");  
  bgpstream_string_filter_t * sf;
  bgpstream_interval_filter_t * tif;
  bool all_false;
  // projects
  all_false = true;
  if(customlist_ds->filter_mgr->projects != NULL) {
    sf = customlist_ds->filter_mgr->projects;
    while(sf != NULL) {
      if(strcmp(sf->value, customlist_ds->project) == 0) {
	all_false = false;
	break;
      }
      sf = sf->next;
    }
    if(all_false) {
      return false; 
    }
  }
  // collectors
  all_false = true;
  if(customlist_ds->filter_mgr->collectors != NULL) {
    sf = customlist_ds->filter_mgr->collectors;
    while(sf != NULL) {
      if(strcmp(sf->value, customlist_ds->collector) == 0) {
	all_false = false;
	break;
      }
      sf = sf->next;
    }
    if(all_false) {
      return false; 
    }
  }
  // bgp_types
  all_false = true;
  if(customlist_ds->filter_mgr->bgp_types != NULL) {
    sf = customlist_ds->filter_mgr->bgp_types;
    while(sf != NULL) {
      if(strcmp(sf->value, customlist_ds->bgp_type) == 0) {
	all_false = false;
	break;
      }
      sf = sf->next;
    }
    if(all_false) {
      return false; 
    }
  }
  // time_intervals
  all_false = true;
  if(customlist_ds->filter_mgr->time_intervals != NULL) {
    tif = customlist_ds->filter_mgr->time_intervals;
    while(tif != NULL) {      
      // filetime (we consider 15 mins before to consider routeviews updates
      // and 120 seconds to have some margins)
      if(customlist_ds->filetime >= (tif->time_interval_start - 15*60 - 120) &&
	 customlist_ds->filetime <= tif->time_interval_stop) {
	all_false = false;
	break;
      }
      tif = tif->next;
    }
    if(all_false) {
      return false; 
    }
  }
  // if all the filters are passed
  return true;
}


static int bgpstream_customlist_datasource_update_input_queue(bgpstream_customlist_datasource_t* customlist_ds,
							      bgpstream_input_mgr_t *input_mgr) {
    debug("\t\tBSDS_CLIST: customlist_ds update input queue start");  
    int num_results = 0;       
    // if list has not been read yet, then we push these files in the input queue
    if(customlist_ds->list_read == 0) {

      // file 1:
      strcpy(customlist_ds->filename, "./test-dumps/routeviews.route-views.jinx.ribs.1401487200.bz2");
      strcpy(customlist_ds->project, "routeviews");
      strcpy(customlist_ds->collector, "route-views.jinx");
      strcpy(customlist_ds->bgp_type, "ribs");
      customlist_ds->filetime = 1401487200;
      if(bgpstream_customlist_datasource_filter_ok(customlist_ds)){
	num_results += bgpstream_input_mgr_push_sorted_input(input_mgr, customlist_ds->filename,
							     customlist_ds->project, customlist_ds->collector,
							     customlist_ds->bgp_type, customlist_ds->filetime);
      }
      // file 2:
      strcpy(customlist_ds->filename, "./test-dumps/routeviews.route-views.jinx.updates.1401493500.bz2");
      strcpy(customlist_ds->project, "routeviews");
      strcpy(customlist_ds->collector, "route-views.jinx");
      strcpy(customlist_ds->bgp_type, "updates");
      customlist_ds->filetime = 1401493500;
      if(bgpstream_customlist_datasource_filter_ok(customlist_ds)){
	num_results += bgpstream_input_mgr_push_sorted_input(input_mgr, customlist_ds->filename,
							     customlist_ds->project, customlist_ds->collector,
							     customlist_ds->bgp_type, customlist_ds->filetime);
      }
      // file 3:
      strcpy(customlist_ds->filename, "./test-dumps/ris.rrc06.ribs.1400544000.gz");
      strcpy(customlist_ds->project, "ris");
      strcpy(customlist_ds->collector, "rrc06");
      strcpy(customlist_ds->bgp_type, "ribs");
      customlist_ds->filetime = 1400544000;
      if(bgpstream_customlist_datasource_filter_ok(customlist_ds)){
	num_results += bgpstream_input_mgr_push_sorted_input(input_mgr, customlist_ds->filename,
							     customlist_ds->project, customlist_ds->collector,
							     customlist_ds->bgp_type, customlist_ds->filetime);
      }
      // file 4:
      strcpy(customlist_ds->filename, "./test-dumps/ris.rrc06.updates.1401488100.gz");
      strcpy(customlist_ds->project, "ris");
      strcpy(customlist_ds->collector, "rrc06");
      strcpy(customlist_ds->bgp_type, "updates");
      customlist_ds->filetime = 1401488100;
      if(bgpstream_customlist_datasource_filter_ok(customlist_ds)){
	num_results += bgpstream_input_mgr_push_sorted_input(input_mgr, customlist_ds->filename,
							     customlist_ds->project, customlist_ds->collector,
							     customlist_ds->bgp_type, customlist_ds->filetime);
      }
      // end of files
    }
    customlist_ds->list_read = 1;
    debug("\t\tBSDS_CLIST: customlist_ds update input queue end");  
    return num_results;
}


static void bgpstream_customlist_datasource_destroy(bgpstream_customlist_datasource_t* customlist_ds) {
  debug("\t\tBSDS_CLIST: destroy customlist_ds start");  
  if(customlist_ds == NULL) {
    return; // nothing to destroy
  }
  customlist_ds->filter_mgr = NULL;
  customlist_ds->list_read = 0;
  free(customlist_ds);
  debug("\t\tBSDS_CLIST: destroy customlist_ds end");  
}




/* ----------- mysql related functions ----------- */

static bgpstream_mysql_datasource_t *bgpstream_mysql_datasource_create(bgpstream_filter_mgr_t *filter_mgr) {
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


  // projects, collectors, bgp_types, and time_intervals are used as filters
  // only if they are provided by the user
  bgpstream_string_filter_t * sf;
  bgpstream_interval_filter_t * tif;
  
  // projects
  if(filter_mgr->projects != NULL) {
    sf = filter_mgr->projects;
    strcat (mysql_ds->sql_query," AND projects.name IN (");
    while(sf != NULL) {
      strcat (mysql_ds->sql_query, "'");
      strcat (mysql_ds->sql_query, sf->value);
      strcat (mysql_ds->sql_query, "'");
      sf = sf->next;
      if(sf!= NULL) {
	strcat (mysql_ds->sql_query, ", ");      
      }
    }
    strcat (mysql_ds->sql_query," )");
  }
  // collectors
  if(filter_mgr->collectors != NULL) {
    sf = filter_mgr->collectors;
    strcat (mysql_ds->sql_query," AND collectors.name IN (");
    while(sf != NULL) {
      strcat (mysql_ds->sql_query, "'");
      strcat (mysql_ds->sql_query, sf->value);
      strcat (mysql_ds->sql_query, "'");
      sf = sf->next;
      if(sf!= NULL) {
	strcat (mysql_ds->sql_query, ", ");      
      }
    }
    strcat (mysql_ds->sql_query," )");
  }
  // bgp_types
  if(filter_mgr->bgp_types != NULL) {
    sf = filter_mgr->bgp_types;
    strcat (mysql_ds->sql_query," AND bgp_types.name IN (");
    while(sf != NULL) {
      strcat (mysql_ds->sql_query, "'");
      strcat (mysql_ds->sql_query, sf->value);
      strcat (mysql_ds->sql_query, "'");
      sf = sf->next;
      if(sf!= NULL) {
	strcat (mysql_ds->sql_query, ", ");      
      }
    }
    strcat (mysql_ds->sql_query," )");
  }

  // time_intervals
  if(filter_mgr->time_intervals != NULL) {
    tif = filter_mgr->time_intervals;
    strcat (mysql_ds->sql_query," AND ( ");
    while(tif != NULL) {
      strcat (mysql_ds->sql_query," (file_time >=  ");
      strcat (mysql_ds->sql_query, tif->start);
      strcat (mysql_ds->sql_query,"  - on_web_frequency.offset - 120");
      strcat (mysql_ds->sql_query," AND file_time <=  ");
      strcat (mysql_ds->sql_query, tif->stop);
      strcat (mysql_ds->sql_query,") ");
      tif = tif->next;
      if(tif!= NULL) {
	strcat (mysql_ds->sql_query, " OR ");      
      }
    }
    strcat (mysql_ds->sql_query," )");
  }
  // comment on 120 seconds:
  // sometimes it happens that ribs or updates carry a filetime which is not
  // compliant with the expected filetime (e.g. :
  //  rib.23.59 instead of rib.00.00
  // in order to compensate for this kind of situations we 
  // retrieve data that are 120 seconds older than the requested 

  // minimum timestamp is a placeholder
  strcat (mysql_ds->sql_query," AND UNIX_TIMESTAMP(ts) > ? AND UNIX_TIMESTAMP(ts) <= UNIX_TIMESTAMP(NOW())-1");

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


static int bgpstream_mysql_datasource_update_input_queue(bgpstream_mysql_datasource_t* mysql_ds,
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


static void bgpstream_mysql_datasource_destroy(bgpstream_mysql_datasource_t* mysql_ds) {
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

