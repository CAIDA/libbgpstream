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

#ifndef _BGPSTREAM_READER_H
#define _BGPSTREAM_READER_H


#include "bgpstream_constants.h"
#include "bgpstream_record.h"
#include "bgpstream_input.h"
#include "bgpstream_filter.h" 

#include <bgpdump_lib.h>
#include <stdbool.h>


typedef enum {
  BGPSTREAM_READER_STATUS_VALID_ENTRY, 
  BGPSTREAM_READER_STATUS_FILTERED_DUMP, 
  BGPSTREAM_READER_STATUS_EMPTY_DUMP, 
  BGPSTREAM_READER_STATUS_CANT_OPEN_DUMP, 
  BGPSTREAM_READER_STATUS_CORRUPTED_DUMP, 
  BGPSTREAM_READER_STATUS_END_OF_DUMP
} bgpstream_reader_status_t;

typedef struct struct_bgpstream_reader_t {
  struct struct_bgpstream_reader_t *next;
  char dump_name[BGPSTREAM_DUMP_MAX_LEN];  // name of bgp dump 
  char dump_project[BGPSTREAM_PAR_MAX_LEN];  // name of bgp project 
  char dump_collector[BGPSTREAM_PAR_MAX_LEN];  // name of bgp collector 
  char dump_type[BGPSTREAM_PAR_MAX_LEN];  // type of bgp dump (rib or update)
  long dump_time;         // timestamp associated with the time the bgp data was aggregated
  long record_time;       // timestamp associated with the current bd_entry
  BGPDUMP *bd_mgr;
  BGPDUMP_ENTRY *bd_entry;
  int successful_read; // n. successful reads, i.e. entry != NULL
  int valid_read; // n. reads successful and compatible with filters
  bgpstream_reader_status_t status;
} bgpstream_reader_t;

typedef enum {
  BGPSTREAM_READER_MGR_STATUS_EMPTY_READER_MGR,
  BGPSTREAM_READER_MGR_STATUS_NON_EMPTY_READER_MGR
} bgpstream_reader_mgr_status_t;

typedef struct struct_bgpstream_reader_mgr_t {
  bgpstream_reader_t *reader_queue;
  const bgpstream_filter_mgr_t *filter_mgr;
  bgpstream_reader_mgr_status_t status;
} bgpstream_reader_mgr_t;



/* create a new reader mgr */
bgpstream_reader_mgr_t * bgpstream_reader_mgr_create(const bgpstream_filter_mgr_t * const filter_mgr);
/* check if the readers' queue is empty  */
bool bgpstream_reader_mgr_is_empty(const bgpstream_reader_mgr_t * const bs_reader_mgr);

/* use a list of inputs to populate the readers' queue */
void bgpstream_reader_mgr_add(bgpstream_reader_mgr_t * const bs_reader_mgr, 
			      const bgpstream_input_t * const toprocess_queue,
			      const bgpstream_filter_mgr_t * const filter_mgr);

/* get the next available record (and update the reader mgr status) */
int bgpstream_reader_mgr_get_next_record(bgpstream_reader_mgr_t * const bs_reader_mgr, 
					 bgpstream_record_t *const bs_record,
					 const bgpstream_filter_mgr_t * const filter_mgr);

/* destroy the reader manager */
void bgpstream_reader_mgr_destroy(bgpstream_reader_mgr_t * const bs_reader_mgr);

#endif /* _BGPSTREAM_READER_H */
