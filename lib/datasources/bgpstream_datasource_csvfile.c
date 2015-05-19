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

#include "bgpstream_datasource_csvfile.h"
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



struct struct_bgpstream_csvfile_datasource_t {
  char *csvfile_file;
  bgpstream_filter_mgr_t * filter_mgr;
  char filename[BGPSTREAM_DUMP_MAX_LEN];
  char project[BGPSTREAM_PAR_MAX_LEN];
  char collector[BGPSTREAM_PAR_MAX_LEN];
  char bgp_type[BGPSTREAM_PAR_MAX_LEN];
  uint32_t filetime;
  uint32_t last_ts;
  uint32_t time_span;
};



bgpstream_csvfile_datasource_t *bgpstream_csvfile_datasource_create(bgpstream_filter_mgr_t *filter_mgr, 
                                                                    char *csvfile_file) {
  bgpstream_debug("\t\tBSDS_CSVFILE: create csvfile_ds start");  
  bgpstream_csvfile_datasource_t *csvfile_ds = (bgpstream_csvfile_datasource_t*) malloc_zero(sizeof(bgpstream_csvfile_datasource_t));
  if(csvfile_ds == NULL) {
    bgpstream_log_err("\t\tBSDS_CSVFILE: create csvfile_ds can't allocate memory");    
    return NULL; // can't allocate memory
  }  
  if(csvfile_file == NULL)
    {
      bgpstream_log_err("\t\tBSDS_CSVFILE: create csvfile_ds no file provided");    
      free(csvfile_ds);
      return NULL;
    }
  csvfile_ds->filter_mgr = filter_mgr;
  csvfile_ds->last_ts = 0;
  
  bgpstream_debug("\t\tBSDS_CSVFILE: create csvfile_ds end");
  return csvfile_ds;
}


static bool
bgpstream_csvfile_datasource_filter_ok(bgpstream_csvfile_datasource_t* csvfile_ds) {
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


int
bgpstream_csvfile_datasource_update_input_queue(bgpstream_csvfile_datasource_t* csvfile_ds,
                                                bgpstream_input_mgr_t *input_mgr) {
  bgpstream_debug("\t\tBSDS_CSVFILE: csvfile_ds update input queue start");  
  int num_results = 0;       
  FILE * stream;
  char * line = NULL;
  char * tok;
  int i;
  uint32_t min_ts = csvfile_ds->last_ts;
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint32_t max_ts  = tv.tv_sec - 1;
  uint32_t ts;
  /* every time we read the entire file and check for lines with a timestamp not */
  /* processed yet */
  stream = fopen(csvfile_ds->csvfile_file, "r");
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
	  case 5:
	    csvfile_ds->time_span = atoi(tok);	
	    break;
	  case 6:
	    ts = atoi(tok);	
	    break;
	  default:
	    continue;
	  }
	  ++i;	  
	}

        if( ts > min_ts && ts <= max_ts)
          {
            if(ts > csvfile_ds->last_ts)
              {
                csvfile_ds->last_ts = ts;
              }
            if(bgpstream_csvfile_datasource_filter_ok(csvfile_ds)){
              num_results += bgpstream_input_mgr_push_sorted_input(input_mgr,
                                                                   strdup(csvfile_ds->filename),
                                                                   strdup(csvfile_ds->project),
                                                                   strdup(csvfile_ds->collector),
                                                                   strdup(csvfile_ds->bgp_type),
                                                                   csvfile_ds->filetime,
                                                                   csvfile_ds->time_span);
            }
          }
	line = realloc(line,1024 * sizeof(char));      	  	
      }
      free(line);
      funlockfile(stream);
      fclose(stream);
    }    
  bgpstream_debug("\t\tBSDS_CSVFILE: csvfile_ds update input queue end");  
  return num_results;
}


void
bgpstream_csvfile_datasource_destroy(bgpstream_csvfile_datasource_t* csvfile_ds) {
  bgpstream_debug("\t\tBSDS_CSVFILE: destroy csvfile_ds start");  
  if(csvfile_ds == NULL) {
    return; // nothing to destroy
  }
  csvfile_ds->filter_mgr = NULL;
  if(csvfile_ds->csvfile_file !=NULL)
    {
      free(csvfile_ds->csvfile_file);
    }
  free(csvfile_ds);
  bgpstream_debug("\t\tBSDS_CSVFILE: destroy csvfile_ds end");  
}

