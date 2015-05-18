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

#ifndef _BGPSTREAM_INPUT_H
#define _BGPSTREAM_INPUT_H

#include "bgpstream_constants.h"
#include <stdbool.h>



typedef struct struct_bgpstream_input_t {
  struct struct_bgpstream_input_t *next;
  char *filename; // name of bgpdump 
  char *fileproject; // bgpdump project
  char *filecollector; // bgpdump collector
  char *filetype; // type of bgpdump (rib or update)
  int epoch_filetime; // timestamp associated to the time the bgp data was generated
  int time_span;
} bgpstream_input_t;


typedef enum {
  BGPSTREAM_INPUT_MGR_STATUS_EMPTY_INPUT_QUEUE,
  BGPSTREAM_INPUT_MGR_STATUS_NON_EMPTY_INPUT_QUEUE
} bgpstream_input_mgr_status_t;

typedef struct struct_bgpstream_input_mgr_t {
  bgpstream_input_t *head;
  bgpstream_input_t *tail;
  bgpstream_input_t *last_to_process;
  bgpstream_input_mgr_status_t status;
  int epoch_minimum_date;
  int epoch_last_ts_input;
} bgpstream_input_mgr_t;


/* prototypes */
bgpstream_input_mgr_t *bgpstream_input_mgr_create();
bool bgpstream_input_mgr_is_empty(const bgpstream_input_mgr_t * const bs_input_mgr);
int bgpstream_input_mgr_push_sorted_input(bgpstream_input_mgr_t * const bs_input_mgr, 
					  char * filename, char * fileproject,
					  char * filecollector, char * const filetype,
					  const int epoch_filetime, const int time_span);
bgpstream_input_t *bgpstream_input_mgr_get_queue_to_process(bgpstream_input_mgr_t * const bs_input_mgr);
void bgpstream_input_mgr_destroy_queue(bgpstream_input_t *queue);
void bgpstream_input_mgr_destroy(bgpstream_input_mgr_t *bs_input_mgr);


#endif /* _BGPSTREAM_INPUT_H */
