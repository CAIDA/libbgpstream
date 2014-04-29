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

#ifndef _BGPSTREAM_LIB_H
#define _BGPSTREAM_LIB_H

#include "bgpstream_input.h"
#include "bgpstream_reader.h"
#include "bgpstream_feeder_plugins.h"


typedef struct struct_bgpstream_t {  
  bgpstream_input_mgr_t *input_mgr;
  bgpstream_reader_mgr_t *reader_mgr;
  // int status;  
} bgpstream_t;


/* prototypes */
int test_mylib(const char *filename);
bgpstream_t *bgpstream_create();
void bgpstream_set_feeder_plugin(bgpstream_t *bs, feeder_callback_t feeder_cb,
				 const char * const feeder_name,
				 const int min_date, const int min_ts);
void bgpstream_destroy(bgpstream_t *bs);
bgpstream_record_t *bgpstream_get_next(bgpstream_t *bs);
void bgpstream_free_mem(bgpstream_record_t *bs_record);


#endif /* _BGPSTREAM_LIB_H */
