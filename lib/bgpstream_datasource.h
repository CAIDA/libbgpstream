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

#ifndef _BGPSTREAM_DATASOURCE_H
#define _BGPSTREAM_DATASOURCE_H

#include "bgpstream_options.h"
#include "bgpstream_constants.h"
#include "bgpstream_input.h"
#include "bgpstream_filter.h"

#include <stdlib.h>
#include <stdio.h>
#include <mysql/mysql.h>


typedef enum {DS_ON,    /* current data source is on */
	      DS_OFF,   /* current data source is off */
	      DS_ERROR  /* current data source generated an error */
} bgpstream_datasource_status_t;


typedef struct struct_bgpstream_customlist_datasource_t {
  int list_read; // 1 if list has bee read, 0 otherwise
  bgpstream_filter_mgr_t * filter_mgr;
  char filename[BGPSTREAM_DUMP_MAX_LEN];
  char project[BGPSTREAM_PAR_MAX_LEN];
  char collector[BGPSTREAM_PAR_MAX_LEN];
  char bgp_type[BGPSTREAM_PAR_MAX_LEN];
  int filetime;
} bgpstream_customlist_datasource_t;


typedef struct struct_bgpstream_csvfile_datasource_t {
  int csvfile_read; // 1 if list has bee read, 0 otherwise
  bgpstream_filter_mgr_t * filter_mgr;
  char filename[BGPSTREAM_DUMP_MAX_LEN];
  char project[BGPSTREAM_PAR_MAX_LEN];
  char collector[BGPSTREAM_PAR_MAX_LEN];
  char bgp_type[BGPSTREAM_PAR_MAX_LEN];
  int filetime;
} bgpstream_csvfile_datasource_t;


typedef struct struct_bgpstream_mysql_datasource_t {
  MYSQL * mysql_con;
  char sql_query[2048];
  MYSQL_STMT *stmt;          // mysql statement
  MYSQL_BIND parameters[2];  // parameters for placeholders
  // variables to bind to placeholders
  long int last_timestamp;       // parameter to bind I
  long int current_timestamp;     // parameter to bind II
  MYSQL_BIND results[8];    // query results
  // variables to bind to results
  char proj_path_res[BGPSTREAM_PAR_MAX_LEN];
  char coll_path_res[BGPSTREAM_PAR_MAX_LEN];
  char type_path_res[BGPSTREAM_PAR_MAX_LEN];
  char proj_name_res[BGPSTREAM_PAR_MAX_LEN];
  char coll_name_res[BGPSTREAM_PAR_MAX_LEN];
  char type_name_res[BGPSTREAM_PAR_MAX_LEN];
  char file_ext_res[BGPSTREAM_PAR_MAX_LEN];
  int filetime_res;
  // others
} bgpstream_mysql_datasource_t;



typedef struct struct_bgpstream_datasource_mgr_t {
  // char datasource_name[BGPSTREAM_DS_MAX_LEN];
  bgpstream_datasource_type datasource;
  // mysql datasource
  bgpstream_mysql_datasource_t *mysql_ds;
  bgpstream_customlist_datasource_t *customlist_ds;
  bgpstream_csvfile_datasource_t *csvfile_ds;
  // other datasources
  // blocking options
  int blocking;
  int backoff_time;
  bgpstream_datasource_status_t status;
} bgpstream_datasource_mgr_t;


/* allocates memory for datasource_mgr */
bgpstream_datasource_mgr_t *bgpstream_datasource_mgr_create();

void bgpstream_datasource_mgr_set_data_interface(bgpstream_datasource_mgr_t *datasource_mgr,
						 const bgpstream_datasource_type datasource);

/* init the datasource_mgr and start/init the selected datasource */
void bgpstream_datasource_mgr_init(bgpstream_datasource_mgr_t *datasource_mgr,
				   bgpstream_filter_mgr_t *filter_mgr);

void bgpstream_datasource_mgr_set_blocking(bgpstream_datasource_mgr_t *datasource_mgr);

int bgpstream_datasource_mgr_update_input_queue(bgpstream_datasource_mgr_t *datasource_mgr,
						bgpstream_input_mgr_t *input_mgr);

/* stop the active data source */
void bgpstream_datasource_mgr_close(bgpstream_datasource_mgr_t *datasource_mgr);

/* destroy the memory allocated for the datasource_mgr */
void bgpstream_datasource_mgr_destroy(bgpstream_datasource_mgr_t *datasource_mgr);




#endif /* _BGPSTREAM_DATASOURCE */
