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

#include "config.h"
#include "corsaro_int.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#include "corsaro_log.h"

#include "corsaro_tag.h"

/** ==== PUBLIC API FUNCTIONS BELOW HERE ==== */

/* ========== TAG MANAGER ========== */

corsaro_tag_manager_t *corsaro_tag_manager_init(corsaro_t *corsaro)
{
  corsaro_tag_manager_t *manager;

  if((manager = malloc_zero(sizeof(corsaro_tag_manager_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc tag manager");
      return NULL;
    }

  /* annnnd, we're done */
  return manager;
}

void corsaro_tag_manager_free(corsaro_tag_manager_t *manager)
{
  int i;

  assert(manager != NULL);

  /* free all the groups that we have allocated */
  if(manager->groups != NULL)
    {
      for(i=0; i<manager->groups_cnt; i++)
	{
	  corsaro_tag_group_free(manager->groups[i]);
	  manager->groups[i] = NULL;
	}
      free(manager->groups);
      manager->groups = NULL;
      manager->groups_cnt = 0;
    }

  /* free all the tags that we have allocated */
  if(manager->tags != NULL)
    {
      for(i=0; i<manager->tags_cnt; i++)
	{
	  corsaro_tag_free(manager->tags[i]);
	  manager->tags[i] = NULL;
	}
      free(manager->tags);
      manager->tags = NULL;
      manager->tags_cnt = 0;
    }

  free(manager);

  return;
}


/* ========== TAG STATE ========== */

void corsaro_tag_state_reset(corsaro_packet_state_t *state)
{
  int i;
  /* reset each matched tag */
  for(i=0; i<state->tags.tag_matches_cnt; i++)
    {
      state->tags.tag_matches[i] = 0;
    }
  /* reset the number of matched tag */
  state->tags.tag_matches_set_cnt = 0;
}

void corsaro_tag_state_free(corsaro_packet_state_t *state)
{
  assert(state->tags.tag_matches != NULL);

  free(state->tags.tag_matches);
  state->tags.tag_matches = NULL;
  state->tags.tag_matches_cnt = 0;
}

/* ========== TAGS ========== */

corsaro_tag_t *corsaro_tag_init(corsaro_t *corsaro, const char *name,
				void *user)
{
  assert(corsaro != NULL);

  corsaro_tag_t *tag;
  corsaro_tag_manager_t *manager = corsaro->tag_manager;

  assert(manager != NULL);

  /* now check that a tag with this name does not already exist */
  if((tag = corsaro_tag_get(corsaro, name)) != NULL)
    {
      return tag;
    }

  if((tag = malloc(sizeof(corsaro_tag_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc tag");
      return NULL;
    }

  /* get the next available tag id (starting from 0)*/
  tag->id = manager->tags_cnt++;

  /* save the name */
  tag->name = strdup(name);

  /* save us */
  tag->manager = manager;

  /* save them */
  tag->user = user;

  /* resize the array of tags to hold this one */
  if((manager->tags = realloc(manager->tags, sizeof(corsaro_tag_t*) *
				 manager->tags_cnt)) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc tag array");
      corsaro_tag_free(tag);
      return NULL;
    }

  manager->tags[tag->id] = tag;

  /* resize the array of matched tags to hold this one */
  if((corsaro->packet->state.tags.tag_matches =
      realloc(corsaro->packet->state.tags.tag_matches,
	      sizeof(uint8_t) * manager->tags_cnt)) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc tag result array");
      return NULL;
    }
  corsaro->packet->state.tags.tag_matches[tag->id] = 0;
  corsaro->packet->state.tags.tag_matches_cnt = manager->tags_cnt;

  return tag;
}

corsaro_tag_t *corsaro_tag_get(corsaro_t *corsaro, const char *name)
{
  assert(corsaro != NULL);
  assert(corsaro->tag_manager != NULL);
  corsaro_tag_manager_t *manager = corsaro->tag_manager;

  int i;

  for(i=0; i<manager->tags_cnt; i++)
    {
      if(manager->tags[i] != NULL &&
	 manager->tags[i]->name != NULL &&
	 (strcmp(manager->tags[i]->name, name) == 0))
	{
	  return manager->tags[i];
	}
    }

  return NULL;
}

int corsaro_tag_get_all(corsaro_t *corsaro, corsaro_tag_t ***tags)
{
  assert(corsaro != NULL);
  assert(corsaro->tag_manager != NULL);
  *tags = corsaro->tag_manager->tags;
  return corsaro->tag_manager->tags_cnt;
}

void corsaro_tag_free(corsaro_tag_t *tag)
{
  /* we will be nice and let people free tags that they created */
  if(tag == NULL)
    {
      return;
    }

  assert(tag->manager != NULL);

  tag->manager->tags[tag->id] = NULL;

  if(tag->name != NULL)
    {
      free(tag->name);
      tag->name = NULL;
    }

  /* we do not own 'manager' */

  /* we do not own 'user' */

  free(tag);
}

int corsaro_tag_is_match(corsaro_packet_state_t *state,
			    corsaro_tag_t *tag)
{
  assert(state != NULL);
  assert(tag != NULL);
  assert(tag->id <= tag->manager->tags_cnt);

  return state->tags.tag_matches[tag->id];
}

int corsaro_tag_is_match_any(corsaro_packet_state_t *state)
{
  return state->tags.tag_matches_set_cnt;
}

void corsaro_tag_set_match(corsaro_packet_state_t *state,
			      corsaro_tag_t *tag,
			      int match)
{
  assert(state != NULL);
  assert(tag != NULL);
  assert(tag->id < tag->manager->tags_cnt);

#if 0
  fprintf(stderr, "setting match for %s:%d to %d (used to be %d)\n",
	  tag->name, tag->id, match, state->tag_matches[tag->id]);
#endif

  if(match != 0)
    {
      state->tags.tag_matches_set_cnt++;
    }

  state->tags.tag_matches[tag->id] = match;
  return;
}

/* ========== TAG GROUPS ========== */

corsaro_tag_group_t *corsaro_tag_group_init(corsaro_t *corsaro,
					    const char *name,
					    corsaro_tag_group_match_mode_t mode,
					    void *user)
{
  assert(corsaro != NULL);

  corsaro_tag_group_t *group;
  corsaro_tag_manager_t *manager = corsaro->tag_manager;

  assert(manager != NULL);

  /* now check that a group with this name does not already exist */
  if((group = corsaro_tag_group_get(corsaro, name)) != NULL)
    {
      return group;
    }

  if((group = malloc(sizeof(corsaro_tag_group_t))) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc group");
      return NULL;
    }

  /* get the next available group id (starting from 0)*/
  group->id = manager->groups_cnt++;

  /* save the name */
  group->name = strdup(name);

  /* save us */
  group->manager = manager;

  /* save them */
  group->user = user;

  /* resize the array of tags to hold this one */
  if((manager->groups = realloc(manager->groups, sizeof(corsaro_tag_group_t*) *
				manager->groups_cnt)) == NULL)
    {
      corsaro_log(__func__, corsaro, "failed to malloc group array");
      corsaro_tag_group_free(group);
      return NULL;
    }

  manager->groups[group->id] = group;

  return group;
}

void corsaro_tag_group_free(corsaro_tag_group_t *group)
{
  /* we will be nice and let people free groups that they created */
  if(group == NULL)
    {
      return;
    }

  assert(group->manager != NULL);

  group->manager->groups[group->id] = NULL;

  if(group->name != NULL)
    {
      free(group->name);
      group->name = NULL;
    }

  /* we do not own 'manager' */

  /* we do not own 'user' */

  free(group);
}

corsaro_tag_group_t *corsaro_tag_group_get(corsaro_t *corsaro, const char *name)
{
  assert(corsaro != NULL);
  assert(corsaro->tag_manager != NULL);
  corsaro_tag_manager_t *manager = corsaro->tag_manager;

  int i;

  for(i=0; i<manager->groups_cnt; i++)
    {
      if(manager->groups[i] != NULL &&
	 manager->groups[i]->name != NULL &&
	 (strcmp(manager->groups[i]->name, name) == 0))
	{
	  return manager->groups[i];
	}
    }

  return NULL;
}

int corsaro_tag_group_get_all(corsaro_t *corsaro, corsaro_tag_group_t ***groups)
{
  assert(corsaro != NULL);
  assert(corsaro->tag_manager != NULL);
  *groups = corsaro->tag_manager->groups;
  return corsaro->tag_manager->groups_cnt;
}

int corsaro_tag_group_add_tag(corsaro_tag_group_t *group,
			      corsaro_tag_t *tag)
{
  assert(group != NULL);
  assert(tag != NULL);

  if((group->tags = realloc(group->tags, sizeof(corsaro_tag_t*) *
			    (group->tags_cnt+1))) == NULL)
    {
      return -1;
    }

  group->tags[group->tags_cnt++] = tag;

  tag->group = group;
  return 0;
}

int corsaro_tag_group_get_tags(corsaro_tag_group_t *group,
			       corsaro_tag_t ***tags)
{
  assert(group != NULL);
  *tags = group->tags;
  return group->tags_cnt;
}

int corsaro_tag_group_is_match(corsaro_packet_state_t *state,
			       corsaro_tag_group_t *group)
{
  int i;
  int matches = 0;
  assert(state != NULL);
  assert(group != NULL);
  assert(group->id <= group->manager->groups_cnt);

  for(i=0; i<group->tags_cnt; i++)
    {
      if(corsaro_tag_is_match(state, group->tags[i]) > 0)
	{
	  matches++;
	}
    }

  switch(group->mode)
    {
    case CORSARO_TAG_GROUP_MATCH_MODE_ANY:
      return matches;
      break;

    case CORSARO_TAG_GROUP_MATCH_MODE_ALL:
      return matches == group->tags_cnt;
      break;

    default:
      return -1;
      break;
    }

  return -1;
}
