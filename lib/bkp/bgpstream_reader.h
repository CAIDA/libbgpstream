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

#ifndef _BGPSTREAM_READER_H
#define _BGPSTREAM_READER_H

#include "bgpstream_input.h"
#include "bgpstream_filter.h"

#include <bgpdump_lib.h>
#include <stdbool.h>

typedef enum {CANT_OPEN_DUMP, /* can't open the dump */
	      NO_VALID_ENTRY_IN_DUMP,  /* read all dump, none of the 
					  entries were filter-compliant */
	      CORRUPTED_DUMP, /* dump corrupted */
	      EMPTY_DUMP,     /* empty dump */
	      VALID_ENTRY     /* valid entry found in dump */
} bgpstream_record_attribute_status_t;


typedef struct struct_bgpstream_record_attributes_t {
  // define a list of useful attributes to associate to bgp dump entry
  // use these fields to tell the client
  // if there were errors during the record creation
  bgpstream_record_attribute_status_t status; 
  struct tm *entry_date;
  
} bgpstream_record_attributes_t;


typedef struct struct_bgpstream_record_t {
  bgpstream_record_attributes_t attributes;
  BGPDUMP_ENTRY *bd_entry;
} bgpstream_record_t;


//typedef enum {NEW_BS_RECORDS_AVAILABLE, LAST_BS_RECORD} bgpstream_reader_status_t;

typedef enum {BS_READER_OFF, BS_READER_ON, BS_READER_LAST} bgpstream_reader_status_t;

typedef struct struct_bgpstream_reader_t {
  struct struct_bgpstream_reader_t *next;
  char filename[BGPSTREAM_MAX_FILE_LEN]; // name of bgpdump *****
  BGPDUMP *bd_mgr;
  int num_valid_records;
  bgpstream_record_t *bs_record;
  bgpstream_reader_status_t status;
} bgpstream_reader_t;

// --------------------------------------------------- /



typedef enum {EMPTY_READER,NON_EMPTY_READER} bgpstream_reader_mgr_status_t;

typedef struct struct_bgpstream_reader_mgr_t {
  bgpstream_reader_t *reader_priority_queue;
  bgpstream_filter_mgr_t *filter_mgr;
  bgpstream_reader_mgr_status_t status;
} bgpstream_reader_mgr_t;


/* prototypes */
bgpstream_reader_mgr_t *bgpstream_reader_mgr_create(bgpstream_filter_mgr_t *filter_mgr);
bool bgpstream_reader_mgr_is_empty(const bgpstream_reader_mgr_t * const bs_reader_mgr);
int bgpstream_reader_mgr_set( bgpstream_reader_mgr_t * const bs_reader_mgr, 
			      const bgpstream_input_t * const toprocess_queue);
bgpstream_record_t *bgpstream_reader_mgr_get_next_record(bgpstream_reader_mgr_t * const bs_reader_mgr);
void bgpstream_reader_destroy_record(bgpstream_record_t *bs_record);
void bgpstream_reader_mgr_destroy(bgpstream_reader_mgr_t *bs_reader_mgr);


#endif /* _BGPSTREAM_READER_H */
