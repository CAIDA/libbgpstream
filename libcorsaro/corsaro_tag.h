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

#ifndef __CORSARO_TAG_H
#define __CORSARO_TAG_H

#include "corsaro_int.h"

/** @file
 *
 * @brief Header file dealing with the corsaro tag manager
 *
 * A plugin can ask to register a new tag (most likely one of the filter*
 * plugins, but really any plugin could do this), and then when it checks a
 * packet against this tag, it asks the tag manager to mark the packet as
 * matched. Other plugins can then look this tag up by name (and thenceforth
 * check if a packet matches using this manager), or perhaps even get a list of
 * all tags and dynamically do something with each tag).
 *
 * @author Alistair King
 *
 */

/** Instance of a single tag */
typedef struct corsaro_tag
{
  /** Name of the tag */
  char *name;

  /** ID of the tag */
  uint8_t id;

  /** Pointer to the tag manager that owns this tag */
  struct corsaro_tag_manager *manager;

  /** User-provided void pointer */
  void *user;

} corsaro_tag_t;

/** State for the tag manager */
typedef struct corsaro_tag_manager
{
  /** Array of currently allocated tags */
  corsaro_tag_t **tags;

  /** Number of allocated tags */
  uint8_t tags_cnt;

} corsaro_tag_manager_t;

/** Create a tag manager instance and associate it with the given corsaro instance
 *
 * @param corsaro       corsaro instance to associate the tag manager with
 * @return pointer to the tag manager created, NULL if an error occurred.
 */
corsaro_tag_manager_t *corsaro_tag_manager_init(corsaro_t *corsaro);

/** Free the tag manager associated with a corsaro instance
 *
 * @param corsaro       corsaro instance to free the tag manager for
 */
void corsaro_tag_manager_free(corsaro_tag_manager_t *manager);

/** Create a new tag with the given name
 *
 * @param corsaro       corsaro instance to create a new tag for
 * @param name          name of the tag to create
 * @param user          void pointer for use by the tag owner
 *
 * @return a corsaro tag instance if the tag was allocated successfully,
 * NULL otherwise
 *
 * @note the name parameter must be unique within an instance of corsaro, the
 * initialization will fail if the name is already in use.
 */
corsaro_tag_t *corsaro_tag_init(corsaro_t *corsaro, const char *name,
				      void *user);

/** Free the given tag
 *
 * @param corsaro       pointer to the corsaro instance that the tag is
 *                      associated with
 * @param tag        pointer to the tag to free
 */
void corsaro_tag_free(corsaro_tag_t *tag);

/** Get the tag that matches the given name
 *
 * @param corsaro       pointer to the corsaro instance to get the tag from
 * @param name          name of the tag to retrieve
 * @return the tag that matches the name given, NULL if there were no matches
 *
 * @note this function searches a list of tags, so it should not be run on a
 * per-packet basis. i.e. keep a pointer to the tag that you are interested
 * in.
 */
corsaro_tag_t *corsaro_tag_get(corsaro_t *corsaro, const char *name);

/** Get the tag that matches the given name
 *
 * @param corsaro       pointer to the corsaro instance to get the tag from
 * @param name          name of the tag to retrieve
 * @return the tag that matches the name given, NULL if there were no matches
 */
int corsaro_tag_get_all(corsaro_t *corsaro, corsaro_tag_t ***tags);

/** Check if a packet matches the given tag
 *
 * @param packet        pointer to a corsaro packet to check the tag against
 * @param tag        pointer to a tag to check against the given packet
 * @return > 0 if the packet matches, 0 if not
 *
 * @note this function **does not** actually apply the tag, it simply checks
 * the result of a previous call to corsaro_tag_set_match.
 */
int corsaro_tag_is_match(corsaro_packet_state_t *state,
			    corsaro_tag_t *tag);

/** Check if a packet matches any current tag
 *
 * @param packet        pointer to a corsaro packet to check the tag against
 * @return > 0 if the packet matches any tag, 0 if not
 *
 * @note this function **does not** actually apply the tag, it simply checks
 * the results previous calls to corsaro_tag_set_match.
 */
int corsaro_tag_is_match_any(corsaro_packet_state_t *state);

/** Set whether a packet matches the given tag
 *
 * @param packet        pointer to a corsaro packet
 * @param tag        tag to update the match value for
 * @param match         0 indicates no match, any other value indicates a match
 *
 * @return 0 if the packet was successfully updated, -1 otherwise
 */
void corsaro_tag_set_match(corsaro_packet_state_t *state,
			   corsaro_tag_t *tag,
			   int match);


#endif /* __CORSARO_TAG_H */
