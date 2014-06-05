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
 * libbgpstream is distributed in the hope that it will be usefuql,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libbgpstream.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "bgpstream_lib.h"
#include <stdio.h>
#include <time.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>

// modified bgpdump process
void bgpdump_process(BGPDUMP_ENTRY *my_entry);


int main(){
  
  // allocate memory for interface
  bgpstream_t * const bs = bgpstream_create();
  if(!bs) {
    printf("Not able to create bs\n");   
    return 1;
  }

  // test case: 1
  // start -> Fri May 30 17:41:40 UTC 2014
  // bgpstream_set_filter(bs, "time_interval_start", "1401471700");
  // stop -> Fri May 30 18:21:00 UTC 2014
  // bgpstream_set_filter(bs, "time_interval_stop", "1401474060");


  // test case: 2
  // start -> Tue, 31 Dec 2013 23:29:00 GMT
  bgpstream_set_filter(bs, "time_interval_start", "1388532540");
  // stop -> Wed, 01 Jan 2014 00:39:00 GMT
  bgpstream_set_filter(bs, "time_interval_stop", "1388536740");



  // other option
  //bgpstream_set_filter(bs, "project", "ris");
  //bgpstream_set_filter(bs, "project", "routeviews");
  //bgpstream_set_filter(bs, "collector", "route-views.saopaulo");
  //bgpstream_set_filter(bs, "bgp_type", "updates");
  //bgpstream_set_filter(bs, "time_interval_start", "1401493500");
  //bgpstream_set_filter(bs, "bgp_type", "ribs");
  //bgpstream_set_filter(bs, "time_interval_start", "1401472800");
  //bgpstream_set_filter(bs, "time_interval_stop", "1401474060");
  

  // set filters
  //bgpstream_set_filter(bs, "project", "routeviews");
  //bgpstream_set_filter(bs, "collector", "route-views2");
  //bgpstream_set_filter(bs, "bgp_type", "updates");
  //bgpstream_set_filter(bs, "bgp_type", "ribs");
  //bgpstream_set_filter(bs, "time_interval_start", "1401472800");
  //bgpstream_set_filter(bs, "time_interval_stop", "1401474060");
  //bgpstream_set_filter(bs, "time_interval_stop", "1357002000");
  
  // set blocking
  //bgpstream_set_blocking(bs);

  // turn on interface and set the datasource!
  //int init_res = bgpstream_init(bs, "customlist");
  int init_res = bgpstream_init(bs, "mysql");
  
  if(init_res <= 0) {
    printf("Not able to turn on bs\n");   
    // deallocate memory for interface
    bgpstream_destroy(bs);
    return 1;
  }

  size_t record_size; 
  int read = 0;
  int get_next_ret = 0;
  int counter = 0;
  char rstatus[50];
  strcpy(rstatus, "");

  // allocate memory for bs_record  
  bgpstream_record_t * const bs_record = bgpstream_create_record();
  if(bs_record != NULL) {
    do {
      get_next_ret = bgpstream_get_next(bs, bs_record);      
      counter++;
      if(get_next_ret > 0) {	
	if(bs_record->status == VALID_RECORD) {
	  strcpy(rstatus, "VALID_RECORD");
	  if(bs_record->bd_entry != NULL) {
	    read++;
	    printf("%d\t%ld\t%ld\t%s\t%s\t%s\n", 
		   counter, 
		   bs_record->attributes.record_time,
		   bs_record->attributes.dump_time,
		   bs_record->attributes.dump_type, 
		   bs_record->attributes.dump_collector,
		   rstatus);
	    
	    // record_size = sizeof((bs_record->bd_entry)->body);
	    // printf("\t\t\t--------------> 1 record of size: %zu READ\n", record_size);   

	    // process entry and get bgpdump output
	    //bgpdump_process(bs_record->bd_entry);
	  }	  
	}
	else {
	  /* 
	   *  printf("%s - %s - %s - %ld - %ld - ", bs_record->attributes.dump_project, 
	   *     bs_record->attributes.dump_collector, bs_record->attributes.dump_type,
	   *     bs_record->attributes.dump_time, bs_record->attributes.record_time);	  
	   */
	  switch(bs_record->status){
	  case CORRUPTED_RECORD:
	    strcpy(rstatus, "CORRUPTED_RECORD");
	    break;
	  case FILTERED_SOURCE:
	    strcpy(rstatus, "FILTERED_SOURCE");
	    break;
	  case EMPTY_SOURCE:
	    strcpy(rstatus, "EMPTY_SOURCE");
	    break;
	  case CORRUPTED_SOURCE:
	    strcpy(rstatus, "CORRUPTED_SOURCE");
	    break;	  
	  default:
	    strcpy(rstatus, "WEIRD");
	  }
	  printf("%d\t%ld\t%ld\t%s\t%s\t%s\n", 
		 counter, 
		 bs_record->attributes.record_time,
		 bs_record->attributes.dump_time,
		 bs_record->attributes.dump_type, 
		 bs_record->attributes.dump_collector,
		 rstatus);	  
	}
      }
    } while(get_next_ret > 0);    
  }

  // de-allocate memory for bs_record
  bgpstream_destroy_record(bs_record);

  // turn off interface
  bgpstream_close(bs);
  
  // deallocate memory for interface
  bgpstream_destroy(bs);

  printf("Read %d values\n", read);

  return 0;
}
