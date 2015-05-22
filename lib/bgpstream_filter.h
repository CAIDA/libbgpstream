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

#ifndef _BGPSTREAM_FILTER_H
#define _BGPSTREAM_FILTER_H

#include "bgpstream.h"
#include "bgpstream_constants.h"
#include "khash.h"

typedef struct struct_bgpstream_string_filter_t {
  char value[BGPSTREAM_PAR_MAX_LEN];
  struct struct_bgpstream_string_filter_t * next;
} bgpstream_string_filter_t;

typedef struct struct_bgpstream_interval_filter_t {
  uint32_t begin_time;
  uint32_t end_time;
  struct struct_bgpstream_interval_filter_t * next;
} bgpstream_interval_filter_t;

KHASH_INIT(collector_ts, char*, uint32_t, 1,
	   kh_str_hash_func, kh_str_hash_equal);

typedef khash_t(collector_ts) collector_ts_t;
                                   
typedef struct struct_bgpstream_filter_mgr_t {
  
  bgpstream_string_filter_t * projects;
  bgpstream_string_filter_t * collectors;
  bgpstream_string_filter_t * bgp_types;
  bgpstream_interval_filter_t * time_intervals;
  collector_ts_t *last_processed_ts;
  uint32_t rib_period;
} bgpstream_filter_mgr_t;


/* allocate memory for a new bgpstream filter */
bgpstream_filter_mgr_t *bgpstream_filter_mgr_create();

/* configure filters in order to select a subset of the bgp data available */
void bgpstream_filter_mgr_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
				     bgpstream_filter_type_t filter_type,
				     const char* filter_value);

void bgpstream_filter_mgr_rib_period_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
                                                uint32_t period);

void bgpstream_filter_mgr_interval_filter_add(bgpstream_filter_mgr_t *bs_filter_mgr,
					      uint32_t begin_time,
					      uint32_t end_time);


/* destroy the memory allocated for bgpstream filter */
void bgpstream_filter_mgr_destroy(bgpstream_filter_mgr_t *bs_filter_mgr);


#endif /* _BGPSTREAM_FILTER_H */
