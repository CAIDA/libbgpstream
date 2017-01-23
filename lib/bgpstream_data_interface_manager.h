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

#ifndef __BGPSTREAM_DATA_INTERFACE_MANAGER_H
#define __BGPSTREAM_DATA_INTERFACE_MANAGER_H

#include "config.h"

#ifdef WITH_DATA_INTERFACE_SINGLEFILE
#include "bgpstream_data_interface_singlefile.h"
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
#include "bgpstream_data_interface_csvfile.h"
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
#include "bgpstream_data_interface_sqlite.h"
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
#include "bgpstream_data_interface_broker.h"
#endif

typedef enum {
  BGPSTREAM_DATA_INTERFACE_STATUS_ON,   /* current data source is on */
  BGPSTREAM_DATA_INTERFACE_STATUS_OFF,  /* current data source is off */
  BGPSTREAM_DATA_INTERFACE_STATUS_ERROR /* current data source generated an
                                           error */
} bgpstream_data_interface_status_t;

typedef struct bgpstream_data_interface_mgr {
  bgpstream_data_interface_id_t di_id;
// data_interfaces available

#ifdef WITH_DATA_INTERFACE_SINGLEFILE
  bgpstream_di_singlefile_t *singlefile;
  char *singlefile_rib_mrtfile;
  char *singlefile_upd_mrtfile;
#endif

#ifdef WITH_DATA_INTERFACE_CSVFILE
  bgpstream_di_csvfile_t *csvfile;
  char *csvfile_file;
#endif

#ifdef WITH_DATA_INTERFACE_SQLITE
  bgpstream_di_sqlite_t *sqlite;
  char *sqlite_file;
#endif

#ifdef WITH_DATA_INTERFACE_BROKER
  bgpstream_di_broker_t *broker;
  char *broker_url;
  char **broker_params;
  int broker_params_cnt;
#endif

  // blocking options
  int blocking;
  int backoff_time;
  int retry_cnt;
  bgpstream_data_interface_status_t status;
} bgpstream_data_interface_mgr_t;

/* allocates memory for data_interface_mgr */
bgpstream_data_interface_mgr_t *bgpstream_data_interface_mgr_create();

void bgpstream_data_interface_mgr_set_data_interface(
  bgpstream_data_interface_mgr_t *data_interface_mgr,
  const bgpstream_data_interface_id_t data_interface);

int bgpstream_data_interface_mgr_set_data_interface_option(
  bgpstream_data_interface_mgr_t *data_interface_mgr,
  const bgpstream_data_interface_option_t *option_type,
  const char *option_value);

/* init the data_interface_mgr and start/init the selected data_interface */
void bgpstream_data_interface_mgr_init(
  bgpstream_data_interface_mgr_t *data_interface_mgr,
  bgpstream_filter_mgr_t *filter_mgr);

void bgpstream_data_interface_mgr_set_blocking(
  bgpstream_data_interface_mgr_t *data_interface_mgr);

int bgpstream_data_interface_mgr_update_input_queue(
  bgpstream_data_interface_mgr_t *data_interface_mgr,
  bgpstream_input_mgr_t *input_mgr);

/* stop the active data source */
void bgpstream_data_interface_mgr_close(
  bgpstream_data_interface_mgr_t *data_interface_mgr);

/* destroy the memory allocated for the data_interface_mgr */
void bgpstream_data_interface_mgr_destroy(
  bgpstream_data_interface_mgr_t *data_interface_mgr);

#endif /* _BGPSTREAM_DATA_INTERFACE_MANAGER */
