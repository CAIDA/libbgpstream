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

#include "bgpstream_datasource_mysql.h"
#include "bgpstream_debug.h"

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



struct struct_bgpstream_mysql_datasource_t {
  /* mysql connection handler */
  MYSQL * mysql_con;
  /* mysql connection options */
  char *mysql_dbname;
  char *mysql_user;
  char *mysql_password;
  char *mysql_host;
  unsigned int mysql_port;
  char *mysql_socket;
  /* command-line options */
  char *mysql_ris_path;
  char *mysql_rv_path;
  /* query text */
  char sql_query[2048];
  /* mysql statement */
  MYSQL_STMT *stmt;
  /* parameters for placeholders */
  MYSQL_BIND parameters[2];   
  /* variables to bind to placeholders */
  long int last_timestamp;       
  long int current_timestamp;
  /* query results */
  MYSQL_BIND results[9];    
  /* variables to bind to results */
  char proj_path_res[BGPSTREAM_PAR_MAX_LEN];
  char coll_path_res[BGPSTREAM_PAR_MAX_LEN];
  char type_path_res[BGPSTREAM_PAR_MAX_LEN];
  char proj_name_res[BGPSTREAM_PAR_MAX_LEN];
  char coll_name_res[BGPSTREAM_PAR_MAX_LEN];
  char type_name_res[BGPSTREAM_PAR_MAX_LEN];
  char file_ext_res[BGPSTREAM_PAR_MAX_LEN];
  int filetime_res;
  int file_time_span;
};



bgpstream_mysql_datasource_t *bgpstream_mysql_datasource_create(bgpstream_filter_mgr_t *filter_mgr,
								       char * mysql_dbname,
								       char * mysql_user,
                                                                       char * mysql_password,
								       char * mysql_host,
                                                                       unsigned int mysql_port,
								       char * mysql_socket,
                                                                       char * mysql_ris_path,
                                                                       char * mysql_rv_path)
{
  bgpstream_debug("\t\tBSDS_MYSQL: create mysql_ds start");
  bgpstream_mysql_datasource_t *mysql_ds = (bgpstream_mysql_datasource_t*) malloc(sizeof(bgpstream_mysql_datasource_t));
  if(mysql_ds == NULL) {
    return NULL; // can't allocate memory
  }
  // set up options or provide defaults
  // default: bgparchive
  if(mysql_dbname == NULL)
    {
      mysql_ds->mysql_dbname = strdup("bgparchive");      
    }
  else 
    {
      mysql_ds->mysql_dbname = strdup(mysql_dbname);
    }
  // default user : bgpstream
  if(mysql_user == NULL)
    {
      mysql_ds->mysql_user = strdup("bgpstream");      
    }
  else 
    {
      mysql_ds->mysql_user = strdup(mysql_user);
    }
  // default: no password
  if(mysql_password == NULL)
    {
      mysql_ds->mysql_password = NULL;      
    }
  else 
    {
      mysql_ds->mysql_password = strdup(mysql_password);
    }
  // default: localhost (if null is passed, localhost is assumed)
  if(mysql_host == NULL)
    {
      mysql_ds->mysql_host = NULL;      
    }
  else 
    {
      mysql_ds->mysql_host = strdup(mysql_host);
    }
  // default: default unix socket (if null is give)
  if(mysql_socket == NULL)
    {
      mysql_ds->mysql_socket = NULL;      
    }
  else 
    {
      mysql_ds->mysql_socket = strdup(mysql_socket);
    }
  // default: default port (if 0)
  if(mysql_port != 0)
    {
      mysql_ds->mysql_port = mysql_port;      
    }
  // default: NULL (data is taken from db)
  if(mysql_ris_path == NULL)
    {
      mysql_ds->mysql_ris_path = NULL;      
    }
  else 
    {
      mysql_ds->mysql_ris_path = strdup(mysql_ris_path);
    }
  // default: NULL (data is taken from db)
  if(mysql_rv_path == NULL)
    {
      mysql_ds->mysql_rv_path = NULL;      
    }
  else 
    {
      mysql_ds->mysql_rv_path = strdup(mysql_rv_path);
    }

  
  // Initialize a MySQL object suitable for connection
  bgpstream_debug("\t\tBSDS_MYSQL: create mysql_ds mysql connection init");
  mysql_ds->mysql_con = mysql_init(NULL);
  if (mysql_ds->mysql_con == NULL) {
    fprintf(stderr, "%s\n", mysql_error(mysql_ds->mysql_con));
    mysql_close(mysql_ds->mysql_con);
    free(mysql_ds);
    mysql_ds = NULL;
    return NULL;
  }

  /* fprintf(stderr, */
  /*         "Mysql configuration: \n\t" */
  /*         "host: %s " */
  /*         "user: %s " */
  /*         "password: %s " */
  /*         "db: %s " */
  /*         "port: %u " */
  /*         "socket: %s\n" */
  /*         "RIS path: %s \n" */
  /*         "RV path: %s \n", */
  /*         mysql_ds->mysql_host, */
  /*         mysql_ds->mysql_user, mysql_ds->mysql_password,  */
  /*         mysql_ds->mysql_dbname, mysql_ds->mysql_port, */
  /*         mysql_ds->mysql_socket, */
  /*         mysql_ds->mysql_ris_path, mysql_ds->mysql_rv_path); */
          
  // Establish a connection to the database
  bgpstream_debug("\t\tBSDS_MYSQL: create mysql_ds mysql connection establishment");
  if (mysql_real_connect(mysql_ds->mysql_con, mysql_ds->mysql_host,
                         mysql_ds->mysql_user, mysql_ds->mysql_password, 
                         mysql_ds->mysql_dbname, mysql_ds->mysql_port,
                         mysql_ds->mysql_socket, 0 /* client-flag */) == NULL) 
    {
    fprintf(stderr, "%s\n", mysql_error(mysql_ds->mysql_con));
    mysql_close(mysql_ds->mysql_con);
    free(mysql_ds);
    mysql_ds = NULL;
    return NULL;  
  } 

  // set time_zone = UTC
  if(mysql_query(mysql_ds->mysql_con, "set time_zone='+0:0'") == 0) {
    bgpstream_debug("\t\tBSDS_MYSQL: create mysql_ds set time_zone");
  }
  else{
    bgpstream_debug("\t\tBSDS_MYSQL: create mysql_ds set time_zone something wrong"); 
  }

  strcpy(mysql_ds->sql_query,
	 "SELECT "
	 "projects.path, collectors.path, bgp_types.path, "
	 "projects.name, collectors.name, bgp_types.name, projects.file_ext, "
	 "file_time, on_web_frequency.offset "
	 "FROM bgp_data "
	 "JOIN bgp_types  ON bgp_types.id  = bgp_data.bgp_type_id "
	 "JOIN collectors ON collectors.id = bgp_data.collector_id "
	 "JOIN projects   ON projects.id   = collectors.project_id "
	 "JOIN on_web_frequency "
	 "     ON on_web_frequency.project_id  = projects.id AND "
	 "        on_web_frequency.bgp_type_id = bgp_types.id"
	 );


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
      strcat (mysql_ds->sql_query," ( ");

      // BEGIN TIME
      strcat (mysql_ds->sql_query," (file_time >=  ");
      sprintf(mysql_ds->sql_query + strlen(mysql_ds->sql_query),
              "%"PRIu32, tif->begin_time);
      strcat (mysql_ds->sql_query,"  - on_web_frequency.offset - 120 )");

      strcat (mysql_ds->sql_query,"  AND  ");

      // END TIME
      strcat (mysql_ds->sql_query," (file_time <=  ");
      sprintf(mysql_ds->sql_query + strlen(mysql_ds->sql_query),
              "%"PRIu32, tif->end_time);
      strcat (mysql_ds->sql_query,") ");

      strcat (mysql_ds->sql_query," ) ");
      tif = tif->next;
      if(tif!= NULL) {
	strcat (mysql_ds->sql_query, " OR ");      
      }
    }
    strcat (mysql_ds->sql_query," )");
  }
  /*  comment on 120 seconds: */
  /*  sometimes it happens that ribs or updates carry a filetime which is not */
  /*  compliant with the expected filetime (e.g. : */
  /*   rib.23.59 instead of rib.00.00 */
  /*  in order to compensate for this kind of situations we  */
  /*  retrieve data that are 120 seconds older than the requested  */

  // minimum timestamp and current timestamp are the two placeholders
  strcat (mysql_ds->sql_query," AND UNIX_TIMESTAMP(ts) > ? AND UNIX_TIMESTAMP(ts) <= ?");

  // order by filetime and bgptypes in reverse order: this way the 
  // input insertions are always "head" insertions, i.e. queue insertion is
  // faster
  strcat (mysql_ds->sql_query," ORDER BY file_time DESC, bgp_types.name DESC");

  // printf("%s\n",mysql_ds->sql_query);
  bgpstream_debug("\t\tBSDS_MYSQL:  mysql query created");

  // the first last_timestamp is 0
  mysql_ds->last_timestamp = 0;
  // the first current_timestamp is 0
  mysql_ds->current_timestamp = 0;
  
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
  mysql_ds->parameters[0].buffer = (char *) &(mysql_ds->last_timestamp);
  mysql_ds->parameters[0].is_null = 0;
  mysql_ds->parameters[0].length = 0;

  mysql_ds->parameters[1].buffer_type = MYSQL_TYPE_LONG;
  mysql_ds->parameters[1].buffer = (char *) &(mysql_ds->current_timestamp);
  mysql_ds->parameters[1].is_null = 0;
  mysql_ds->parameters[1].length = 0;

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
  memset(mysql_ds->results, 0, sizeof(mysql_ds->results));

  /* PROJECT PATH */
  mysql_ds->results[0].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[0].buffer = mysql_ds->proj_path_res;
  mysql_ds->results[0].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[0].is_null = 0;
  /* COLLECTOR PATH */
  mysql_ds->results[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[1].buffer = mysql_ds->coll_path_res;
  mysql_ds->results[1].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[1].is_null = 0;
  /* TYPE PATH */
  mysql_ds->results[2].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[2].buffer = mysql_ds->type_path_res;
  mysql_ds->results[2].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[2].is_null = 0;

  /* PROJECT NAME */
  mysql_ds->results[3].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[3].buffer = mysql_ds->proj_name_res;
  mysql_ds->results[3].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[3].is_null = 0;
  /* COLLECTOR NAME */
  mysql_ds->results[4].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[4].buffer = mysql_ds->coll_name_res;
  mysql_ds->results[4].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[4].is_null = 0;
  /* TYPE NAME */
  mysql_ds->results[5].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[5].buffer = mysql_ds->type_name_res;
  mysql_ds->results[5].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[5].is_null = 0;
  /* FILE EXT */
  mysql_ds->results[6].buffer_type= MYSQL_TYPE_VAR_STRING;
  mysql_ds->results[6].buffer = mysql_ds->file_ext_res;
  mysql_ds->results[6].buffer_length = BGPSTREAM_PAR_MAX_LEN;
  mysql_ds->results[6].is_null = 0;
  /* FILETIME */
  mysql_ds->results[7].buffer_type = MYSQL_TYPE_LONG;
  mysql_ds->results[7].buffer = (void *) &(mysql_ds->filetime_res);
  mysql_ds->results[7].is_unsigned = 0;
  mysql_ds->results[7].is_null = 0;
  mysql_ds->results[7].length = 0;
  /* FILE TIME SPAN */
  mysql_ds->results[8].buffer_type = MYSQL_TYPE_LONG;
  mysql_ds->results[8].buffer = (void *) &(mysql_ds->file_time_span);
  mysql_ds->results[8].is_unsigned = 0;
  mysql_ds->results[8].is_null = 0;
  mysql_ds->results[8].length = 0;

  
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

  bgpstream_debug("\t\tBSDS_MYSQL: create mysql_ds end");
  return mysql_ds;
}


static char *
build_filename(bgpstream_mysql_datasource_t *ds) {

  char filename[4096];

  char date[11]; /* "YYYY/MM/DD" */
  long tmp = ds->filetime_res;
  struct tm time_result;
  // gmtime_r is the thread_safe version of gmtime
  if(strftime(date, 11, "%Y/%m/%d", gmtime_r(&tmp,&time_result)) == 0) {
    return NULL;
  }

  // default path
  char *path_string = ds->proj_path_res;
  
  // project is r[o]uteviews and rv path is set
  if(ds->mysql_rv_path != NULL && ds->proj_name_res[1] == 'o')
    {
      path_string = ds->mysql_rv_path;
    }
  
  // project is r[i]s and ris path is set
  if(ds->mysql_ris_path != NULL && ds->proj_name_res[1] == 'i')
    {
      path_string = ds->mysql_ris_path;
    }

  if(sprintf(filename, "%s/%s/%s/%s/%s.%s.%s.%d.%s",
	   path_string, ds->coll_path_res,
	   ds->type_path_res, date,
	   ds->proj_name_res, ds->coll_name_res,
	   ds->type_name_res, ds->filetime_res,
	   ds->file_ext_res
             ) -1 > 4095)
    {
      fprintf(stderr, "Error, trying to write a file name larger than 4095 characters!\n");
      return NULL;
    }

  return strdup(filename);
}


int bgpstream_mysql_datasource_update_input_queue(bgpstream_mysql_datasource_t* mysql_ds,
                                                  bgpstream_input_mgr_t *input_mgr) {
  
  // printf("--> %d\n",  mysql_ds->current_timestamp);
  bgpstream_debug("\t\tBSDS_MYSQL: mysql_ds update input queue start ");

  char *filename = NULL;
  int num_results = 0;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  // update current_timestamp - we always ask for data 1 second old at least
  mysql_ds->current_timestamp = tv.tv_sec - 1; // now() - 1 second

  /* just to be safe, we clear all the strings */
  mysql_ds->proj_path_res[0] = '\0';
  mysql_ds->coll_path_res[0] = '\0';
  mysql_ds->type_path_res[0] = '\0';
  mysql_ds->proj_name_res[0] = '\0';
  mysql_ds->coll_name_res[0] = '\0';
  mysql_ds->type_name_res[0] = '\0';
  mysql_ds->file_ext_res[0]  = '\0';

  // mysql_ds->last_timestamp is the only parameter for the query

  /* Try to execute the statement, if it
  *  fails then retry after waiting an
  *  increasing retry delay
  */
  int stmt_ret = mysql_stmt_execute(mysql_ds->stmt);

  unsigned int retry_attempts = 0;
  unsigned int maximum_wait_time = 900; // 15 minutes
  unsigned int wait_time = 1; // initial wait time 1 second
  
  while(stmt_ret != 0)
    {
      retry_attempts++;      

      // check if the wait time is less than the maximum
      if(wait_time < maximum_wait_time)
        {
          // we double the wait time at each attempt
          wait_time = wait_time *2;
        }
      
      fprintf(stderr,
              "bgpstream: connection to mysql failed, retrying [last timestamp: %ld, attempt: %d]\n",
	      mysql_ds->last_timestamp, retry_attempts);

      sleep(wait_time);
      
      // closing all structures and restarting
      if(mysql_ds->stmt != NULL)
	{
	  mysql_stmt_close(mysql_ds->stmt);
	}
      mysql_ds->stmt = NULL;     
      mysql_close(mysql_ds->mysql_con);
      mysql_ds->mysql_con = NULL;
      
      // reconnect and retry one more time
      // http://bugs.mysql.com/bug.php?id=35937
      // note that mysql prepared statement have to be reconfigured again

      if( (mysql_ds->mysql_con = mysql_init(NULL)) != NULL)
	{
	  if (mysql_real_connect(mysql_ds->mysql_con, mysql_ds->mysql_host,
                                 mysql_ds->mysql_user, mysql_ds->mysql_password, 
				 mysql_ds->mysql_dbname, mysql_ds->mysql_port,
                                 mysql_ds->mysql_socket, 0 /* client-flag */) != NULL)	
	    {
	      if(mysql_query(mysql_ds->mysql_con, "set time_zone='+0:0'") == 0)
		{
		  if((mysql_ds->stmt = mysql_stmt_init(mysql_ds->mysql_con)) != NULL)
		    {
		      if (mysql_stmt_prepare(mysql_ds->stmt, mysql_ds->sql_query,
                                             strlen(mysql_ds->sql_query)) == 0)
			{
			  if(mysql_stmt_bind_param(mysql_ds->stmt, mysql_ds->parameters) == 0)
			    {
			      if(mysql_stmt_bind_result(mysql_ds->stmt, mysql_ds->results) == 0) 
				{
				  if((stmt_ret = mysql_stmt_execute(mysql_ds->stmt)) == 0)
				    {
				      break;
				    }
				}
			    }
			}
		    }
		}
	    }
	  // if here something went wrong
	  fprintf(stderr, "%s\n", mysql_error(mysql_ds->mysql_con));
	}
    }
  
  // if stmt ret is still wrong  
  if (stmt_ret != 0) {
    // something wrong happened
    fprintf(stderr, " mysql_stmt_execute(), failed\n");
    fprintf(stderr, " %s\n", mysql_stmt_error(mysql_ds->stmt));
    return -1;
  }


  /* Print our results */
  while(mysql_stmt_fetch (mysql_ds->stmt) == 0) {

    /* build the filename from pieces given by sql */
    if((filename = build_filename(mysql_ds)) == NULL) {
      fprintf(stderr, "could not build file name\n");
      return -1;
    }

    num_results +=
      bgpstream_input_mgr_push_sorted_input(input_mgr,
					    filename,
					    strdup(mysql_ds->proj_name_res),
					    strdup(mysql_ds->coll_name_res),
					    strdup(mysql_ds->type_name_res),
					    mysql_ds->filetime_res,
					    mysql_ds->file_time_span);
    //DEBUG printf("%s\n", mysql_ds->filename_res);
    bgpstream_debug("\t\tBSDS_MYSQL: added %d new inputs to input queue", num_results);
    bgpstream_debug("\t\tBSDS_MYSQL: %s - %s - %d", 
		    filename, mysql_ds->type_name_res, mysql_ds->filetime_res);
    // here
    /* clear all the strings... just in case */
    mysql_ds->proj_path_res[0] = '\0';
    mysql_ds->coll_path_res[0] = '\0';
    mysql_ds->type_path_res[0] = '\0';
    mysql_ds->proj_name_res[0] = '\0';
    mysql_ds->coll_name_res[0] = '\0';
    mysql_ds->type_name_res[0] = '\0';
    mysql_ds->file_ext_res[0] = '\0';
  }
  // the next time we will pull data that has been written 
  // after the current timestamp
  mysql_ds->last_timestamp = mysql_ds->current_timestamp;

  bgpstream_debug("\t\tBSDS_MYSQL: mysql_ds update input queue end");
  return num_results;
}


void
bgpstream_mysql_datasource_destroy(bgpstream_mysql_datasource_t* mysql_ds) {
  bgpstream_debug("\t\tBSDS_MYSQL: destroy mysql_ds start");
  if(mysql_ds == NULL) {
    return; // nothing to destroy
  }
  // closing statement
  if(mysql_ds->stmt != NULL) {
    mysql_stmt_close(mysql_ds->stmt);
  }
  mysql_ds->stmt = NULL;
  // closing mysql connection
  mysql_close(mysql_ds->mysql_con);
  mysql_ds->mysql_con = NULL;
  // free memory allocated for mysql datasource
  if(mysql_ds->mysql_dbname != NULL)
    {
      free(mysql_ds->mysql_dbname);
    }
  if(mysql_ds->mysql_user != NULL)
    {
      free(mysql_ds->mysql_user);
    }
  if(mysql_ds->mysql_password != NULL)
    {
      free(mysql_ds->mysql_password);
    }
  if(mysql_ds->mysql_host != NULL)
    {
      free(mysql_ds->mysql_host);
    }
  if(mysql_ds->mysql_socket != NULL)
    {
      free(mysql_ds->mysql_socket);
    }
  if(mysql_ds->mysql_ris_path != NULL)
    {
      free(mysql_ds->mysql_ris_path);
    }
  if(mysql_ds->mysql_rv_path != NULL)
    {
      free(mysql_ds->mysql_rv_path);
    }
  free(mysql_ds);
  bgpstream_debug("\t\tBSDS_MYSQL: destroy mysql_ds end");
  return;
}









