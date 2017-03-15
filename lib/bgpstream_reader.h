/*
 * This file is part of bgpstream
 *
 * CAIDA, UC San Diego
 * bgpstream-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 * Authors: Alistair King, Chiara Orsini
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BGPSTREAM_READER_H
#define _BGPSTREAM_READER_H

#include "bgpstream_filter.h"
#include "bgpstream_record.h"
#include "bgpstream_resource.h"

typedef enum {
  BGPSTREAM_READER_STATUS_VALID_ENTRY,
  BGPSTREAM_READER_STATUS_FILTERED_DUMP,
  BGPSTREAM_READER_STATUS_EMPTY_DUMP,
  BGPSTREAM_READER_STATUS_CANT_OPEN_DUMP,
  BGPSTREAM_READER_STATUS_CORRUPTED_DUMP,
  BGPSTREAM_READER_STATUS_END_OF_DUMP
} bgpstream_reader_status_t;

// TODO: move back to reader.c
#include "bgpdump/bgpdump_lib.h"
struct bgpstream_reader {
  struct bgpstream_reader *next;
  char dump_name[BGPSTREAM_DUMP_MAX_LEN];     // name of bgp dump
  char dump_project[BGPSTREAM_PAR_MAX_LEN];   // name of bgp project
  char dump_collector[BGPSTREAM_PAR_MAX_LEN]; // name of bgp collector
  bgpstream_record_dump_type_t dump_type;
  long dump_time;   // timestamp associated with the time the bgp data was
                    // aggregated
  long record_time; // timestamp associated with the current bd_entry
  BGPDUMP_ENTRY *bd_entry;
  int successful_read; // n. successful reads, i.e. entry != NULL
  int valid_read;      // n. reads successful and compatible with filters
  bgpstream_reader_status_t status;

  BGPDUMP *bd_mgr;
  /** The thread that opens the bgpdump */
  pthread_t producer;
  /* has the thread opened the dump? */
  int dump_ready;
  pthread_cond_t dump_ready_cond;
  pthread_mutex_t mutex;
  /* have we already checked that the dump is ready? */
  int skip_dump_check;
};

/** Opaque structure representting a reader instance */
typedef struct bgpstream_reader bgpstream_reader_t;

bgpstream_reader_t *
bgpstream_reader_create(bgpstream_resource_t *resource,
                        bgpstream_filter_mgr_t *filter_mgr);

void bgpstream_reader_destroy(bgpstream_reader_t *bs_reader);

void
bgpstream_reader_read_new_data(bgpstream_reader_t *bs_reader,
                               bgpstream_filter_mgr_t *filter_mgr);

void
bgpstream_reader_export_record(bgpstream_reader_t *bs_reader,
                               bgpstream_record_t *bs_record,
                               bgpstream_filter_mgr_t *filter_mgr);

#endif /* _BGPSTREAM_READER_H */
