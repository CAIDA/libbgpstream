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

#include "bgpstream_filter.h"
#include "bgpstream_debug.h"


/* allocate memory for a new bgpstream filter */
bgpstream_filter_mgr_t *bgpstream_filter_mgr_create() {
  bgpstream_debug("\tBSF_MGR: create start");
  bgpstream_filter_mgr_t *bs_filter_mgr = (bgpstream_filter_mgr_t*) malloc(sizeof(bgpstream_filter_mgr_t));
  if(bs_filter_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  bs_filter_mgr->projects = NULL;
  bs_filter_mgr->collectors = NULL;
  bs_filter_mgr->bgp_types = NULL;
  bs_filter_mgr->time_intervals = NULL;
  bgpstream_debug("\tBSF_MGR: create end");
  return bs_filter_mgr;
}



void bgpstream_filter_mgr_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
				     bgpstream_filter_type_t filter_type,
				     const char* filter_value) {
  bgpstream_debug("\tBSF_MGR:: add_filter start");
  if(bs_filter_mgr == NULL) {
    return; // nothing to customize
  }
  // create a new filter structure
  bgpstream_string_filter_t *f = (bgpstream_string_filter_t*) malloc(sizeof(bgpstream_string_filter_t));
  if(f == NULL) {
    bgpstream_debug("\tBSF_MGR:: add_filter malloc failed");
    bgpstream_log_warn("\tBSF_MGR: can't allocate memory");       
    return; 
  }
  // copying filter value
  strcpy(f->value, filter_value);
  // add filter to the appropriate list
  switch(filter_type) {    
  case BGPSTREAM_FILTER_TYPE_PROJECT:
    f->next = bs_filter_mgr->projects;
    bs_filter_mgr->projects = f;
    break;
  case BGPSTREAM_FILTER_TYPE_COLLECTOR:
    f->next = bs_filter_mgr->collectors;
    bs_filter_mgr->collectors = f;
    break;
  case BGPSTREAM_FILTER_TYPE_RECORD_TYPE:
    f->next = bs_filter_mgr->bgp_types;
    bs_filter_mgr->bgp_types = f;
    break;
  default:
    free(f);
    bgpstream_log_warn("\tBSF_MGR: unknown filter - ignoring");   
    return;
  }
  bgpstream_debug("\tBSF_MGR:: add_filter stop");  
}



void bgpstream_filter_mgr_interval_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
					      uint32_t begin_time,
                                              uint32_t end_time){  
  bgpstream_debug("\tBSF_MGR:: add_filter start");
  if(bs_filter_mgr == NULL) {
    return; // nothing to customize
  }
  // create a new filter structure
  bgpstream_interval_filter_t *f = (bgpstream_interval_filter_t*) malloc(sizeof(bgpstream_interval_filter_t));
  if(f == NULL) {
    bgpstream_debug("\tBSF_MGR:: add_filter malloc failed");
    bgpstream_log_warn("\tBSF_MGR: can't allocate memory");       
    return; 
  }
  // copying filter values
  f->begin_time = begin_time;
  f->end_time = end_time;
  f->next = bs_filter_mgr->time_intervals;
  bs_filter_mgr->time_intervals = f;

  bgpstream_debug("\tBSF_MGR:: add_filter stop");  
}



/* destroy the memory allocated for bgpstream filter */
void bgpstream_filter_mgr_destroy(bgpstream_filter_mgr_t *bs_filter_mgr) {
  bgpstream_debug("\tBSF_MGR:: destroy start");
  if(bs_filter_mgr == NULL) {
    return; // nothing to destroy
  }
  // destroying filters
  bgpstream_string_filter_t * sf;
  bgpstream_interval_filter_t * tif;
  // projects
  sf = NULL;
  while(bs_filter_mgr->projects != NULL) {
    sf =  bs_filter_mgr->projects;
    bs_filter_mgr->projects =  bs_filter_mgr->projects->next;
    free(sf);
  }
  // collectors
  sf = NULL;
  while(bs_filter_mgr->collectors != NULL) {
    sf =  bs_filter_mgr->collectors;
    bs_filter_mgr->collectors =  bs_filter_mgr->collectors->next;
    free(sf);
  }
  // bgp_types
  sf = NULL;
  while(bs_filter_mgr->bgp_types != NULL) {
    sf =  bs_filter_mgr->bgp_types;
    bs_filter_mgr->bgp_types =  bs_filter_mgr->bgp_types->next;
    free(sf);
  }
  // time_intervals
  tif = NULL;
  while(bs_filter_mgr->time_intervals != NULL) {
    tif =  bs_filter_mgr->time_intervals;
    bs_filter_mgr->time_intervals =  bs_filter_mgr->time_intervals->next;
    free(tif);
  }
  // free the mgr structure
  free(bs_filter_mgr);
  bs_filter_mgr = NULL;
  bgpstream_debug("\tBSF_MGR:: destroy end");
}


