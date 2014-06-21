/*
 * libbgpstream
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * chiara@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
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
  // all projects
  // bgpstream_set_filter(bs, BS_PROJECT, "***");

  // all types
  // bgpstream_set_filter(bs, BS_BGP_TYPE, "***");

  // best collectors (delay below 25 minutes, i.e. 1500 seconds)
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views.linx");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views6");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views.saopaulo");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views.sydney");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views2");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views.perth");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views.isc");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views4");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views3");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views.telxatl");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views.nwax");
  bgpstream_add_filter(bs, BS_COLLECTOR, "route-views.wide");
  bgpstream_add_filter(bs, BS_COLLECTOR, "rrc10");
  bgpstream_add_filter(bs, BS_COLLECTOR, "rrc11");
  bgpstream_add_filter(bs, BS_COLLECTOR, "rrc12");


  bgpstream_add_interval_filter(bs, BS_TIME_INTERVAL, "1403229491", "1403236583");
  // start -> Fri, 20 Jun 2014 01:58:11 GMT
  // stop -> Fri, 20 Jun 2014 03:56:23 GMT

  // set blocking
  bgpstream_set_blocking(bs);

  // set datasource interface
  bgpstream_set_data_interface(bs, "mysql");

  // turn on interface
  int init_res = bgpstream_init(bs);
  
  if(init_res <= 0) {
    printf("Not able to turn on bs\n");   
    // deallocate memory for interface
    bgpstream_destroy(bs);
    return 1;
  }

  FILE *f = fopen("/scratch/satc/chiaras_test/june_20.txt", "w");
  if (f == NULL) {
    printf("Error opening file!\n");
    exit(1);
  }


  size_t record_size; 
  int read = 0;
  int get_next_ret = 0;
  int counter = 0;
  char rstatus[50];
  time_t result_time = time(NULL);
  strcpy(rstatus, "");


  // allocate memory for bs_record  
  bgpstream_record_t * const bs_record = bgpstream_create_record();
  if(bs_record != NULL) {
    do {
      get_next_ret = bgpstream_get_next(bs, bs_record);      
      result_time = time(NULL);
      counter++;
      if(get_next_ret > 0) {	
	if(bs_record->status == VALID_RECORD) {
	  strcpy(rstatus, "VALID_RECORD");
	  if(bs_record->bd_entry != NULL) {
	    read++;
	    fprintf(f,"%d\t%ld\t%ld\t%s\t%s\t%s\t%d\n", 
		   counter, 
		   bs_record->attributes.record_time,
		   bs_record->attributes.dump_time,
		   bs_record->attributes.dump_type, 
		   bs_record->attributes.dump_collector,
		   rstatus, (int)result_time);
	  }	  
	}
	else {
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
	  fprintf(f,"%d\t%ld\t%ld\t%s\t%s\t%s\t%d\n", 
		 counter, 
		 bs_record->attributes.record_time,
		 bs_record->attributes.dump_time,
		 bs_record->attributes.dump_type, 
		 bs_record->attributes.dump_collector,
		 rstatus, (int)result_time);	  
	}
      }
    } while(get_next_ret > 0);    
  }

  // close file
  fclose(f);

  // de-allocate memory for bs_record
  bgpstream_destroy_record(bs_record);

  // turn off interface
  bgpstream_close(bs);
  
  // deallocate memory for interface
  bgpstream_destroy(bs);


  printf("Read %d values\n", read);

  return 0;
}
