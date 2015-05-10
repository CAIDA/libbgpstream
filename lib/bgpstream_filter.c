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

#include "bgpstream_filter.h"
#include "bgpstream_debug.h"
#include "assert.h"

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
  bs_filter_mgr->last_processed_ts = NULL;
  bs_filter_mgr->rib_frequency = 0;
  bs_filter_mgr->update_frequency = 0;
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


void bgpstream_filter_mgr_frequency_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
                                               bgpstream_record_dump_type_t type,
                                               uint32_t frequency)
{
  bgpstream_debug("\tBSF_MGR:: add_filter start");
  assert(bs_filter_mgr != NULL);
  if(frequency != 0 && bs_filter_mgr->last_processed_ts == NULL)
    {
      if((bs_filter_mgr->last_processed_ts = kh_init(collector_type_ts)) == NULL)
        {
          bgpstream_log_warn("\tBSF_MGR: can't allocate memory for collectortype map"); 
        }
    }
  if(type == BGPSTREAM_RIB)
    {
      bs_filter_mgr->rib_frequency = frequency;
    }
  else
    {
      bs_filter_mgr->update_frequency = frequency;
    }
  bgpstream_debug("\tBSF_MGR:: add_filter end");

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
  khiter_t k;
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
  // rib/update frequency
  if(bs_filter_mgr->last_processed_ts != NULL)
    {
      for(k=kh_begin(bs_filter_mgr->last_processed_ts); k!=kh_end(bs_filter_mgr->last_processed_ts); ++k)
        {
          if(kh_exist(bs_filter_mgr->last_processed_ts,k))
            {
              free(kh_key(bs_filter_mgr->last_processed_ts,k));
            }
        }
      kh_destroy(collector_type_ts, bs_filter_mgr->last_processed_ts);
    }
  // free the mgr structure
  free(bs_filter_mgr);
  bs_filter_mgr = NULL;
  bgpstream_debug("\tBSF_MGR:: destroy end");
}


