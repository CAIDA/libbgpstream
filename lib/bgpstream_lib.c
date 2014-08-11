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

#include <stdio.h>
#include <stdlib.h>
#include <bgpdump_lib.h>

#include "bgpstream_lib.h"

#include "bgpstream_int.h"
#include "bgpstream_debug.h"


/* allocate memory for a new bgpstream interface 
 */
bgpstream_t *bgpstream_create() {
  debug("BS: create start");
  bgpstream_t *  bs = (bgpstream_t*) malloc(sizeof(bgpstream_t));
  if(bs == NULL) {
    return NULL; // can't allocate memory
  }
  bs->filter_mgr = bgpstream_filter_mgr_create(); 
  if(bs->filter_mgr == NULL) {
    bgpstream_destroy(bs);
    bs = NULL;
    return NULL;
  }
  bs->datasource_mgr = bgpstream_datasource_mgr_create(); 
  if(bs->datasource_mgr == NULL) {
    bgpstream_destroy(bs);
    return NULL;
  }
  /* create an empty input mgr
   * the input queue will be populated when a
   * bgpstream record is requested */
  bs->input_mgr = bgpstream_input_mgr_create();
  if(bs->input_mgr == NULL) {
    bgpstream_destroy(bs);
    bs = NULL;
    return NULL;
  }
  bs->reader_mgr = bgpstream_reader_mgr_create(bs->filter_mgr); 
  if(bs->reader_mgr == NULL) {
    bgpstream_destroy(bs);
    bs = NULL;
    return NULL;
  }
  /* memory for the bgpstream interface has been
   * allocated correctly */
  bs->status = ALLOCATED;
  debug("BS: create end");
  return bs;
}
/* side note: filters are part of the bgpstream so they
 * can be accessed both from the input_mgr and the
 * reader_mgr (input_mgr use them to apply a coarse-grained 
 * filtering, the reader_mgr applies a fine-grained filtering
 * of the data provided by the input_mgr)
 */



/* configure filters in order to select a subset of the bgp data available */
void bgpstream_add_filter(bgpstream_t * const bs, bgpstream_filter_type filter_type,
			  const char* filter_value){
  debug("BS: set_filter start");
  if(bs == NULL || (bs != NULL && bs->status != ALLOCATED)) {
    return; // nothing to customize
  }
  bgpstream_filter_mgr_filter_add(bs->filter_mgr, filter_type, filter_value);
  debug("BS: set_filter end");
}


void bgpstream_add_interval_filter(bgpstream_t * const bs, bgpstream_filter_type filter_type,
				   const char* filter_start, const char* filter_stop){
  debug("BS: set_filter start");
  if(bs == NULL || (bs != NULL && bs->status != ALLOCATED)) {
    return; // nothing to customize
  }
  bgpstream_filter_mgr_interval_filter_add(bs->filter_mgr, filter_type, filter_start, filter_stop);
  debug("BS: set_filter end");
}


/* configure the interface so that it connects
 * to a specific datasource interface
 */
void bgpstream_set_data_interface(bgpstream_t * const bs, const bgpstream_datasource_type datasource) {
  debug("BS: set_data_interface start");
  if(bs == NULL || (bs != NULL && bs->status != ALLOCATED)) {
    return; // nothing to customize
  }
  bgpstream_datasource_mgr_set_data_interface(bs->datasource_mgr, datasource);
  debug("BS: set_data_interface stop");
}


/* configure the interface so that it blocks
 * waiting for new data 
 */
void bgpstream_set_blocking(bgpstream_t * const bs) {
  debug("BS: set_blocking start");
  if(bs == NULL || (bs != NULL && bs->status != ALLOCATED)) {
    return; // nothing to customize
  }
  bgpstream_datasource_mgr_set_blocking(bs->datasource_mgr);
  debug("BS: set_blocking stop");
}


/* turn on the bgpstream interface, i.e.: 
 * it makes the interface ready
 * for a new get next call 
*/
int bgpstream_init(bgpstream_t * const bs) {
  debug("BS: init start");
  if(bs == NULL || (bs != NULL && bs->status != ALLOCATED)) {
    return 0; // nothing to init
  }
  // turn on datasource interface
  bgpstream_datasource_mgr_init(bs->datasource_mgr, bs->filter_mgr);
  if(bs->datasource_mgr->status == DS_ON) {
    bs->status = ON; // interface is on
    debug("BS: init end: ok");
    return 1;
  }
  else{
    bs->status = ALLOCATED; // interface is not on (something wrong with datasource)
    debug("BS: init warning: check if the datasource provided is ok");
    debug("BS: init end: not ok");
    return -1;
  }
}


/* allocate memory for a bs_record */
bgpstream_record_t *bgpstream_create_record() {
  debug("BS: create record start");
  bgpstream_record_t *bs_record = (bgpstream_record_t*) malloc(sizeof(bgpstream_record_t));
  if(bs_record == NULL) {
    return NULL; // can't allocate memory
  }
  bs_record->bd_entry = NULL;
  bs_record->status = EMPTY_SOURCE;
  bs_record->dump_pos = DUMP_START;
  strcpy(bs_record->attributes.dump_project, "");
  strcpy(bs_record->attributes.dump_collector, "");
  bs_record->attributes.dump_type = BGPSTREAM_UPDATE;
  bs_record->attributes.dump_time = 0;
  bs_record->attributes.record_time = 0;
  debug("BS: create record end");
  return bs_record;
}


/* free memory associated to a bs_record  */
void bgpstream_destroy_record(bgpstream_record_t * const bs_record){
  debug("BS: destroy record start");
  if(bs_record == NULL) {
    debug("BS: record destroy end");
    return; // nothing to do
  }
  if(bs_record->bd_entry != NULL){
    debug("BS - free bs_record->bgpdump_entry");
    bgpdump_free_mem(bs_record->bd_entry);
    bs_record->bd_entry = NULL;
  }
  debug("BS - free bs_record");
  free(bs_record);
  debug("BS: destroy record end");
}



/* this function returns the next available record read
 * if the input_queue (i.e. list of files connected from
 * an external source) or the reader_cqueue (i.e. list
 * of bgpdump currently open) are empty then it 
 * triggers a mechanism to populate the queues or
 * return 0 if nothing is available 
 */
int bgpstream_get_next_record(bgpstream_t * const bs, bgpstream_record_t * const bs_record) {
  debug("BS: get next");
  if(bs == NULL || (bs != NULL && bs->status != ON)) {
  return -1; // wrong status
  }
  // bgpstream_record_t *bs_record = NULL;
  int num_query_results = 0;
  bgpstream_input_t *bs_in = NULL;
  while(bgpstream_reader_mgr_is_empty(bs->reader_mgr)) {
    debug("BS: reader mgr is empty");
    // get new data to process and set the reader_mgr
    while(bgpstream_input_mgr_is_empty(bs->input_mgr)) {
      debug("BS: input mgr is empty");
      /* query the external source and append new
       * input objects to the input_mgr queue */
      num_query_results = bgpstream_datasource_mgr_update_input_queue(bs->datasource_mgr,
								      bs->input_mgr);
      if(num_query_results == 0){
	debug("BS: no (more) data are available");
	return 0; // no (more) data are available
      }
      debug("BS: got results from datasource");
    }
    debug("BS: input mgr not empty");
    bs_in = bgpstream_input_mgr_get_queue_to_process(bs->input_mgr);
    bgpstream_reader_mgr_add(bs->reader_mgr, bs_in, bs->filter_mgr);
    bgpstream_input_mgr_destroy_queue(bs_in);        
    bs_in = NULL;
  }
  debug("BS: reader mgr not empty");  
  return bgpstream_reader_mgr_get_next_record(bs->reader_mgr, bs_record, bs->filter_mgr);
}


/* turn off the bgpstream interface */
void bgpstream_close(bgpstream_t * const bs) {
  debug("BS: close start");
  if(bs == NULL || (bs != NULL && bs->status != ON)) {
    return; // nothing to close
  }
  bgpstream_datasource_mgr_close(bs->datasource_mgr);
  bs->status = OFF; // interface is off
  debug("BS: close end");

}


/* destroy a bgpstream interface istance
 */
void bgpstream_destroy(bgpstream_t * const bs){
  debug("BS: destroy start");
  if(bs == NULL) {
    return; // nothing to destroy
  }
  bgpstream_input_mgr_destroy(bs->input_mgr);
  bs->input_mgr = NULL;
  bgpstream_reader_mgr_destroy(bs->reader_mgr);
  bs->reader_mgr = NULL;
  bgpstream_filter_mgr_destroy(bs->filter_mgr);
  bs->filter_mgr = NULL;
  bgpstream_datasource_mgr_destroy(bs->datasource_mgr);
  bs->datasource_mgr = NULL;
  free(bs);
  debug("BS: destroy end");
}


