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

#ifndef __BGPSTREAM_H
#define __BGPSTREAM_H

#include <bgpstream_utils.h>
#include <bgpstream_elem.h>
#include <bgpstream_record.h>

/** @file
 *
 * @brief Header file that exposes the public interface of bgpstream. See
 * bgpstream_record.h and bgpstream_elem.h for the public interfaces of
 * bgpstream_record and bgpstream_elem respectively.
 *
 * @author Chiara Orsini
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque handle that represents a BGP Stream instance */
typedef struct struct_bgpstream_t bgpstream_t;

/** @} */

/**
 * @name Public Data Structures
 *
 * @{ */

/** @} */

/**
 * @name Public Enums
 *
 * @{ */

/** The type of the filter to be applied */
typedef enum {

  /** Filter records based on associated project (e.g. 'ris') */
  BS_PROJECT       = 1,

  /** Filter records based on collector (e.g. 'rrc01') */
  BS_COLLECTOR     = 2,

  /** Filter records based on record type (e.g. 'updates') */
  BS_BGP_TYPE      = 3,

  /** Filter records based on time */
  BS_TIME_INTERVAL = 4,

} bgpstream_filter_type;


/** Data Interface IDs */
typedef enum {

  /** MySQL data interface */
  BS_MYSQL      = 1,

  /** Customlist interface */
  BS_CUSTOMLIST = 2,

  /** CSV file interface */
  BS_CSVFILE    = 3,

} bgpstream_datasource_type;

/** @todo REPLACE these with per-interface options */
typedef enum {
  BS_MYSQL_DB,
  BS_MYSQL_USER,
  BS_MYSQL_HOST,
  BS_CSVFILE_FILE
} bgpstream_datasource_option;

/** @} */

/**
 * @name Public API Functions
 *
 * @{ */

/** Create a new BGP Stream instance
 *
 * @return a pointer to a BGP Stream instance if successful, NULL otherwise
 */
bgpstream_t *bgpstream_create();

/** Add a filter in order to select a subset of the bgp data available
 *
 * @param bs            pointer to a BGP Stream instance to filter
 * @param filter_type   the type of the filter to apply
 * @param filter_value  the value to set the filter to
 */
void bgpstream_add_filter(bgpstream_t *bs,
                          bgpstream_filter_type filter_type,
			  const char* filter_value);

/** @todo fix this function */
void bgpstream_add_interval_filter(bgpstream_t *bs,
                                   bgpstream_filter_type filter_type,
				   const char *filter_start,
                                   const char *filter_stop);

/** Set the data interface that BGP Stream uses to find BGP data
 *
 * @param bs            pointer to a BGP Stream instance to configure
 * @param if_id         ID of the data interface to use
 */
void bgpstream_set_data_interface(bgpstream_t *bs,
                                  bgpstream_datasource_type datasource);

/** Set a data interface option
 *
 * @param bs            pointer to a BGP Stream instance to configure
 * @param if_id         ID if the data interface to set an option for
 * @param option_type   name of the option to set
 * @param option_value  value to set the option to
 */
void bgpstream_set_data_interface_options(bgpstream_t *bs,
                                        bgpstream_datasource_option option_type,
				        char *option_value);

/** Configure the interface to blocks waiting for new data instead of returning
 * end-of-stream if no more data is available.
 *
 * @param bs            pointer to a BGP Stream instance to put into blocking
 *                      mode
 */
void bgpstream_set_blocking(bgpstream_t *bs);

/** Start the given BGP Stream instance.
 *
 * @param bs            pointer to a BGP Stream instance to start
 * @return 0 if the stream was started successfully, -1 otherwise
 *
 * This function must be called after all configuration functions, and before
 * the first call to bgpstream_get_next_record.
 */
int bgpstream_start(bgpstream_t *bs);

/** Retrieve from the stream, the next record that matches configured filters.
 *
 * @param bs            pointer to a BGP Stream instance to get record from
 * @param record        pointer to a bgpstream record instance created using
 *                      bgpstream_record_create
 * @return >0 if a record was read successfully, 0 if end-of-stream has been
 * reached, <0 if an error occurred.
 *
 * The record passed to this function may be reused for subsequent calls if
 * state for previous records is not needed (i.e. the records are processed
 * independently of each other). If records are not processed independently,
 * then a new record must be created for each call to this function.
 */
int bgpstream_get_next_record(bgpstream_t *bs,
                              bgpstream_record_t *record);

/** Stop the given BGP Stream instance
 *
 * @param bs            pointer to a BGP Stream instance to stop
 */
void bgpstream_stop(bgpstream_t *bs);

/** Destroy the given BGP Stream instance
 *
 * @param bs            pointer to a BGP Stream instance to destroy
 */
void bgpstream_destroy(bgpstream_t *bs);

/** @} */

#endif /* __BGPSTREAM_H */
