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
#include "bgpstream_lib.h"
#include "debug.h"
#include <bgpdump_lib.h>


int test_mylib(const char *filename){
  
  BGPDUMP *my_dump = bgpdump_open_dump(filename);
  if(!my_dump){
    return 1;
  }
  BGPDUMP_ENTRY *my_entry = bgpdump_read_next(my_dump);
  bgpdump_free_mem(my_entry);
  bgpdump_close_dump(my_dump);
  return 0;

}

/* create a new bgpstream interface 
 */
bgpstream_t *bgpstream_create() {
  debug("BS: create start");
  bgpstream_t *bs = (bgpstream_t*) malloc(sizeof(bgpstream_t));
  if(bs == NULL) {
    return NULL; // can't allocate memory
  }
  /* create an empty input mgr
   * the input queue will be populated when a
   * bgpstream record is requested */
  bs->input_mgr = bgpstream_input_mgr_create();
  if(bs->input_mgr == NULL) {
    bgpstream_destroy(bs);
    return NULL;
  }
  // default feeder plugin
  bgpstream_set_feeder_plugin(bs,feeder_default, "", 0, 0);
  bs->reader_mgr = bgpstream_reader_mgr_create(); 
  if(bs->reader_mgr == NULL) {
    bgpstream_destroy(bs);
    return NULL;
  }
  debug("BS: create end");
  return bs;
}


/* customize the bgpstream interface 
 * providing the right information
 * associated to the feeder plugin
 */
void bgpstream_set_feeder_plugin(bgpstream_t *bs, feeder_callback_t feeder_cb,
				 const char * const feeder_name,
				 const int min_date, const int min_ts) {
  if(bs == NULL || bs->input_mgr == NULL) {
    return; // nothing to customize
  }
  bs->input_mgr->feeder_cb = feeder_cb;
  strcpy(bs->input_mgr->feeder_name, feeder_name);
  bs->input_mgr->epoch_minimum_date = min_date;
  bs->input_mgr->epoch_last_ts_input = min_ts;  
}


/* destroy a bgpstream interface istance
 */
void bgpstream_destroy(bgpstream_t *bs){
  debug("BS: destroy start");
  if(bs == NULL) {
    return; // nothing to destroy
  }
  bgpstream_input_mgr_destroy(bs->input_mgr);
  bgpstream_reader_mgr_destroy(bs->reader_mgr);
  free(bs);
  bs = NULL;
  debug("BS: destroy end");
}



/* this function returns the next available record read
 * if the input_queue (i.e. list of files connected from
 * an external source) or the reader_cqueue (i.e. list
 * of bgpdump currently open) are empty then it 
 * triggers a mechanism to populate the queues or
 * return NULL if nothing is available 
 */
bgpstream_record_t *bgpstream_get_next(bgpstream_t *bs) {
  debug("BS: get next");
  bgpstream_record_t *bs_record = NULL;
  int num_query_results = 0;
  int bs_reader_set_ret = 0;
  while(bgpstream_reader_mgr_is_empty(bs->reader_mgr)) {
    debug("BS: reader mgr is empty");
    // get new data to process and set the reader_mgr
    while(bgpstream_input_mgr_is_empty(bs->input_mgr)) {
      debug("BS: input mgr is empty");
      /* query the external source and append new
       * input objects to the input_mgr queue */
      // num_query_results = my_empty_callback_function(bs->input_mgr);
      num_query_results = bs->input_mgr->feeder_cb(bs->input_mgr);
      if(num_query_results == 0){
	return NULL; // no data are available
      }
      debug("BS: got results from callback");
    }
    debug("BS: input mgr not empty");
    bgpstream_input_t *bs_in = bgpstream_input_get_queue_to_process(bs->input_mgr);
    bs_reader_set_ret = bgpstream_reader_mgr_set(bs->reader_mgr,bs_in);
    bgpstream_input_destroy_queue(bs_in);
    if(bs_reader_set_ret == 0) {
      // something strange is going on (the reader was empty, we try to create
      // a new cqueue to process but it fails (e.g. it was not really empty?)
      return NULL;
    }
  }
  debug("BS: reader mgr not empty");
  return bgpstream_reader_mgr_get_next_record(bs->reader_mgr);
}


/* free memory associated to a bs_record 
 */
void bgpstream_free_mem(bgpstream_record_t *bs_record){
  debug("BS: free record start");
  bgpstream_reader_destroy_record(bs_record);
  bs_record = NULL;
  debug("BS: free record end");
}
