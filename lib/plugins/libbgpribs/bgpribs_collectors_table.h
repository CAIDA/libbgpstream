/*
 * bgpcorsaro
 *
 * Chiara Orsini, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2014 The Regents of the University of California.
 *
 * This file is part of bgpcorsaro.
 *
 * bgpcorsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpcorsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpcorsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __BGPRIBS_COLLECTORS_TABLE_H
#define __BGPRIBS_COLLECTORS_TABLE_H

#include <assert.h>
#include "khash.h"
#include "utils.h"
#include "config.h"
#include "bgpribs_collectordata.h"
#ifdef WITH_BGPWATCHER
#include "bgpribs_bgpwatcher_client.h"
#endif


/** @file
 *
 * @brief Header file that exposes the structures needed
 * to manage a set of collectors.
 *
 * @author Chiara Orsini
 *
 */


KHASH_INIT(collectors_table_t,       /* name */
	   char *,                   /* khkey_t */
	   collectordata_t *,        /* khval_t */
	   1,                        /* kh_is_map */
	   kh_str_hash_func,         /* __hash_func */
	   kh_str_hash_equal         /* __hash_equal */
	   )

/** collectors table
 *  this structure contains a map that associate to
 *  each collector (string) a collectordata structure
 */
typedef struct collectors_table_wrapper {
  khash_t(collectors_table_t) * table;
} collectors_table_wrapper_t;


/** Allocate memory for a strucure that maintains
 *  the information about a set of collectors.
 *
 * @return a pointer to the structure, or
 *  NULL if an error occurred
 */
collectors_table_wrapper_t *collectors_table_create();


/** The function considers the BGP information embedded into the
 *  bgpstream record structure, and it forward such information to
 *  the appropriate collector.
 * 
 * @param collectors_table a pointer to the collectors_table structure
 * @param bs_record a pointer to the bgpstream record under processing
 * @return 0 if the function ended correctly
 *        -1 if something went wrong during the function execution
 */
int collectors_table_process_record(collectors_table_wrapper_t *collectors_table,
				    bgpstream_record_t *bs_record);

#ifdef WITH_BGPWATCHER
/** The function prints the statistics of a set of collectors for the interval of time
 *  that starts at interval_start if the status is not NULL.
 * 
 * @param collectors_table a pointer to the collectors_table structure
 * @param interval_processing_start when we started processing the current interval
 * @param interval_start start of the interval in epoch time
 * @param interval_end end of the interval in epoch time
 * @param metric_pfx string that prepends every metric provided in output
 * @param bw_client a pointer to the bgpwatcher client used to send data to the bgpwatcher
 * @return 0 if the function ended correctly,
 *        -1 if something went wrong during the function execution
 */
int collectors_table_interval_end(collectors_table_wrapper_t *collectors_table,
				  int interval_processing_start,
				  int interval_start,
				  int interval_end,
				  char *metric_pfx,
				  bw_client_t *bw_client);
#else
/** The function prints the statistics of a set of collectors for the interval of time
 *  that starts at interval_start if the status is not NULL.
 * 
 * @param collectors_table a pointer to the collectors_table structure
 * @param interval_processing_start when we started processing the current interval
 * @param interval_start start of the interval in epoch time
 * @param interval_end end of the interval in epoch time
 * @param metric_pfx string that prepends every metric provided in output
 * @return 0 if the function ended correctly,
 *        -1 if something went wrong during the function execution
 */
int collectors_table_interval_end(collectors_table_wrapper_t *collectors_table,
				  int interval_processing_start,
				  int interval_start,
				  int interval_end,
				  char *metric_pfx);
#endif


/** Deallocate memory for the collectors-table's data.
 *
 * @param collectors_table a pointer to the collectors-table's data
 */
void collectors_table_destroy(collectors_table_wrapper_t *collectors_table);



#endif /* __BGPRIBS_COLLECTORS_TABLE_H */
