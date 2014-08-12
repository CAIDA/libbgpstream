/*
 * bgpcorsaro
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
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

#ifndef __BGPCORSARO_TAG_H
#define __BGPCORSARO_TAG_H

#include "bgpcorsaro_int.h"

/** @file
 *
 * @brief Header file dealing with the bgpcorsaro tag manager
 *
 * A plugin can ask to register a new tag (most likely one of the filter*
 * plugins, but really any plugin could do this), and then when it checks a
 * record against this tag, it asks the tag manager to mark the record as
 * matched. Other plugins can then look this tag up by name (and thenceforth
 * check if a record matches using this manager), or perhaps even get a list of
 * all tags and dynamically do something with each tag).
 *
 * @author Alistair King
 *
 */

/** Modes for determining if a record matches a group of tags */
typedef enum bgpcorsaro_tag_group_match_mode
  {
    /** A record matches this group if ANY of the tags match */
    BGPCORSARO_TAG_GROUP_MATCH_MODE_ANY = 0,

    /** A record matches this group if ALL of the tags match */
    BGPCORSARO_TAG_GROUP_MATCH_MODE_ALL = 1,

    /** Default matching mode for a group (ANY tag matches) */
    BGPCORSARO_TAG_GROUP_MATCH_MODE_DEFAULT = BGPCORSARO_TAG_GROUP_MATCH_MODE_ANY,
  } bgpcorsaro_tag_group_match_mode_t;

/** Instance of a single tag */
typedef struct bgpcorsaro_tag
{
  /** Name of the tag */
  char *name;

  /** ID of the tag */
  uint8_t id;

  /** Pointer to the tag manager that owns this tag */
  struct bgpcorsaro_tag_manager *manager;

  /** Number of groups that this tag belongs to */
  int groups_cnt;

  /** User-provided void pointer */
  void *user;

} bgpcorsaro_tag_t;

/** Instance of a tag group */
typedef struct bgpcorsaro_tag_group
{
  /** Name of the group */
  char *name;

  /** ID of the group */
  uint8_t id;

  /** Array of tags in this group */
  bgpcorsaro_tag_t **tags;

  /** Number of tags in this group */
  int tags_cnt;

  /** Mode for determining if a record matches this group */
  bgpcorsaro_tag_group_match_mode_t mode;

  /** Pointer to the tag manager that owns this group */
  struct bgpcorsaro_tag_manager *manager;

  /** User provided pointer */
  void *user;

} bgpcorsaro_tag_group_t;

/** State for the tag manager */
typedef struct bgpcorsaro_tag_manager
{
  /** Array of currently allocated tags */
  bgpcorsaro_tag_t **tags;

  /** Number of allocated tags */
  int tags_cnt;

  /** Array of currently allocated tag groups */
  bgpcorsaro_tag_group_t **groups;

  /** Number of allocated groups */
  int groups_cnt;

} bgpcorsaro_tag_manager_t;

/** State information for a specific record */
typedef struct bgpcorsaro_tag_state
{
  /** Array of boolean values indicating which tags have been matched by this
      record.  bgpcorsaro_tag is responsible for dynamically allocating tag IDs
      based on requests by plugins */
  uint8_t *tag_matches;

  /** Total number of tags in the tag_matches array (this is always the same as
      the total number of tags allocated) */
  int tag_matches_cnt;

  /** Number of tags that are set to matching for the current record.
      Provides an efficient way to check if *any* tag matches the current
      record */
  int tag_matches_set_cnt;
} bgpcorsaro_tag_state_t;

/** Create a tag manager instance and associate it with the given bgpcorsaro instance
 *
 * @param bgpcorsaro       bgpcorsaro instance to associate the tag manager with
 * @return pointer to the tag manager created, NULL if an error occurred.
 */
bgpcorsaro_tag_manager_t *bgpcorsaro_tag_manager_init(bgpcorsaro_t *bgpcorsaro);

/** Free the tag manager associated with a bgpcorsaro instance
 *
 * @param bgpcorsaro       bgpcorsaro instance to free the tag manager for
 */
void bgpcorsaro_tag_manager_free(bgpcorsaro_tag_manager_t *manager);

/** Reset the tag state in the given record state
 *
 * @param state         pointer to a record state instance to reset
 */
void bgpcorsaro_tag_state_reset(bgpcorsaro_record_state_t *state);

/** Free the tag state in the given record state
 *
 * @param state         pointer to the record state instance to free
 */
void bgpcorsaro_tag_state_free(bgpcorsaro_record_state_t *state);

/** Create a new tag with the given name
 *
 * @param bgpcorsaro       bgpcorsaro instance to create a new tag for
 * @param name          name of the tag to create
 * @param user          void pointer for use by the tag owner
 *
 * @return a bgpcorsaro tag instance if the tag was allocated successfully,
 * NULL otherwise
 *
 * @note the name parameter must be unique within an instance of bgpcorsaro.  If
 * the name is not unique, the pre-existing tag with the same name will be
 * returned (and the user pointer will **not** be updated).
 */
bgpcorsaro_tag_t *bgpcorsaro_tag_init(bgpcorsaro_t *bgpcorsaro, const char *name,
				      void *user);

/** Free the given tag
 *
 * @param bgpcorsaro       pointer to the bgpcorsaro instance that the tag is
 *                      associated with
 * @param tag        pointer to the tag to free
 */
void bgpcorsaro_tag_free(bgpcorsaro_tag_t *tag);

/** Get the tag that matches the given name
 *
 * @param bgpcorsaro       pointer to the bgpcorsaro instance to get the tag from
 * @param name          name of the tag to retrieve
 * @return the tag that matches the name given, NULL if there were no matches
 *
 * @note this function searches a list of tags, so it should not be run on a
 * per-record basis. i.e. keep a pointer to the tag that you are interested
 * in.
 */
bgpcorsaro_tag_t *bgpcorsaro_tag_get(bgpcorsaro_t *bgpcorsaro, const char *name);

/** Get the tag that matches the given name
 *
 * @param bgpcorsaro       pointer to the bgpcorsaro instance to get the tag from
 * @param[out] tags     filled with a pointer to an array of tags
 * @return the number of tags in the returned array
 */
int bgpcorsaro_tag_get_all(bgpcorsaro_t *bgpcorsaro, bgpcorsaro_tag_t ***tags);

/** Check if a record matches the given tag
 *
 * @param record        pointer to a bgpcorsaro record to check the tag against
 * @param tag        pointer to a tag to check against the given record
 * @return > 0 if the record matches, 0 if not
 *
 * @note this function **does not** actually apply the tag, it simply checks
 * the result of a previous call to bgpcorsaro_tag_set_match.
 */
int bgpcorsaro_tag_is_match(bgpcorsaro_record_state_t *state,
			 bgpcorsaro_tag_t *tag);

/** Check if a record matches any current tag
 *
 * @param record        pointer to a bgpcorsaro record to check the tag against
 * @return > 0 if the record matches any tag, 0 if not
 *
 * @note this function **does not** actually apply the tag, it simply checks
 * the results previous calls to bgpcorsaro_tag_set_match.
 */
int bgpcorsaro_tag_is_match_any(bgpcorsaro_record_state_t *state);

/** Set whether a record matches the given tag
 *
 * @param record        pointer to a bgpcorsaro record
 * @param tag        tag to update the match value for
 * @param match         0 indicates no match, any other value indicates a match
 *
 * @return 0 if the record was successfully updated, -1 otherwise
 */
void bgpcorsaro_tag_set_match(bgpcorsaro_record_state_t *state,
			   bgpcorsaro_tag_t *tag,
			   int match);

/** Create a new tag group with the given name
 *
 * @param bgpcorsaro       bgpcorsaro instance to create a new tag for
 * @param name          name of the tag to create
 * @param user          void pointer for use by the tag owner
 *
 * @return a bgpcorsaro tag group instance if the group was allocated successfully,
 * NULL otherwise
 *
 * @note the name parameter must be unique within an instance of bgpcorsaro. If the
 * name is not unique, the pre-existing group with the same name will be
 * returned (and the user pointer will **not** be updated).
 */
bgpcorsaro_tag_group_t *bgpcorsaro_tag_group_init(bgpcorsaro_t *bgpcorsaro,
					    const char *name,
					    bgpcorsaro_tag_group_match_mode_t mode,
					    void *user);

/** Free the given tag group
 *
 * @param bgpcorsaro       pointer to the bgpcorsaro instance that the group is
 *                      to be associated with
 * @param group         pointer to the group to free
 */
void bgpcorsaro_tag_group_free(bgpcorsaro_tag_group_t *group);

/** Get the tag group that matches the given name
 *
 * @param bgpcorsaro       pointer to the bgpcorsaro instance to get the group from
 * @param name          name of the group to retrieve
 * @return the group that matches the name given, NULL if there were no matches
 *
 * @note this function searches a list of groups, so it should not be run on a
 * per-record basis. i.e. keep a pointer to the group that you are interested in.
 */
bgpcorsaro_tag_group_t *bgpcorsaro_tag_group_get(bgpcorsaro_t *bgpcorsaro, const char *name);

/** Get the tag that matches the given name
 *
 * @param bgpcorsaro       pointer to the bgpcorsaro instance to get the group from
 * @param[out] groups   filled with a pointer to an array of tag groups
 * @return the number of groups in the returned array
 */
int bgpcorsaro_tag_group_get_all(bgpcorsaro_t *bgpcorsaro, bgpcorsaro_tag_group_t ***groups);

/** Add a tag to a group
 *
 * @param group         pointer to the group to add the tag to
 * @param tag           pointer to the tag to add
 * @return 0 if the tag was added successfully, -1 otherwise
 */
int bgpcorsaro_tag_group_add_tag(bgpcorsaro_tag_group_t *group,
			      bgpcorsaro_tag_t *tag);

/** Get the tags that are part of the given group
 *
 * @param group         pointer to the group to retrieve the tags from
 * @param[out] tags     filled with a pointer to an array of tags
 * @return the number of tags in the returned array
 */
int bgpcorsaro_tag_group_get_tags(bgpcorsaro_tag_group_t *group,
			       bgpcorsaro_tag_t ***tags);

/** Check if a record matches the given tag group
 *
 * @param record        pointer to a bgpcorsaro record to check the group against
 * @param group         pointer to a tag group to check against the given record
 * @return > 0 if the record matches, 0 if not
 *
 * @note this function **does not** actually apply the tags, it simply checks
 * the result of previous calls to bgpcorsaro_tag_set_match for tags within the
 * group. The result is dependent on the match mode of the group.
 */
int bgpcorsaro_tag_group_is_match(bgpcorsaro_record_state_t *state,
			       bgpcorsaro_tag_group_t *group);


#endif /* __BGPCORSARO_TAG_H */
