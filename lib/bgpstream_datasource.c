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

/* datasource specific functions declarations */

// customlist datasource functions
static bgpstream_customlist_datasource_t *bgpstream_customlist_datasource_create(bgpstream_filter_mgr_t *filter_mgr);
static int bgpstream_customlist_datasource_update_input_queue(bgpstream_customlist_datasource_t* customlist_ds,
							      bgpstream_input_mgr_t *input_mgr);
static void bgpstream_customlist_datasource_destroy(bgpstream_customlist_datasource_t* customlist_ds);

// csvfile datasource functions
static bgpstream_csvfile_datasource_t *bgpstream_csvfile_datasource_create(bgpstream_filter_mgr_t *filter_mgr,
									   char * csvfile_file);
static int bgpstream_csvfile_datasource_update_input_queue(bgpstream_csvfile_datasource_t* csvfile_ds,
							   bgpstream_input_mgr_t *input_mgr);
static void bgpstream_csvfile_datasource_destroy(bgpstream_csvfile_datasource_t* csvfile_ds);

// mysql datasource functions
static bgpstream_mysql_datasource_t *bgpstream_mysql_datasource_create(bgpstream_filter_mgr_t *filter_mgr,
								       char * mysql_dbname,
								       char * mysql_user,
								       char * mysql_host);
static int bgpstream_mysql_datasource_update_input_queue(bgpstream_mysql_datasource_t* mysql_ds,
							 bgpstream_input_mgr_t *input_mgr);
static void bgpstream_mysql_datasource_destroy(bgpstream_mysql_datasource_t* mysql_ds);


/* datasource mgr related functions */

// backoff minimum and maximum times
const static int bs_min_wait = 30;   // 30 seconds
const static int bs_max_wait = 3600; // 1 hour



bgpstream_datasource_mgr_t *bgpstream_datasource_mgr_create(){
  bgpstream_debug("\tBSDS_MGR: create start");
  bgpstream_datasource_mgr_t *datasource_mgr = (bgpstream_datasource_mgr_t*) malloc(sizeof(bgpstream_datasource_mgr_t));
  if(datasource_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  // default values
  datasource_mgr->datasource = BGPSTREAM_DATA_INTERFACE_MYSQL; // default data source
  datasource_mgr->blocking = 0;
  datasource_mgr->backoff_time = bs_min_wait;
  // datasources (none of them is active at the beginning)
  datasource_mgr->mysql_ds = NULL;
  datasource_mgr->customlist_ds = NULL;
  datasource_mgr->csvfile_ds = NULL;
  datasource_mgr->status = DS_OFF;
  // datasource options
  datasource_mgr->mysql_dbname = NULL;
  datasource_mgr->mysql_user = NULL;
  datasource_mgr->mysql_host = NULL;
  datasource_mgr->csvfile_file = NULL;
  bgpstream_debug("\tBSDS_MGR: create end");
  return datasource_mgr;
}

void bgpstream_datasource_mgr_set_data_interface(bgpstream_datasource_mgr_t *datasource_mgr,
						 const bgpstream_data_interface_id_t datasource) {
  bgpstream_debug("\tBSDS_MGR: set data interface start");
  if(datasource_mgr == NULL) {
    return; // no manager
  }
  datasource_mgr->datasource = datasource;   
  bgpstream_debug("\tBSDS_MGR: set  data interface end");
}


void bgpstream_datasource_mgr_set_data_interface_option(bgpstream_datasource_mgr_t *datasource_mgr,
				 const bgpstream_data_interface_option_t *option_type,
							const char *option_value) {
  // this option has no effect if the datasource selected is not
  // using this option
  switch(option_type->if_id)
    {
    case BGPSTREAM_DATA_INTERFACE_MYSQL:
      switch(option_type->id)
        {
        case 0:
          if(datasource_mgr->mysql_dbname!=NULL)
            {
              free(datasource_mgr->mysql_dbname);
            }
          datasource_mgr->mysql_dbname = strdup(option_value);
          break;
        case 1:
          if(datasource_mgr->mysql_user!=NULL)
            {
              free(datasource_mgr->mysql_user);
            }
          datasource_mgr->mysql_user = strdup(option_value);
          break;
        case 2:
          if(datasource_mgr->mysql_host!=NULL)
            {
              free(datasource_mgr->mysql_host);
            }
          datasource_mgr->mysql_host = strdup(option_value);
          break;
        }
      break;

    case BGPSTREAM_DATA_INTERFACE_CUSTOMLIST:
      /* no options */
      break;

    case BGPSTREAM_DATA_INTERFACE_CSVFILE:
      switch(option_type->id)
        {
        case 0:
          if(datasource_mgr->csvfile_file!=NULL)
            {
              free(datasource_mgr->csvfile_file);
            }
          datasource_mgr->csvfile_file = strdup(option_value);
          break;
        }
      break;
    }
}


void bgpstream_datasource_mgr_init(bgpstream_datasource_mgr_t *datasource_mgr,
				   bgpstream_filter_mgr_t *filter_mgr){
  bgpstream_debug("\tBSDS_MGR: init start");
  if(datasource_mgr == NULL) {
    return; // no manager
  }
  // datasource_mgr->blocking can be set at any time
  if (datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_MYSQL) {
    datasource_mgr->mysql_ds = bgpstream_mysql_datasource_create(filter_mgr, 
								 datasource_mgr->mysql_dbname,
								 datasource_mgr->mysql_user,
								 datasource_mgr->mysql_host);
    if(datasource_mgr->mysql_ds == NULL) {
      datasource_mgr->status = DS_ERROR;
    } 
    else {
      datasource_mgr->status = DS_ON;
    }
  }
  if (datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_CUSTOMLIST) {
    datasource_mgr->customlist_ds = bgpstream_customlist_datasource_create(filter_mgr);
    if(datasource_mgr->customlist_ds == NULL) {
      datasource_mgr->status = DS_ERROR;
    } 
    else {
      datasource_mgr->status = DS_ON;
    }
  }
  if (datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_CSVFILE) {
    datasource_mgr->csvfile_ds = bgpstream_csvfile_datasource_create(filter_mgr,
								     datasource_mgr->csvfile_file);
    if(datasource_mgr->csvfile_ds == NULL) {
      datasource_mgr->status = DS_ERROR;
    } 
    else {
      datasource_mgr->status = DS_ON;
    }
  }
  // if none of the datasources is matched the status of the DS is not set to ON
  bgpstream_debug("\tBSDS_MGR: init end");
}


void bgpstream_datasource_mgr_set_blocking(bgpstream_datasource_mgr_t *datasource_mgr){
  bgpstream_debug("\tBSDS_MGR: set blocking start");
  if(datasource_mgr == NULL) {
    return; // no manager
  }
  datasource_mgr->blocking = 1;
  bgpstream_debug("\tBSDS_MGR: set blocking end");
}


int bgpstream_datasource_mgr_update_input_queue(bgpstream_datasource_mgr_t *datasource_mgr,
						bgpstream_input_mgr_t *input_mgr) {
  bgpstream_debug("\tBSDS_MGR: get data start");
  if(datasource_mgr == NULL) {
    return -1; // no datasource manager
  }
  int results = -1;
  if(datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_MYSQL) {
    do{
      results = bgpstream_mysql_datasource_update_input_queue(datasource_mgr->mysql_ds, input_mgr);
      if(results == 0 && datasource_mgr->blocking) {
	// results = 0 => 2+ time and database did not give any error
	sleep(datasource_mgr->backoff_time);
	datasource_mgr->backoff_time = datasource_mgr->backoff_time * 2;
	if(datasource_mgr->backoff_time > bs_max_wait) {
	  datasource_mgr->backoff_time = bs_max_wait;
	}
      }
      bgpstream_debug("\tBSDS_MGR: got %d (blocking: %d)", results, datasource_mgr->blocking);
    } while(datasource_mgr->blocking && results == 0);
    // if we received something we reset the backoff time
    if(datasource_mgr->blocking) {
      datasource_mgr->backoff_time = bs_min_wait;
    }
  }
  if(datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_CUSTOMLIST) {
    results = bgpstream_customlist_datasource_update_input_queue(datasource_mgr->customlist_ds, input_mgr);
    bgpstream_debug("\tBSDS_MGR: got %d (blocking: %d)", results, datasource_mgr->blocking);
  }
  if(datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_CSVFILE) {
    results = bgpstream_csvfile_datasource_update_input_queue(datasource_mgr->csvfile_ds, input_mgr);
    bgpstream_debug("\tBSDS_MGR: got %d (blocking: %d)", results, datasource_mgr->blocking);
  }

  bgpstream_debug("\tBSDS_MGR: get data end");
  return results; 
}


void bgpstream_datasource_mgr_close(bgpstream_datasource_mgr_t *datasource_mgr) {
  bgpstream_debug("\tBSDS_MGR: close start");
  if(datasource_mgr == NULL) {
    return; // no manager to destroy
  }
  if (datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_MYSQL) {
    bgpstream_mysql_datasource_destroy(datasource_mgr->mysql_ds);
    datasource_mgr->mysql_ds = NULL;
  }
  if (datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_CUSTOMLIST) {
    bgpstream_customlist_datasource_destroy(datasource_mgr->customlist_ds);
    datasource_mgr->customlist_ds = NULL;
  }
  if (datasource_mgr->datasource == BGPSTREAM_DATA_INTERFACE_CSVFILE) {
    bgpstream_csvfile_datasource_destroy(datasource_mgr->csvfile_ds);
    datasource_mgr->csvfile_ds = NULL;
  }

  datasource_mgr->status = DS_OFF;
  bgpstream_debug("\tBSDS_MGR: close end");
}


void bgpstream_datasource_mgr_destroy(bgpstream_datasource_mgr_t *datasource_mgr) {
  bgpstream_debug("\tBSDS_MGR: destroy start");
  if(datasource_mgr == NULL) {
    return; // no manager to destroy
  }
  // destroy any active datasource (if they have not been destroyed before)
  if(datasource_mgr->mysql_ds != NULL) {
    bgpstream_mysql_datasource_destroy(datasource_mgr->mysql_ds);
    datasource_mgr->mysql_ds = NULL;
  }
  if(datasource_mgr->customlist_ds != NULL) {
    bgpstream_customlist_datasource_destroy(datasource_mgr->customlist_ds);
    datasource_mgr->customlist_ds = NULL;
  }
  if(datasource_mgr->csvfile_ds != NULL) {
    bgpstream_csvfile_datasource_destroy(datasource_mgr->csvfile_ds);
    datasource_mgr->csvfile_ds = NULL;
  }
  // destroy memory allocated for options
  if(datasource_mgr->mysql_dbname!=NULL)
    {
      free(datasource_mgr->mysql_dbname);
    }
  if(datasource_mgr->mysql_user!=NULL)
    {
      free(datasource_mgr->mysql_user);
    }
  if(datasource_mgr->mysql_host!=NULL)
    {
      free(datasource_mgr->mysql_host);
    }
  if(datasource_mgr->csvfile_file!=NULL)
    {
      free(datasource_mgr->csvfile_file);
    }
  free(datasource_mgr);  
  bgpstream_debug("\tBSDS_MGR: destroy end");
}



/* ----------- customlist related functions ----------- */

static bgpstream_customlist_datasource_t *bgpstream_customlist_datasource_create(bgpstream_filter_mgr_t *filter_mgr) {
  bgpstream_debug("\t\tBSDS_CLIST: create customlist_ds start");  
  bgpstream_customlist_datasource_t *customlist_ds = (bgpstream_customlist_datasource_t*) malloc(sizeof(bgpstream_customlist_datasource_t));
  if(customlist_ds == NULL) {
    bgpstream_log_err("\t\tBSDS_CLIST: create customlist_ds can't allocate memory");    
    return NULL; // can't allocate memory
  }
  customlist_ds->filter_mgr = filter_mgr;
  customlist_ds->list_read = 0;
  bgpstream_debug("\t\tBSDS_CLIST: create customlist_ds end");
  return customlist_ds;
}


static bool bgpstream_customlist_datasource_filter_ok(bgpstream_customlist_datasource_t* customlist_ds) {
  bgpstream_debug("\t\tBSDS_CLIST: customlist_ds apply filter start");  
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
      if(customlist_ds->filetime >= (tif->begin_time - 15*60 - 120) &&
	 customlist_ds->filetime <= tif->end_time) {
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
    bgpstream_debug("\t\tBSDS_CLIST: customlist_ds update input queue start");  
    int num_results = 0;       
    // if list has not been read yet, then we push these files in the input queue
    if(customlist_ds->list_read == 0) {

      // file 1:
      /* strcpy(customlist_ds->filename, "./test-dumps/routeviews.route-views.jinx.ribs.1401487200.bz2"); */
      /* strcpy(customlist_ds->project, "routeviews"); */
      /* strcpy(customlist_ds->collector, "route-views.jinx"); */
      /* strcpy(customlist_ds->bgp_type, "ribs"); */
      /* customlist_ds->filetime = 1401487200; */
      /* if(bgpstream_customlist_datasource_filter_ok(customlist_ds)){ */
      /* 	num_results += bgpstream_input_mgr_push_sorted_input(input_mgr, strdup(customlist_ds->filename), */
      /* 							     strdup(customlist_ds->project), */
      /* 							     strdup(customlist_ds->collector), */
      /* 							     strdup(customlist_ds->bgp_type), */
      /* 							     customlist_ds->filetime); */
      /* } */
      // file 2:
      strcpy(customlist_ds->filename, "./test-dumps/routeviews.route-views.jinx.updates.1401493500.bz2");
      strcpy(customlist_ds->project, "routeviews");
      strcpy(customlist_ds->collector, "route-views.jinx");
      strcpy(customlist_ds->bgp_type, "updates");
      customlist_ds->filetime = 1401493500;
      if(bgpstream_customlist_datasource_filter_ok(customlist_ds)){
	num_results += bgpstream_input_mgr_push_sorted_input(input_mgr, strdup(customlist_ds->filename),
							     strdup(customlist_ds->project),
							     strdup(customlist_ds->collector),
							     strdup(customlist_ds->bgp_type),
							     customlist_ds->filetime);
      }
      // file 3:
      /* strcpy(customlist_ds->filename, "./test-dumps/ris.rrc06.ribs.1400544000.gz"); */
      /* strcpy(customlist_ds->project, "ris"); */
      /* strcpy(customlist_ds->collector, "rrc06"); */
      /* strcpy(customlist_ds->bgp_type, "ribs"); */
      /* customlist_ds->filetime = 1400544000; */
      /* if(bgpstream_customlist_datasource_filter_ok(customlist_ds)){ */
      /* 	num_results += bgpstream_input_mgr_push_sorted_input(input_mgr, strdup(customlist_ds->filename), */
      /* 							     strdup(customlist_ds->project), */
      /* 							     strdup(customlist_ds->collector), */
      /* 							     strdup(customlist_ds->bgp_type), */
      /* 							     customlist_ds->filetime); */
      /* } */
      // file 4:
      strcpy(customlist_ds->filename, "./test-dumps/ris.rrc06.updates.1401488100.gz");
      strcpy(customlist_ds->project, "ris");
      strcpy(customlist_ds->collector, "rrc06");
      strcpy(customlist_ds->bgp_type, "updates");
      customlist_ds->filetime = 1401488100;
      if(bgpstream_customlist_datasource_filter_ok(customlist_ds)){
	num_results += bgpstream_input_mgr_push_sorted_input(input_mgr, strdup(customlist_ds->filename),
							     strdup(customlist_ds->project),
							     strdup(customlist_ds->collector),
							     strdup(customlist_ds->bgp_type),
							     customlist_ds->filetime);
      }
      // end of files
    }
    customlist_ds->list_read = 1;
    bgpstream_debug("\t\tBSDS_CLIST: customlist_ds update input queue end");  
    return num_results;
}


static void bgpstream_customlist_datasource_destroy(bgpstream_customlist_datasource_t* customlist_ds) {
  bgpstream_debug("\t\tBSDS_CLIST: destroy customlist_ds start");  
  if(customlist_ds == NULL) {
    return; // nothing to destroy
  }
  customlist_ds->filter_mgr = NULL;
  customlist_ds->list_read = 0;
  free(customlist_ds);
  bgpstream_debug("\t\tBSDS_CLIST: destroy customlist_ds end");  
}


/* ----------- csvfile related functions ----------- */

static bgpstream_csvfile_datasource_t *bgpstream_csvfile_datasource_create(bgpstream_filter_mgr_t *filter_mgr, 
									   char *csvfile_file) {
  bgpstream_debug("\t\tBSDS_CSVFILE: create csvfile_ds start");  
  bgpstream_csvfile_datasource_t *csvfile_ds = (bgpstream_csvfile_datasource_t*) malloc(sizeof(bgpstream_csvfile_datasource_t));
  if(csvfile_ds == NULL) {
    bgpstream_log_err("\t\tBSDS_CSVFILE: create csvfile_ds can't allocate memory");    
    return NULL; // can't allocate memory
  }
  if(csvfile_file == NULL)
    {
      csvfile_ds->csvfile_file = strdup("/Users/chiara/Desktop/local_db/bgp_data.csv");
    }
  else
    {
      csvfile_ds->csvfile_file = strdup(csvfile_file);
    }
  csvfile_ds->filter_mgr = filter_mgr;
  csvfile_ds->csvfile_read = 0;
  bgpstream_debug("\t\tBSDS_CSVFILE: create csvfile_ds end");
  return csvfile_ds;
}


static bool bgpstream_csvfile_datasource_filter_ok(bgpstream_csvfile_datasource_t* csvfile_ds) {
  bgpstream_debug("\t\tBSDS_CSVFILE: csvfile_ds apply filter start");  
  bgpstream_string_filter_t * sf;
  bgpstream_interval_filter_t * tif;
  bool all_false;
  // projects
  all_false = true;
  if(csvfile_ds->filter_mgr->projects != NULL) {
    sf = csvfile_ds->filter_mgr->projects;
    while(sf != NULL) {
      if(strcmp(sf->value, csvfile_ds->project) == 0) {
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
  if(csvfile_ds->filter_mgr->collectors != NULL) {
    sf = csvfile_ds->filter_mgr->collectors;
    while(sf != NULL) {
      if(strcmp(sf->value, csvfile_ds->collector) == 0) {
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
  if(csvfile_ds->filter_mgr->bgp_types != NULL) {
    sf = csvfile_ds->filter_mgr->bgp_types;
    while(sf != NULL) {
      if(strcmp(sf->value, csvfile_ds->bgp_type) == 0) {
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
  if(csvfile_ds->filter_mgr->time_intervals != NULL) {
    tif = csvfile_ds->filter_mgr->time_intervals;
    while(tif != NULL) {      
      // filetime (we consider 15 mins before to consider routeviews updates
      // and 120 seconds to have some margins)
      if(csvfile_ds->filetime >= (tif->begin_time - 15*60 - 120) &&
	 csvfile_ds->filetime <= tif->end_time) {
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


static int bgpstream_csvfile_datasource_update_input_queue(bgpstream_csvfile_datasource_t* csvfile_ds,
							   bgpstream_input_mgr_t *input_mgr) {
  bgpstream_debug("\t\tBSDS_CSVFILE: csvfile_ds update input queue start");  
  int num_results = 0;       
  FILE * stream;
  // char line[1024];
  char * line = NULL;
  char * tok;
  int i;
  // if list has not been read yet, then we push these files in the input queue
  if(csvfile_ds->csvfile_read == 0) {
    stream = fopen(csvfile_ds->csvfile_file, "r");
    // stream = fopen("/Users/chiara/Desktop/local_db/bgp_data.csv", "r");
    //stream = fopen("/scratch/satc/chiaras_test/local_db/bgp_data.csv", "r");
    if(stream != NULL) {
      /* The flockfile function acquires the internal locking object associated
       * with the stream stream. This ensures that no other thread can explicitly
       * through flockfile/ftrylockfile or implicit through a call of a stream 
       * function lock the stream. The thread will block until the lock is acquired. 
       */
      flock(fileno(stream),LOCK_EX);
      fseek (stream , 0, SEEK_SET);
      line = (char *) malloc(1024 * sizeof(char));
      while (fgets(line, 1024, stream)) {
	// printf("%s\n", line);
	i = 0;
	while((tok = strsep(&line, ",")) != NULL) {
	  // printf("%s\n", tok);	
	  switch(i) {
	  case 0:
	    strcpy(csvfile_ds->filename, tok);	  
	    break;
	  case 1:
	    strcpy(csvfile_ds->project, tok);	  
	    break;
	  case 2:
	    strcpy(csvfile_ds->bgp_type, tok);	  
	    break;
	  case 3:
	    strcpy(csvfile_ds->collector, tok);	  
	    break;
	  case 4:
	    csvfile_ds->filetime = atoi(tok);	
	    break;
	  default:
	    continue;
	  }
	  ++i;	  
	}
	// printf("%s = %s = %s = %d\n", csvfile_ds->filename, csvfile_ds->bgp_type, csvfile_ds->collector, csvfile_ds->filetime);
	if(bgpstream_csvfile_datasource_filter_ok(csvfile_ds)){
	  num_results += bgpstream_input_mgr_push_sorted_input(input_mgr,
							       strdup(csvfile_ds->filename),
							       strdup(csvfile_ds->project),
							       strdup(csvfile_ds->collector),
							       strdup(csvfile_ds->bgp_type),
							       csvfile_ds->filetime);
	}
	line = realloc(line,1024 * sizeof(char));      	  	

      }
      free(line);
      funlockfile(stream);
      fclose(stream);
    }    
  }
  csvfile_ds->csvfile_read = 1;
  bgpstream_debug("\t\tBSDS_CSVFILE: csvfile_ds update input queue end");  
  return num_results;
}


static void bgpstream_csvfile_datasource_destroy(bgpstream_csvfile_datasource_t* csvfile_ds) {
  bgpstream_debug("\t\tBSDS_CSVFILE: destroy csvfile_ds start");  
  if(csvfile_ds == NULL) {
    return; // nothing to destroy
  }
  csvfile_ds->filter_mgr = NULL;
  csvfile_ds->csvfile_read = 0;
  if(csvfile_ds->csvfile_file !=NULL)
    {
      free(csvfile_ds->csvfile_file);
    }
  free(csvfile_ds);
  bgpstream_debug("\t\tBSDS_CSVFILE: destroy csvfile_ds end");  
}


/* ----------- mysql related functions ----------- */

static bgpstream_mysql_datasource_t *bgpstream_mysql_datasource_create(bgpstream_filter_mgr_t *filter_mgr,
								       char *mysql_dbname,
								       char *mysql_user,
								       char *mysql_host) {
  bgpstream_debug("\t\tBSDS_MYSQL: create mysql_ds start");
  bgpstream_mysql_datasource_t *mysql_ds = (bgpstream_mysql_datasource_t*) malloc(sizeof(bgpstream_mysql_datasource_t));
  if(mysql_ds == NULL) {
    return NULL; // can't allocate memory
  }
  // set up options or provide defaults
  if(mysql_dbname == NULL)
    {
      mysql_ds->mysql_dbname = strdup("bgparchive");      
    }
  else 
    {
      mysql_ds->mysql_dbname = strdup(mysql_dbname);
    }
  if(mysql_user == NULL)
    {
      mysql_ds->mysql_user = strdup("bgpstream");      
    }
  else 
    {
      mysql_ds->mysql_user = strdup(mysql_user);
    }
  if(mysql_host == NULL)
    {
      mysql_ds->mysql_host = strdup("localhost");      
    }
  else 
    {
      mysql_ds->mysql_host = strdup(mysql_host);
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
  // Establish a connection to the database
  bgpstream_debug("\t\tBSDS_MYSQL: create mysql_ds mysql connection establishment");
  if (mysql_real_connect(mysql_ds->mysql_con, mysql_ds->mysql_host, mysql_ds->mysql_user, NULL, 
			     mysql_ds->mysql_dbname, 0, NULL, 0) == NULL) 
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
	 "file_time "
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
  // comment on 120 seconds:
  // sometimes it happens that ribs or updates carry a filetime which is not
  // compliant with the expected filetime (e.g. :
  //  rib.23.59 instead of rib.00.00
  // in order to compensate for this kind of situations we 
  // retrieve data that are 120 seconds older than the requested 

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

static char *build_filename(bgpstream_mysql_datasource_t *ds) {
  char *filename = NULL;
  char date[11]; /* "YYYY/MM/DD" */
  long tmp = ds->filetime_res;
  struct tm time_result;
  // gmtime_r is the thread_safe version of gmtime
  if(strftime(date, 11, "%Y/%m/%d", gmtime_r(&tmp,&time_result)) == 0) {
    return NULL;
  }

  asprintf(&filename, "%s/%s/%s/%s/%s.%s.%s.%d.%s",
	   ds->proj_path_res, ds->coll_path_res,
	   ds->type_path_res, date,
	   ds->proj_name_res, ds->coll_name_res,
	   ds->type_name_res, ds->filetime_res,
	   ds->file_ext_res
	   );

  return filename;
}


static int bgpstream_mysql_datasource_update_input_queue(bgpstream_mysql_datasource_t* mysql_ds,
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
  /* Try to execute the statement */
  int stmt_ret = mysql_stmt_execute(mysql_ds->stmt);
  int attempts = 0;
  int max_attempts = 30; // 30 ~ number of collectors
  while(stmt_ret != 0 && attempts < max_attempts)
    {
      attempts++;

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
	  if (mysql_real_connect(mysql_ds->mysql_con, mysql_ds->mysql_host, mysql_ds->mysql_user, NULL, 
				 mysql_ds->mysql_dbname, 0, NULL, 0) != NULL)	
	    {
	      if(mysql_query(mysql_ds->mysql_con, "set time_zone='+0:0'") == 0)
		{
		  if((mysql_ds->stmt = mysql_stmt_init(mysql_ds->mysql_con)) != NULL)
		    {
		      if (mysql_stmt_prepare(mysql_ds->stmt, mysql_ds->sql_query, strlen(mysql_ds->sql_query)) == 0)
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
					    mysql_ds->filetime_res
					    );
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


static void bgpstream_mysql_datasource_destroy(bgpstream_mysql_datasource_t* mysql_ds) {
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
  if(mysql_ds->mysql_host != NULL)
    {
      free(mysql_ds->mysql_host);
    }
  free(mysql_ds);
  bgpstream_debug("\t\tBSDS_MYSQL: destroy mysql_ds end");
  return;
}

