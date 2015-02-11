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

#include "bgpdump_lib.h"
#include "bgpdump_process.h"

#include "bgpstream_record.h"
#include "bgpstream_debug.h"

/* allocate memory for a bs_record */
bgpstream_record_t *bgpstream_record_create() {
  bgpstream_debug("BS: create record start");
  bgpstream_record_t *bs_record = (bgpstream_record_t*) malloc(sizeof(bgpstream_record_t));
  if(bs_record == NULL) {
    return NULL; // can't allocate memory
  }
  bs_record->bd_entry = NULL;
  bs_record->status = EMPTY_SOURCE;
  bs_record->dump_pos = BGPSTREAM_DUMP_START;
  strcpy(bs_record->attributes.dump_project, "");
  strcpy(bs_record->attributes.dump_collector, "");
  bs_record->attributes.dump_type = BGPSTREAM_UPDATE;
  bs_record->attributes.dump_time = 0;
  bs_record->attributes.record_time = 0;
  bgpstream_debug("BS: create record end");
  return bs_record;
}


/* free memory associated to a bs_record  */
void bgpstream_record_destroy(bgpstream_record_t * const bs_record){
  bgpstream_debug("BS: destroy record start");
  if(bs_record == NULL) {
    bgpstream_debug("BS: record destroy end");
    return; // nothing to do
  }
  if(bs_record->bd_entry != NULL){
    bgpstream_debug("BS - free bs_record->bgpdump_entry");
    bgpdump_free_mem(bs_record->bd_entry);
    bs_record->bd_entry = NULL;
  }
  bgpstream_debug("BS - free bs_record");
  free(bs_record);
  bgpstream_debug("BS: destroy record end");
}

void bgpstream_record_print_mrt_data(bgpstream_record_t * const bs_record) {
  bgpdump_print_entry(bs_record->bd_entry);
}
