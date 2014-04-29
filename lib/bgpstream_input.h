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

#ifndef _BGPSTREAM_INPUT_H
#define _BGPSTREAM_INPUT_H

#include <stdbool.h>

#define BGPSTREAM_MAX_FILE_LEN 1024
#define BGPSTREAM_MAX_TYPE_LEN 1024


typedef struct struct_bgpstream_input_t {
  struct struct_bgpstream_input_t *next;
  char filename[BGPSTREAM_MAX_FILE_LEN]; // name of bgpdump 
  char filetype[BGPSTREAM_MAX_TYPE_LEN]; // type of bgpdump (rib or update)
  int epoch_filetime; // timestamp associated to the time the bgp data was generated
} bgpstream_input_t;


typedef enum {EMPTY_INPUT_QUEUE, NON_EMPTY_INPUT_QUEUE} bgpstream_input_mgr_status_t;

typedef struct struct_bgpstream_input_mgr_t {
  bgpstream_input_t *head;
  bgpstream_input_t *tail;
  bgpstream_input_t *last_to_process;
  bgpstream_input_mgr_status_t status;
  //  feeder_callback_t feeder_cb;
  int (*feeder_cb)(struct struct_bgpstream_input_mgr_t * const);
  int epoch_minimum_date;
  int epoch_last_ts_input;
  char feeder_name[BGPSTREAM_MAX_FILE_LEN];
} bgpstream_input_mgr_t;

typedef int (*feeder_callback_t)(bgpstream_input_mgr_t * const);


/* prototypes */
bgpstream_input_mgr_t *bgpstream_input_mgr_create();
bool bgpstream_input_mgr_is_empty(const bgpstream_input_mgr_t * const bs_input_mgr);
int bgpstream_input_push_input( bgpstream_input_mgr_t * const bs_input_mgr, 
				const char * const filename, 
				const char * const filetype, 
				const int epoch_filetime);
int bgpstream_input_push_sorted_input( bgpstream_input_mgr_t * const bs_input_mgr, 
				const char * const filename, 
				const char * const filetype, 
				const int epoch_filetime);
bgpstream_input_t *bgpstream_input_get_queue_to_process(bgpstream_input_mgr_t * const bs_input_mgr);
void bgpstream_input_destroy_queue(bgpstream_input_t *queue);
void bgpstream_input_mgr_destroy(bgpstream_input_mgr_t *bs_input_mgr);


#endif /* _BGPSTREAM_INPUT_H */
