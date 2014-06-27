/*
 * corsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of corsaro.
 *
 * corsaro is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * corsaro is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with corsaro.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __CORSARO_FILTER_H
#define __CORSARO_FILTER_H

#include "corsaro_int.h"

/** @file
 *
 * @brief Header file dealing with the corsaro filter manager
 *
 * The filter manager is at the moment just a fancy way of dynamically
 * allocating the flags for the filter_matches bit array in corsaro_packet_t. A
 * plugin can ask to register a new filter (most likely one of the filter*
 * plugins, but really any plugin could do this), and then when it checks a
 * packet against this filter, it asks the filter manage to mark the packet as
 * matched. Other plugins can then look this filter up by name (and thenceforth
 * check if a packet matches using this manager), or perhaps even get a list of
 * all filters and dynamically do something with each filter).
 *
 * @author Alistair King
 *
 */

/** The maximum number of filters that we support (currently this is based on
    the number of bits in corsaro_packet_state_t filter_matches) */
#define CORSARO_FILTER_ID_MAX 64

/** Instance of a single filter */
typedef struct corsaro_filter
{
  /** Name of the filter */
  char *name;

  /** ID of the filter */
  uint8_t id;

  /** Pointer to the filter manager that owns this filter */
  struct corsaro_filter_manager *manager;

  /** User-provided void pointer */
  void *user;

} corsaro_filter_t;

/** State for the filter manager */
typedef struct corsaro_filter_manager
{
  /** Array of currently allocated filters */
  corsaro_filter_t **filters;

  /** Number of allocated filters */
  uint8_t filters_cnt;

} corsaro_filter_manager_t;

/** Create a filter manager instance and associate it with the given corsaro instance
 *
 * @param corsaro       corsaro instance to associate the filter manager with
 * @return pointer to the filter manager created, NULL if an error occurred.
 */
corsaro_filter_manager_t *corsaro_filter_manager_init(corsaro_t *corsaro);

/** Free the filter manager associated with a corsaro instance
 *
 * @param corsaro       corsaro instance to free the filter manager for
 */
void corsaro_filter_manager_free(corsaro_filter_manager_t *manager);

/** Create a new filter with the given name
 *
 * @param corsaro       corsaro instance to create a new filter for
 * @param name          name of the filter to create
 * @param user          void pointer for use by the filter owner
 *
 * @return a corsaro filter instance if the filter was allocated successfully,
 * NULL otherwise
 *
 * @note the name parameter must be unique within an instance of corsaro, the
 * initialization will fail if the name is already in use.
 */
corsaro_filter_t *corsaro_filter_init(corsaro_t *corsaro, const char *name,
				      void *user);

/** Free the given filter
 *
 * @param corsaro       pointer to the corsaro instance that the filter is
 *                      associated with
 * @param filter        pointer to the filter to free
 */
void corsaro_filter_free(corsaro_filter_t *filter);

/** Get the filter that matches the given name
 *
 * @param corsaro       pointer to the corsaro instance to get the filter from
 * @param name          name of the filter to retrieve
 * @return the filter that matches the name given, NULL if there were no matches
 *
 * @note this function searches a list of filters, so it should not be run on a
 * per-packet basis. i.e. keep a pointer to the filter that you are interested
 * in.
 */
corsaro_filter_t *corsaro_filter_get(corsaro_t *corsaro, const char *name);

/** Get the filter that matches the given name
 *
 * @param corsaro       pointer to the corsaro instance to get the filter from
 * @param name          name of the filter to retrieve
 * @return the filter that matches the name given, NULL if there were no matches
 *
 * @note this function searches a list of filters, so it should not be run on a
 * per-packet basis. i.e. keep a pointer to the filter that you are interested
 * in.
 */
int corsaro_filter_get_all(corsaro_t *corsaro, corsaro_filter_t ***filters);

/** Check if a packet matches the given filter
 *
 * @param packet        pointer to a corsaro packet to check the filter against
 * @param filter        pointer to a filter to check against the given packet
 * @return > 0 if the packet matches, 0 if not
 *
 * @note this function **does not** actually apply the filter, it simply checks
 * the result of a previous call to corsaro_filter_set_match.
 */
int corsaro_filter_is_match(corsaro_packet_state_t *state,
			    corsaro_filter_t *filter);

/** Check if a packet matches any current filter
 *
 * @param packet        pointer to a corsaro packet to check the filter against
 * @return > 0 if the packet matches any filter, 0 if not
 *
 * @note this function **does not** actually apply the filter, it simply checks
 * the results previous calls to corsaro_filter_set_match.
 */
int corsaro_filter_is_match_any(corsaro_packet_state_t *state);

/** Set whether a packet matches the given filter
 *
 * @param packet        pointer to a corsaro packet
 * @param filter        filter to update the match value for
 * @param match         0 indicates no match, any other value indicates a match
 *
 * @return 0 if the packet was successfully updated, -1 otherwise
 */
void corsaro_filter_set_match(corsaro_packet_state_t *state,
			      corsaro_filter_t *filter,
			      int match);


#endif /* __CORSARO_FILTER_H */
