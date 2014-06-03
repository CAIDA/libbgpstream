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

#include "bgpstream_filter.h"
#include "debug.h"


/* allocate memory for a new bgpstream filter */
bgpstream_filter_mgr_t *bgpstream_filter_mgr_create() {
  debug("\tBSF_MGR: create start");
  bgpstream_filter_mgr_t *bs_filter_mgr = (bgpstream_filter_mgr_t*) malloc(sizeof(bgpstream_filter_mgr_t));
  if(bs_filter_mgr == NULL) {
    return NULL; // can't allocate memory
  }
  // default filters (i.e. no filtering)
  //  strcpy(bs_f->datasource, "mysql");
  memset(bs_filter_mgr->project, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(bs_filter_mgr->collector, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(bs_filter_mgr->bgp_type, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(bs_filter_mgr->time_interval_start_str, 0, BGPSTREAM_PAR_MAX_LEN);
  memset(bs_filter_mgr->time_interval_stop_str, 0, BGPSTREAM_PAR_MAX_LEN);
  // memset done
  strcpy(bs_filter_mgr->project, "");
  strcpy(bs_filter_mgr->collector, "");
  strcpy(bs_filter_mgr->bgp_type, "");
  /* min and max unix times that can be
   * represented by an integer */
  bs_filter_mgr->time_interval_start = 0; // Thu Jan  1 00:00:00 UTC 1970
  strcpy(bs_filter_mgr->time_interval_start_str, "0");
  bs_filter_mgr->time_interval_stop = 2147483647; // Jan 19 03:14:07 UTC 2038
  strcpy(bs_filter_mgr->time_interval_stop_str, "2147483647");
  debug("\tBSF_MGR: create end");
  return bs_filter_mgr;
}


/* configure filters in order to select a subset of the bgp data available */
void bgpstream_filter_mgr_filter_set(bgpstream_filter_mgr_t *bs_filter_mgr, const char* filter_name, 
			  const char* filter_value) {
  debug("\tBSF_MGR:: set_filter start");
  if(bs_filter_mgr == NULL) {
    return; // nothing to customize
  }
  // datasource
  // if (strcmp(filter_name, "datasource") == 0) {
  //  strcpy(bs->filters.datasource, filter_value);
  // }
  // project
  if (strcmp(filter_name, "project") == 0) {
    strcpy(bs_filter_mgr->project, filter_value);
  }
  // collector
  if (strcmp(filter_name, "collector") == 0) {
    strcpy(bs_filter_mgr->collector, filter_value);
  }
  // bgp_type
  if (strcmp(filter_name, "bgp_type") == 0) {
    strcpy(bs_filter_mgr->bgp_type, filter_value);
  }
  // time_interval_start
  if (strcmp(filter_name, "time_interval_start") == 0) {
    strcpy(bs_filter_mgr->time_interval_start_str, filter_value);
    bs_filter_mgr->time_interval_start = atoi(filter_value);
  }
  // time_interval_stop
  if (strcmp(filter_name, "time_interval_stop") == 0) {
    strcpy(bs_filter_mgr->time_interval_stop_str, filter_value);
    bs_filter_mgr->time_interval_stop = atoi(filter_value);
  }
  debug("\tBSF_MGR:: set_filter stop");
}


/* destroy the memory allocated for bgpstream filter */
void bgpstream_filter_mgr_destroy(bgpstream_filter_mgr_t *bs_filter_mgr) {
  debug("\tBSF_MGR:: destroy start");
  if(bs_filter_mgr == NULL) {
    return; // nothing to destroy
  }
  free(bs_filter_mgr);
  bs_filter_mgr = NULL;
  debug("\tBSF_MGR:: destroy end");
}


