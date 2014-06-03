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

#ifndef _BGPSTREAM_FILTER_H
#define _BGPSTREAM_FILTER_H


#include "bgpstream_constants.h"
#include <stdlib.h>
#include<stdio.h>


typedef struct struct_bgpstream_filter_mgr_t {
  char project[BGPSTREAM_PAR_MAX_LEN];
  char collector[BGPSTREAM_PAR_MAX_LEN];
  char bgp_type[BGPSTREAM_PAR_MAX_LEN];
  int time_interval_start;
  char time_interval_start_str[BGPSTREAM_PAR_MAX_LEN];
  int time_interval_stop;
  char time_interval_stop_str[BGPSTREAM_PAR_MAX_LEN];

} bgpstream_filter_mgr_t;


/* allocate memory for a new bgpstream filter */
bgpstream_filter_mgr_t *bgpstream_filter_mgr_create();

/* configure filters in order to select a subset of the bgp data available */
void bgpstream_filter_mgr_filter_set(bgpstream_filter_mgr_t *bs_filter_mgr,
				     const char* filter_name,
				     const char* filter_value);

/* destroy the memory allocated for bgpstream filter */
void bgpstream_filter_mgr_destroy(bgpstream_filter_mgr_t *bs_filter_mgr);


#endif /* _BGPSTREAM_FILTER_H */
