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
#include "debug.h"


/* allocate memory for a new bgpstream filter */
bgpstream_filter_mgr_t *bgpstream_filter_mgr_create() {
  debug("\tBSF_MGR: create start");
  bgpstream_filter_mgr_t *bs_filter_mgr = (bgpstream_filter_mgr_t*) malloc(sizeof(bgpstream_filter_mgr_t));
  if(bs_filter_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  bs_filter_mgr->projects = NULL;
  bs_filter_mgr->collectors = NULL;
  bs_filter_mgr->bgp_types = NULL;
  bs_filter_mgr->time_intervals = NULL;
  debug("\tBSF_MGR: create end");
  return bs_filter_mgr;
}



void bgpstream_filter_mgr_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
				     bgpstream_filter_type filter_type,
				     const char* filter_value) {
  debug("\tBSF_MGR:: add_filter start");
  if(bs_filter_mgr == NULL) {
    return; // nothing to customize
  }
  // create a new filter structure
  bgpstream_string_filter_t *f = (bgpstream_string_filter_t*) malloc(sizeof(bgpstream_string_filter_t));
  if(f == NULL) {
    debug("\tBSF_MGR:: add_filter malloc failed");
    log_warn("\tBSF_MGR: can't allocate memory");       
    return; 
  }
  // copying filter value
  strcpy(f->value, filter_value);
  // add filter to the appropriate list
  switch(filter_type) {    
  case BS_PROJECT:
    f->next = bs_filter_mgr->projects;
    bs_filter_mgr->projects = f;
    break;
  case BS_COLLECTOR:
    f->next = bs_filter_mgr->collectors;
    bs_filter_mgr->collectors = f;
    break;
  case BS_BGP_TYPE:    
    f->next = bs_filter_mgr->bgp_types;
    bs_filter_mgr->bgp_types = f;
    break;
  case BS_TIME_INTERVAL:    
    free(f);
    log_warn("\tBSF_MGR: wrong interval filter request - ignoring");   
    break;
  default:
    free(f);
    log_warn("\tBSF_MGR: unknown filter - ignoring");   
    return;
  }
  debug("\tBSF_MGR:: add_filter stop");  
}



void bgpstream_filter_mgr_interval_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
					      bgpstream_filter_type filter_type,
					      const char* filter_start,
					      const char* filter_stop){  
  debug("\tBSF_MGR:: add_filter start");
  if(bs_filter_mgr == NULL) {
    return; // nothing to customize
  }
  // create a new filter structure
  bgpstream_interval_filter_t *f = (bgpstream_interval_filter_t*) malloc(sizeof(bgpstream_interval_filter_t));
  if(f == NULL) {
    debug("\tBSF_MGR:: add_filter malloc failed");
    log_warn("\tBSF_MGR: can't allocate memory");       
    return; 
  }
  // copying filter values
  strcpy(f->start, filter_start);
  strcpy(f->stop, filter_stop);
  f->time_interval_start = atoi(filter_start);
  f->time_interval_stop = atoi(filter_stop);
  switch(filter_type) {   
  case BS_TIME_INTERVAL:    
    f->next = bs_filter_mgr->time_intervals;
    bs_filter_mgr->time_intervals = f;
    break;
  case BS_PROJECT:
  case BS_COLLECTOR:
  case BS_BGP_TYPE:    
    free(f);
    log_warn("\tBSF_MGR: wrong filter request - ignoring");   
    break;
  default:
    free(f);
    log_warn("\tBSF_MGR: unknown filter - ignoring");   
    return;
  }
  debug("\tBSF_MGR:: add_filter stop");  
}



/* destroy the memory allocated for bgpstream filter */
void bgpstream_filter_mgr_destroy(bgpstream_filter_mgr_t *bs_filter_mgr) {
  debug("\tBSF_MGR:: destroy start");
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
  while(bs_filter_mgr->bgp_types != NULL) {
    tif =  bs_filter_mgr->time_intervals;
    bs_filter_mgr->time_intervals =  bs_filter_mgr->time_intervals->next;
    free(tif);
  }
  // free the mgr structure
  free(bs_filter_mgr);
  bs_filter_mgr = NULL;
  debug("\tBSF_MGR:: destroy end");
}


