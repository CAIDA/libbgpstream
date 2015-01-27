/*
 * bgpwatcher
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of bgpwatcher.
 *
 * bgpwatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bgpwatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bgpwatcher.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef __BGPWATCHER_CONSUMER_MANAGER_H
#define __BGPWATCHER_CONSUMER_MANAGER_H

#include <stdlib.h>
#include <stdint.h>
#include <timeseries.h>

#include "bgpwatcher_view.h"


/** @file
 *
 * @brief Header file that exposes the public interface of the bgpwatcher
 * consumer manager
 *
 * @author Alistair King
 *
 */

/**
 * @name Public Opaque Data Structures
 *
 * @{ */

/** Opaque struct holding state for the bgpwatcher consumer manager */
typedef struct bw_consumer_manager bw_consumer_manager_t;

/** Opaque struct holding state for a bgpwatcher consumer */
typedef struct bwc bwc_t;

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

/** A unique identifier for each bgpwatcher consumer that bgpwatcher supports
 */
typedef enum bwc_id
  {
    /** Dumps debugging information about the views to stdout */
    BWC_ID_TEST               = 1,

    BWC_ID_PERFMONITOR        = 2,
    
    /** Writes information about per-AS visibility information to Charthouse */
    BWC_ID_PERASVISIBILITY    = 3,

    /** Writes information about per-Geo visibility information to Charthouse */
    BWC_ID_PERGEOVISIBILITY   = 4,

    /** @todo add more consumers here */

    /** Lowest numbered bgpwatcher consumer ID */
    BWC_ID_FIRST      = BWC_ID_TEST,
    /** Highest numbered bgpwatcher consumer ID */
    BWC_ID_LAST       = BWC_ID_PERGEOVISIBILITY,

  } bwc_id_t;

/** @} */

/** Create a new consumer manager instance
 *
 * @param timeseries    pointer to an initialized timeseries instance
 *
 * @return the consumer manager instance created, NULL if an error occurs
 */
bw_consumer_manager_t *bw_consumer_manager_create(timeseries_t *timeseries);

/** Free a consumer manager instance
 *
 * @param               Double-pointer to consumer manager instance to free
 */
void bw_consumer_manager_destroy(bw_consumer_manager_t **mgr_p);

/** Enable the given consumer unless it is already enabled
 *
 * @param consumer      Pointer to the consumer to be enabled
 * @param options       A string of options to configure the consumer
 * @return 0 if the consumer was initialized, -1 if an error occurred
 *
 * Once bw_consumer_manager_create is called,
 * bw_consumer_manager_enable_consumer should be called once for each
 * consumer that is to be used.
 *
 * To obtain a pointer to a consumer, use the
 * bw_consumer_manager_get_consumer_by_name or
 * bw_consumer_manager_get_consumer_by_id functions. To enumerate a list
 * of available consumers, the bw_consumer_manager_get_all_consumers
 * function can be used to get a list of all consumers and then bwc_get_name can
 * be used on each to get their name.
 *
 * If configuring a plugin from command line arguments, the helper function
 * bw_consumer_manager_enable_consumer_from_str can be used which takes a
 * single string where the first token (space-separated) is the name of the
 * consumer, and the remainder of the string is taken to be the options.
 */
int bw_consumer_manager_enable_consumer(bwc_t *consumer,
					const char *options);

/** Attempt to enable a consumer based on the given command string
 *
 * @param mgr           The manager object to enable the consumer for
 * @param cmd           The command string to parse for consumer name and options
 * @return an enabled consumer if successful, NULL otherwise
 *
 * The `cmd` string is separated at the first space. The first token is taken to
 * be the consumer name, and the remainder is taken to be the options. For
 * example, the command: `test -a all` will attempt to enable the `test`
 * consumer and will pass `-a all` as options.
 */
bwc_t *bw_consumer_manager_enable_consumer_from_str(bw_consumer_manager_t *mgr,
						    const char *cmd);

/** Retrieve the consumer object for the given consumer ID
 *
 * @param mgr           The manager object to retrieve the consumer object from
 * @param id            The ID of the consumer to retrieve
 * @return the consumer object for the given ID, NULL if there are no matches
 */
bwc_t *bw_consumer_manager_get_consumer_by_id(bw_consumer_manager_t *mgr,
					      bwc_id_t id);


/** Retrieve the consumer object for the given consumer name
 *
 * @param mgr           Manager object to retrieve the consumer from
 * @param name          The consumer name to retrieve
 * @return the consumer object for the given name, NULL if there are no matches
 */
bwc_t *bw_consumer_manager_get_consumer_by_name(bw_consumer_manager_t *mgr,
						const char *name);

/** Get an array of available consumers
 *
 * @param mgr           The manager object to get all the consumers for
 * @return an array of consumer objects
 *
 * @note the number of elements in the array will be exactly BWC_ID_LAST.
 *
 * @note not all consumers in the list may be present (i.e. there may be NULL
 * pointers), or some may not be enabled. use bwc_is_enabled to check.
 */
bwc_t **bw_consumer_manager_get_all_consumers(bw_consumer_manager_t *mgr);

/** Process the given view using each enabled consumer
 *
 * @param mgr           The manager object
 * @param interests     Bit-array of bgpwatcher_consumer_interest_t flags
 *                        indicating which interests the given view satisfies
 * @param view          Borrowed reference to the BGP Watcher View to process
 * @param return 0 if the view was processed successfully, -1 otherwise
 */
int bw_consumer_manager_process_view(bw_consumer_manager_t *mgr,
				     uint8_t interests,
				     bgpwatcher_view_t *view);

/** Check if the given consumer is enabled already
 *
 * @param consumer       The consumer to check the status of
 * @return 1 if the consumer is enabled, 0 otherwise
 */
int bwc_is_enabled(bwc_t *consumer);

/** Get the ID for the given consumer
 *
 * @param consumer      The consumer object to retrieve the ID from
 * @return the ID of the given consumer
 */
bwc_id_t bwc_get_id(bwc_t *consumer);

/** Get the consumer name for the given ID
 *
 * @param id            The consumer ID to retrieve the name for
 * @return the name of the consumer, NULL if an invalid consumer was provided
 */
const char *bwc_get_name(bwc_t *consumer);

#endif /* __BGPWATCHER_CONSUMER_H */
